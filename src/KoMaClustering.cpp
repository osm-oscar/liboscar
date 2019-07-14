#include <liboscar/KoMaClustering.h>
#include <sserialize/mt/ThreadPool.h>

namespace liboscar{
namespace detail {
namespace KoMaClustering {
void
Data::update(const KeyValue &key, const uint32_t& itemId) {
	keyValueItemMap[key].emplace_back(itemId);
}
Data::Data() {}

State::State(const Static::OsmKeyValueObjectStore &store, const sserialize::ItemIndex &items,
			 const kvclustering::KeyExclusions &keyExclusions,
			 const kvclustering::KeyValueExclusions &keyValueExclusions,
			 Data::KeyValueItemMap &keyValueItemMap,
			 size_t numberOfThreads) :
		store(store),
		items(items),
		keyExclusions(keyExclusions),
		keyValueExclusions(keyValueExclusions),
		keyValueItemMap(keyValueItemMap),
		numberOfThreads(numberOfThreads)
{}

void State::merge() {
	for(auto & data : d) {
		for(auto & keyValueItem : data.keyValueItemMap) {
			auto & destination = keyValueItemMap[keyValueItem.first];
			auto & itemVec = keyValueItem.second;
			std::move(itemVec.begin(), itemVec.end(), std::back_inserter(destination));
		}
	}
}

Worker::Worker(State * state):
		state(state)
{}
void Worker::operator()() {

	size_t size = state->items.size();
	while(true) {
		std::size_t p = state->pos.fetch_add(BlockSize, std::memory_order_relaxed);
		if (p>= state->items.size())
			break;
		for(std::size_t i(0); i < BlockSize && p < size; ++i, ++p) {
			 uint32_t itemId = state->items.at(p);
			 const auto &item = state->store.kvBaseItem(itemId);
			//iterate over all key-value pairs
			 for (uint32_t i = 0; i < item.size(); ++i) {
				 d.update(std::make_pair(item.keyId(i), item.valueId(i)), p);
			 }
			 if(d.keyValueItemMap.size() > FlushSize)
			 	flush();
		 }
	}
	flush();
}

void Worker::flush() {
	std::unique_lock<std::mutex> lck{state->m, std::defer_lock};
	lck.lock();
	auto & destination = state->d;
	destination.emplace_back(std::move(d));
	lck.unlock();
	d.keyValueItemMap.clear();
}


} //end namespace KoMaClustering
} //end namespace detail

KoMaClustering::KoMaClustering(const Static::OsmKeyValueObjectStore &store,
							   sserialize::ItemIndex &items,
							   kvclustering::KeyExclusions &keyExclusions,
							   kvclustering::KeyValueExclusions &keyValueExclusions,
							   const uint32_t threadCount) :
		store(store), items(items), keyExclusions(keyExclusions), keyValueExclusions(keyValueExclusions), threadCount(threadCount)
{
	if (items.type() & sserialize::ItemIndex::RANDOM_ACCESS_NO) {
		items = sserialize::ItemIndex( items.toVector() );
	}
}

void KoMaClustering::preprocess()  {

	auto state = detail::KoMaClustering::State(store, items, keyExclusions, keyValueExclusions, keyValueItemMap, threadCount);

	sserialize::ThreadPool::execute(detail::KoMaClustering::Worker(&state), threadCount,
	        sserialize::ThreadPool::CopyTaskTag());
	state.merge();
	sort();
}

void KoMaClustering::exclude(const kvclustering::KeyExclusions &e) {
	keyExclusions + e;
}

void KoMaClustering::exclude(const kvclustering::KeyValueExclusions &e) {
	keyValueExclusions + e;
}

std::vector<KoMaClustering::KeyInfo> KoMaClustering::topKeys(uint32_t k) {
	return std::vector<KeyInfo>();
}

std::vector<KoMaClustering::KeyValueInfo> KoMaClustering::topKeyValues(uint32_t k) {
	std::vector<std::pair<KeyValuePair, std::uint32_t>> result;
	auto &countVec = keyValueCountVec;
	auto &itemMap = keyValueItemMap;
	auto itI = countVec.begin();
	bool startParentsFound = false;
	std::float_t maxNumberOfIntersections;

	for (; itI < countVec.end(); ++itI) {
		if(keyExclusions.contains(itI->first.first))
			continue;
		if(keyValueExclusions.contains(itI -> first.first, itI->first.second))
			continue;


		for (auto itJ = countVec.begin(); itJ < itI; ++itJ) {
			if(keyExclusions.contains(itJ->first.first))
				continue;
			if(keyValueExclusions.contains(itJ -> first.first, itJ->first.second))
				continue;
			const std::vector<uint32_t> &setI = itemMap[itI->first];
			const std::vector<uint32_t> &setJ = itemMap[itJ->first];

			maxNumberOfIntersections = (setI.size() + setJ.size()) / 200.0f;
			if (!hasIntersection(setI.begin(), setI.end(), setJ.begin(), setJ.end(), maxNumberOfIntersections)) {
				// no required amount of intersections
				// add both parents to results
				result.emplace_back(itJ->first, itJ->second);
				result.emplace_back(itI->first, itI->second);
				//end the algorithm
				startParentsFound = true;
				break;
			}
		}
		if(startParentsFound)
			break;
	}



	if (startParentsFound) {
		for (auto itK = itI + 1; itK < countVec.end() && result.size() < k; ++itK) {
			if(keyExclusions.contains(itK->first.first)) {
				continue;
			}
			if(keyValueExclusions.contains(itK -> first.first, itK->first.second)) {
				continue;
			}
			bool discarded = false;
			for (auto &parentPair : result) {
				maxNumberOfIntersections = (parentPair.second + (*itK).second) / 200.0f;
				const std::vector<uint32_t> &setI = itemMap[(*itK).first];
				const std::vector<uint32_t> &setJ = itemMap[parentPair.first];
				if (hasIntersection(setI.begin(), setI.end(), setJ.begin(), setJ.end(), maxNumberOfIntersections)) {
					discarded = true;
					break;
				}
			}
			if (!discarded) {
				//parent does not intersect with previous found parents; add to results
				result.emplace_back(*itK);
			}
		}
	}
	std::vector<KeyValueInfo> keyValueResult;
	for (auto &keyValuePair : result) {
		keyValueResult.emplace_back(KeyValueInfo(KeyInfo(keyValuePair.first.first, keyValuePair.second),
												 ValueInfo(keyValuePair.first.second, keyValuePair.second)));
	}

	return keyValueResult;
}
template<typename It>
bool KoMaClustering::hasIntersection(
		It beginI, It endI, It beginJ, It endJ, const std::float_t &minNumber) {
	std::uint32_t intersectionCount = 0;
	while (beginI != endI && beginJ != endJ) {
		if (*beginI < *beginJ) ++beginI;
		else if (*beginJ < *beginI) ++beginJ;
		else {
			++beginI;
			++beginJ;
			if (++intersectionCount > minNumber) {
				return true;
			};
		}
	}
	return false;
}

std::vector<std::pair<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>>
KoMaClustering::facets(uint32_t k, std::map<std::uint32_t, std::uint32_t> dynFacetSize, std::uint32_t defaultFacetSize) {
	std::vector<std::pair<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>> result;
	std::unordered_set<uint32_t> keyIds;
	for(auto i = 0; i < k; ++i){
		const auto& topKeyValue = topKeyValues(1);
		if(topKeyValue.empty())
			return result;
		const auto keyId = topKeyValue.at(0).ki.keyId;
		if(keyIds.find(keyId)!=keyIds.end())
			break;

		keyIds.emplace(keyId);
		uint32_t facetSize = dynFacetSize.find(keyId) == dynFacetSize.end() ? defaultFacetSize : dynFacetSize[keyId];
		const auto& valueVector = findValuesToKey(keyId, facetSize);
		result.emplace_back(keyId, valueVector);
		keyExclusions.add(keyId);
		keyExclusions.preprocess();
	}
	return result;
}


std::list<KoMaClustering::ValueCountPair> KoMaClustering::findValuesToKey(std::uint32_t keyId, std::uint32_t facetSize) {
	std::list<KoMaClustering::ValueCountPair> result;
	// find range of key value pairs which match the given key
	// use binary search to find any matching key value pair in the key value list
	// then propagate forwards and backwards to find the facet range
	// report all matches according to facetSize

	// the range is saved in the variables begin and end
	size_t begin, end;

	size_t lb = 0;
	size_t ub = keyValueCountVecSortedByIds.size();


	while(ub >= lb) {
		size_t mid = lb + (ub - lb) / 2;
		if(keyValueCountVecSortedByIds.at(mid).first.first == keyId ) {
			// found key now go backwards and forwards to find the total range of values with the same key
			begin = mid;
			// backwards
			for(; begin > 0; --begin){
				if(keyValueCountVecSortedByIds.at(begin).first.first != keyId){
					begin++;
					break;
				}
			}
			end = mid + 1;
			// forwards
			for(;end < keyValueCountVecSortedByIds.size(); ++end){
				if(keyValueCountVecSortedByIds.at(end).first.first != keyId){
						end--;
						break;
				}
			}
			break;

		}
		else if(keyValueCountVecSortedByIds.at(mid).first.first < keyId) {
			lb = mid + 1;
		} else {
			ub = mid - 1;
		}
	}
	for(int i = begin; i <= end && i < begin+facetSize; ++i) {
		auto& keyValuePair = keyValueCountVecSortedByIds.at(i);
		result.emplace_back(std::make_pair(keyValuePair.first.second, keyValuePair.second));
	}
	return result;
}

void KoMaClustering::sort() {
	// sort
	for (auto &keyValueItems : keyValueItemMap){
		std::sort(keyValueItems.second.begin(), keyValueItems.second.end());
		keyValueCountVec.emplace_back(std::make_pair(keyValueItems.first, keyValueItems.second.size()));
		keyValueCountVecSortedByIds.emplace_back(std::make_pair(keyValueItems.first, keyValueItems.second.size()));
	}
	using KeyValue = std::pair<std::uint32_t, std::uint32_t>;
	// sort all keyValues descending by itemCount to find the top KeyValues faster
	std::sort(keyValueCountVec.begin(), keyValueCountVec.end(),
			  [](std::pair<KeyValue , std::uint32_t> const &a,
				 std::pair<KeyValue, std::uint32_t> const &b) {
				  return a.second != b.second ? a.second > b.second : a.first < b.first;
			  });
	std::sort(keyValueCountVecSortedByIds.begin(), keyValueCountVecSortedByIds.end(),
			  [](std::pair<KeyValue, std::uint32_t> const &a,
				 std::pair<KeyValue, std::uint32_t> const &b) {
				  return a.first.first != b.first.first ? a.first.first < b.first.first : a.second > b.second;
			  });

}

} //end namespace liboscar
