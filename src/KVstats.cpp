#include <liboscar/KVStats.h>
#include <sserialize/mt/ThreadPool.h>


namespace liboscar {
namespace detail {
namespace KVStats {

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

State::State(const Static::OsmKeyValueObjectStore & store, const sserialize::ItemIndex & items) :
store(store),
items(items)
{}


Worker::Worker(State * state) : state(state) {}

Worker::Worker(const Worker & other) : state(other.state) {}

void Worker::operator()() {
	while (true) {
		std::size_t p = state->pos.fetch_add(1, std::memory_order_relaxed);
		if (p >= state->items.size()) {
			break;
		}
		uint32_t itemId = state->items.at(p);
		d.update( state->store.kvBaseItem(itemId) );
	}
	flush();
}

void Worker::flush() {
	std::unique_lock<std::mutex> lck(state->lock, std::defer_lock);
	while(true) {
		lck.lock();
		state->d.emplace_back(std::move(d));
		if (state->d.size() < 2) {
			break;
		}
		Data first = std::move(state->d.back());
		state->d.pop_back();
		Data second = std::move(state->d.back());
		state->d.pop_back();
		lck.unlock();
		this->d = Data::merge(std::move(first), std::move(second));
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


Stats::KeyInfo & Stats::keyInfo(uint32_t keyId) {
	return m_keyInfoStore.at( m_keyInfo.at(keyId).offset );
}

const Stats::KeyInfo & Stats::keyInfo(uint32_t keyId) const {
	return m_keyInfoStore.at( m_keyInfo.at(keyId).offset );
}

}} //end namespace detail::KVStats


KVStats::KVStats(const Static::OsmKeyValueObjectStore & store) :
m_store(store)
{}

KVStats::Stats KVStats::stats(const sserialize::ItemIndex & items, uint32_t threadCount) {
	if (items.type() & sserialize::ItemIndex::RANDOM_ACCESS_NO) {
		return stats( sserialize::ItemIndex( items.toVector() ) );
	}
	
	detail::KVStats::State state(m_store, items);
	
	sserialize::ThreadPool::execute(detail::KVStats::Worker(&state), threadCount, sserialize::ThreadPool::CopyTaskTag());
	
	//calculate KeyInfo
	auto & keyValueCount = state.d.front().keyValueCount;
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
	
}//end namespace
