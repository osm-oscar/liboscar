#include <liboscar/KVStats.h>
#include <sserialize/mt/ThreadPool.h>


namespace liboscar {
namespace detail {
namespace KVStats {
	
SortedData::SortedData() {}

SortedData::SortedData(Data && other) :
keyValueCount(other.keyValueCount.begin(), other.keyValueCount.end()) 
{
	if (!std::is_same<Data::KeyValueCountMap, Data::KeyValueCountSortedMap>::value) {
		std::sort(keyValueCount.begin(), keyValueCount.end(), [](const KeyValueCount & a, const KeyValueCount & b) {
			return a.first < b.first;
		});
	}
	other.keyValueCount.clear();
}

SortedData::SortedData(SortedData && other) :
keyValueCount(std::move(other.keyValueCount))
{}

SortedData & SortedData::operator=(SortedData && other) {
	keyValueCount = std::move(other.keyValueCount);
	return *this;
}

SortedData SortedData::merge(SortedData && first, SortedData && second) {
	auto fit(first.keyValueCount.begin()), fend(first.keyValueCount.end());
	auto sit(second.keyValueCount.begin()), send(second.keyValueCount.end());
	SortedData result;
	result.keyValueCount.reserve(std::max(first.keyValueCount.size(), second.keyValueCount.size()));
	for(; fit != fend && sit != send;) {
		if (fit->first < sit->first) {
			result.keyValueCount.emplace_back(*fit);
			++fit;
		}
		else if (sit->first < fit->first) {
			result.keyValueCount.emplace_back(*sit);
			++sit;
		}
		else {
			result.keyValueCount.emplace_back(fit->first, fit->second + sit->second);
			++fit;
			++sit;
		}
	}
	result.keyValueCount.insert(result.keyValueCount.end(), fit, fend);
	result.keyValueCount.insert(result.keyValueCount.end(), sit, send);
	first.keyValueCount.clear();
	second.keyValueCount.clear();
	return result;
}

Data::Data() {}

Data::Data(Data && other) : keyValueCount(std::move(other.keyValueCount)) {}

Data & Data::operator=(Data && other) {
	keyValueCount = std::move(other.keyValueCount);
	return *this;
}

void Data::update(const Static::OsmKeyValueObjectStore::KVItemBase & item) {
	for (uint32_t i(0), s(item.size()); i < s; ++i) {
		uint32_t key = item.keyId(i);
		uint32_t value = item.valueId(i);
		++(keyValueCount[std::make_pair(key, value)]); //init to 0 if not existent
	}
}

Data Data::merge(Data && first, Data && second) {
	if (first.keyValueCount.size() < second.keyValueCount.size()) {
		return merge(std::move(second), std::move(first));
	}
	if (!second.keyValueCount.size()) {
		return std::move(first);
	}
	
	for(const auto & x : second.keyValueCount) {
		first.keyValueCount[x.first] += x.second; 
	}
	second.keyValueCount.clear();
	return std::move(first);
}

KeyInfo::KeyInfo() : values(sserialize::CFLArray<std::vector<ValueInfo>>::DeferContainerAssignment{}) {}

KeyInfo::KeyInfo(uint32_t keyId, std::vector<ValueInfo> * valuesContainer, uint64_t offset, uint32_t size) :
keyId(keyId),
values(valuesContainer, offset, size)
{}

State::State(const Static::OsmKeyValueObjectStore & store, const sserialize::ItemIndex & items) :
store(store),
items(items)
{}


Worker::Worker(State * state) : state(state) {}

Worker::Worker(const Worker & other) : state(other.state) {}

void Worker::operator()() {
	std::size_t size = state->items.size();
	while (true) {
		std::size_t p = state->pos.fetch_add(BlockSize, std::memory_order_relaxed);
		if (p >= state->items.size()) {
			break;
		}
		for(std::size_t i(0); i < BlockSize && p < size; ++i, ++p) {
			uint32_t itemId = state->items.at(p);
			d.update( state->store.kvBaseItem(itemId) );
		}
		if (d.keyValueCount.size() > FlushSize) {
			flush();
		}
	}
	flush();
}

void Worker::flush() {
	State::DataType sd(std::move(d));
	std::unique_lock<std::mutex> lck(state->lock, std::defer_lock);
	while(true) {
		lck.lock();
		state->d.emplace_back(std::move(sd));
		if (state->d.size() < 2) {
			break;
		}
		auto first = std::move(state->d.back());
		state->d.pop_back();
		auto second = std::move(state->d.back());
		state->d.pop_back();
		lck.unlock();
		sd = State::DataType::merge(std::move(first), std::move(second));
	}
}


Stats::Stats(std::unique_ptr<std::vector<ValueInfo>> && valueInfoStore, std::vector<KeyInfo> && keyInfoStore, std::unordered_map<uint32_t, KeyInfoPtr> && keyInfo) :
m_valueInfoStore(std::move(valueInfoStore)),
m_keyInfoStore(std::move(keyInfoStore)),
m_keyInfo(std::move(keyInfo))
{}

Stats::Stats(Stats && other) :
m_valueInfoStore(std::move(other.m_valueInfoStore)),
m_keyInfoStore(std::move(other.m_keyInfoStore)),
m_keyInfo(std::move(other.m_keyInfo))
{}

Stats & Stats::operator=(Stats && other) {
	m_valueInfoStore = std::move(other.m_valueInfoStore);
	m_keyInfoStore = std::move(other.m_keyInfoStore);
	m_keyInfo = std::move(other.m_keyInfo);
	return *this;
}


Stats::KeyInfo & Stats::key(uint32_t keyId) {
	return m_keyInfoStore.at( m_keyInfo.at(keyId).offset );
}

const Stats::KeyInfo & Stats::key(uint32_t keyId) const {
	return m_keyInfoStore.at( m_keyInfo.at(keyId).offset );
}

const std::vector<KeyInfo> & Stats::keys() const {
	return m_keyInfoStore;
}

std::vector<KeyInfo> & Stats::keys() {
	return m_keyInfoStore;
}

Stats::KeyInfoIterator Stats::keysBegin() {
	return m_keyInfoStore.begin();
}

Stats::KeyInfoConstIterator Stats::keysBegin() const {
	return m_keyInfoStore.cbegin();
}

Stats::KeyInfoIterator Stats::keysEnd() {
	return m_keyInfoStore.end();
}

Stats::KeyInfoConstIterator Stats::keysEnd() const {
	return m_keyInfoStore.cend();
}

}} //end namespace detail::KVStats


KVStats::KVStats(const Static::OsmKeyValueObjectStore & store) :
m_store(store)
{}

KVStats::Stats KVStats::stats(const sserialize::ItemIndex & items, uint32_t threadCount) {
	if (items.type() & sserialize::ItemIndex::RANDOM_ACCESS_NO) {
		return stats( sserialize::ItemIndex( items.toVector() ), threadCount);
	}
	
	///we can process about 1k items per ms per thread, starting a thread costs less than 1 ms
	threadCount = std::min<uint32_t>(threadCount, std::max<uint32_t>(1, items.size()/1000));
	
	detail::KVStats::State state(m_store, items);
	
	sserialize::ThreadPool::execute(detail::KVStats::Worker(&state), threadCount, sserialize::ThreadPool::CopyTaskTag());
	
	return stats(std::move(state.d.front()));
}

KVStats::Stats KVStats::stats(detail::KVStats::Data && data) {
	//calculate KeyInfo
	auto & keyValueCount = data.keyValueCount;
	auto valueInfoStore = std::make_unique<std::vector<detail::KVStats::ValueInfo> >(keyValueCount.size());
	std::vector<detail::KVStats::KeyInfo> keyInfoStore;
	std::unordered_map<uint32_t, detail::KVStats::KeyInfoPtr> keyInfo;
	{
		//get the keys and the number of values per key
		for(const auto & x : keyValueCount) {
			const std::pair<uint32_t, uint32_t> & kv = x.first;
			const uint32_t & kvcount = x.second;
			detail::KVStats::KeyInfoPtr & kiptr = keyInfo[kv.first];
			if (!kiptr.valid()) {
				kiptr.offset = keyInfoStore.size();
				keyInfoStore.resize(keyInfoStore.size()+1);
			}
			detail::KVStats::KeyInfo & ki = keyInfoStore[kiptr.offset];
			ki.count += kvcount;
			ki.values.resize(ki.values.size()+1);
		}
		//init store
		uint64_t offset = 0;
		for(auto & x : keyInfo) {
			detail::KVStats::KeyInfo & ki = keyInfoStore[x.second.offset];
			ki.keyId = x.first;
			ki.values.reposition(offset);
			ki.values.rebind(valueInfoStore.get());
			offset += ki.values.size();
			ki.values.resize(0);
		}
		//copy the values
		for(const auto & x : keyValueCount) {
			const std::pair<uint32_t, uint32_t> & kv = x.first;
			detail::KVStats::KeyInfo & ki = keyInfoStore[ keyInfo[kv.first].offset ];
			ki.values.resize(ki.values.size()+1);
			ValueInfo & vi = ki.values.back();
			vi.count = x.second;
			vi.valueId = kv.second;
		}
	}
	return Stats(std::move(valueInfoStore), std::move(keyInfoStore), std::move(keyInfo));
}

KVStats::Stats KVStats::stats(detail::KVStats::SortedData && data) {
	//calculate KeyInfo
	auto & keyValueCount = data.keyValueCount;
	auto valueInfoStore = std::make_unique<std::vector<detail::KVStats::ValueInfo> >(keyValueCount.size());
	std::vector<detail::KVStats::KeyInfo> keyInfoStore;
	std::unordered_map<uint32_t, detail::KVStats::KeyInfoPtr> keyInfo;
	
	//We can calculate the correct data in a single linear sweep
	uint64_t visOffset = 0;
	for(auto it(keyValueCount.begin()), end(keyValueCount.end()); it != end;) {
		uint32_t keyId = it->first.first;
		keyInfo[keyId].offset = keyInfoStore.size();
		keyInfoStore.emplace_back(keyId, valueInfoStore.get(), visOffset, detail::KVStats::KeyInfo::ValuesContainer::MaxSize);
		auto & ki = keyInfoStore.back();
		auto vit = ki.values.begin();
		for(; it != end && it->first.first == keyId; ++it, ++vit) {
			vit->valueId = it->first.second;
			vit->count = it->second;
			ki.count += it->second;
		}
		ki.values.resize(vit - ki.values.begin());
		visOffset += ki.values.size();
	}
	return Stats(std::move(valueInfoStore), std::move(keyInfoStore), std::move(keyInfo));
}

}//end namespace
