#pragma once
#ifndef OSCAR_WEB_KOMACLUSTERING_H
#define OSCAR_WEB_KOMACLUSTERING_H

#include <unordered_map>
#include <vector>
#include "OsmKeyValueObjectStore.h"
#include "KVClustering.h"

namespace liboscar {
namespace detail {
namespace KoMaClustering {
struct Data {
	using KeyValue = std::pair<uint32_t, uint32_t>;
	using KeyValueItemMap = std::unordered_map<KeyValue, std::vector<uint32_t>>;
	using KeyValueCountVec = std::vector<std::pair<KeyValue, uint32_t>>;
	KeyValueItemMap keyValueItemMap;
	KeyValueCountVec keyValueCountVec;
	//const kvclustering::KeyExclusions &keyExclusions;
	//const kvclustering::KeyValueExclusions &keyValueExclusions;
	Data();
	void update(const KeyValue &key, const uint32_t& itemId);
	void sort();
};

struct State {
	const Static::OsmKeyValueObjectStore &store;
	const sserialize::CellQueryResult &cqr;
	const kvclustering::KeyExclusions &keyExclusions;
	const kvclustering::KeyValueExclusions &keyValueExclusions;
	Data & d;
	State(const Static::OsmKeyValueObjectStore &store,
		const sserialize::CellQueryResult &cqr,
		const kvclustering::KeyExclusions &keyExclusions,
		const kvclustering::KeyValueExclusions &keyValueExclusions,
		Data & d);
};

struct Worker {
	State * state;
	void operator()();
	Worker(State * state);
};
} // end namespace KoMaClustering
} // end namespace detail

// Implementation
class KoMaClustering final : public kvclustering::Interface {
public:
	using KeyValueInfo = kvclustering::KeyValueInfo;
	using KeyInfo = kvclustering::KeyInfo;
	using ValueInfo = kvclustering::ValueInfo;
	using KeyValuePair = std::pair<uint32_t, uint32_t>;
public:
	KoMaClustering(const Static::OsmKeyValueObjectStore &store,
			const sserialize::CellQueryResult &cqr,
			kvclustering::KeyExclusions &keyExclusions,
			kvclustering::KeyValueExclusions &keyValueExclusions);

	~KoMaClustering() override = default;

private:
	const Static::OsmKeyValueObjectStore &store;
	detail::KoMaClustering::Data m_data;
	const sserialize::CellQueryResult &cqr;
	kvclustering::KeyExclusions &keyExclusions;
	kvclustering::KeyValueExclusions &keyValueExclusions;
public:
	void preprocess() override {
		auto state = detail::KoMaClustering::State(store, cqr, keyExclusions, keyValueExclusions, m_data);
		auto worker = detail::KoMaClustering::Worker(&state);
		worker();
		state.d.sort();
	};

	void exclude(const kvclustering::KeyExclusions & e) override {
		keyExclusions + e;
	}

	void exclude(const kvclustering::KeyValueExclusions & e) override {
		keyValueExclusions + e;
	}

	std::vector<KeyInfo> topKeys(uint32_t k) override {
		return std::vector<KeyInfo>();
	}

	std::vector<KeyValueInfo> topKeyValues(uint32_t k) override {
		std::vector<std::pair<KeyValuePair, std::uint32_t>> result;
		auto &countVec = m_data.keyValueCountVec;
		auto &itemMap = m_data.keyValueItemMap;
		auto itI = countVec.begin() + 1;
		bool startParentsFound = false;
		std::float_t maxNumberOfIntersections;
		for (; itI < countVec.end(); ++itI) {
			for (auto itJ = countVec.begin(); itJ < itI; ++itJ) {
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
	};

	//returns true if the number of intersections is greater than minNumber
	template<typename It>
	bool hasIntersection(It beginI, It endI, It beginJ, It endJ, const std::float_t &minNumber);
};

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
} // end namepsace liboscar

#endif //OSCAR_WEB_KOMACLUSTERING_H
