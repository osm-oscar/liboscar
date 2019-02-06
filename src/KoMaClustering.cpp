#include <liboscar/KoMaClustering.h>

namespace liboscar{
namespace detail {
namespace KoMaClustering {
void
Data::update(const KeyValue &key, const uint32_t& itemId) {
	keyValueItemMap[key].emplace_back(itemId);
}
void
Data::sort() {
	// sort
	for (auto &keyValueItems : keyValueItemMap){
		std::sort(keyValueItems.second.begin(), keyValueItems.second.end());
		keyValueCountVec.emplace_back(std::make_pair(keyValueItems.first, keyValueItems.second.size()));
		keyValueCountVecSortedByIds.emplace_back(std::make_pair(keyValueItems.first, keyValueItems.second.size()));
	}
	// sort all keyValues descending by itemCount to find the top KeyValues faster
	std::sort(keyValueCountVec.begin(), keyValueCountVec.end(),
			  [](std::pair<KeyValue, std::uint32_t> const &a,
				 std::pair<KeyValue, std::uint32_t> const &b) {
				  return a.second != b.second ? a.second > b.second : a.first < b.first;
			  });
	std::sort(keyValueCountVecSortedByIds.begin(), keyValueCountVecSortedByIds.end(),
			  [](std::pair<KeyValue, std::uint32_t> const &a,
				 std::pair<KeyValue, std::uint32_t> const &b) {
				  return a.first.first != b.first.first ? a.first.first < b.first.first : a.second > b.second;
			  });

}
Data::Data() {}

State::State(const Static::OsmKeyValueObjectStore &store, const sserialize::CellQueryResult &cqr,
			 const kvclustering::KeyExclusions &keyExclusions,
			 const kvclustering::KeyValueExclusions &keyValueExclusions,
			 Data & d) :
		keyExclusions(keyExclusions),
		keyValueExclusions(keyValueExclusions),
		store(store),
		cqr(cqr),
		d(d)
{}
Worker::Worker(State * state) :
		state(state)
{}
void Worker::operator()() {
	for (sserialize::CellQueryResult::const_iterator it(state->cqr.begin()), end(state->cqr.end()); it != end; ++it) {
		for (const uint32_t &x : it.idx()) {
			const auto &item = state->store.kvBaseItem(x);
			//iterate over all item keys-value items
			for (uint32_t i = 0; i < item.size(); ++i) {

				state->d.update(std::make_pair(item.keyId(i), item.valueId(i)), x);
			}
		}
	}
}


} //end namespace KoMaClustering
} //end namespace detail

KoMaClustering::KoMaClustering(const Static::OsmKeyValueObjectStore &store,
							   const sserialize::CellQueryResult &cqr,
							   kvclustering::KeyExclusions &keyExclusions,
							   kvclustering::KeyValueExclusions &keyValueExclusions) :
		store(store), cqr(cqr), keyExclusions(keyExclusions), keyValueExclusions(keyValueExclusions)
{}

void KoMaClustering::preprocess()  {
	auto state = detail::KoMaClustering::State(store, cqr, keyExclusions, keyValueExclusions, m_data);
	auto worker = detail::KoMaClustering::Worker(&state);
	worker();
	state.d.sort();
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
	auto &countVec = m_data.keyValueCountVec;
	auto &itemMap = m_data.keyValueItemMap;
	auto itI = countVec.begin() + 1;
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
			if(keyExclusions.contains(itK->first.first))
				continue;
			if(keyValueExclusions.contains(itK -> first.first, itK->first.second))
				continue;
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
KoMaClustering::facets(uint32_t k) {
	std::vector<std::pair<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>> result;
	for(auto i = 0; i < k; ++i){
		auto keyId = topKeyValues(1).at(0).ki.keyId;
		const auto& valueVector = findValuesToKey(keyId);
		result.emplace_back(keyId, valueVector);
		keyExclusions.add(keyId);
		keyExclusions.preprocess();
	}
	return result;
}


std::list<KoMaClustering::ValueCountPair> KoMaClustering::findValuesToKey(std::uint32_t keyId) {
	std::list<KoMaClustering::ValueCountPair> result;
	// binary search key
	size_t lb = 0;
	size_t ub = m_data.keyValueCountVecSortedByIds.size();

	while(ub >= lb) {
		size_t mid = lb + (ub - lb) / 2;
		if(m_data.keyValueCountVecSortedByIds.at(mid).first.first == keyId ) {
			// found key now go backwards and forwards to find all other key values with the same key
			for(size_t i = mid; i > 0; i-- ){
				if(m_data.keyValueCountVecSortedByIds.at(i).first.first == keyId){
					auto& keyValuePair = m_data.keyValueCountVecSortedByIds.at(i);
					// emplace in front of result so that ordering of counts is correct
					result.emplace(result.begin(), std::make_pair(keyValuePair.first.second, keyValuePair.second));
				} else {
					break;
				}
			}
			for(size_t i = mid + 1; i < m_data.keyValueCountVecSortedByIds.size(); i++){
				if(m_data.keyValueCountVecSortedByIds.at(i).first.first == keyId){
					auto& keyValuePair = m_data.keyValueCountVecSortedByIds.at(i);
					result.emplace_back(keyValuePair.first.second, keyValuePair.second);
				} else {
					break;
				}
			}
			break;
		}
		else if(m_data.keyValueCountVecSortedByIds.at(mid).first.first < keyId) {
			lb = mid + 1;
		} else {
			ub = mid - 1;
		}
	}

	return result;
}
} //end namespace liboscar

