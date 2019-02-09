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
	KeyValueCountVec keyValueCountVecSortedByIds;
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
	using ValueCountPair = std::pair<uint32_t, uint32_t>;
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
	void preprocess() override;

	std::vector<std::pair<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>> facets(uint32_t k);

	std::list<ValueCountPair> findValuesToKey(std::uint32_t keyId);

	void exclude(const kvclustering::KeyExclusions & e) override;

	void exclude(const kvclustering::KeyValueExclusions & e) override;

	std::vector<KeyInfo> topKeys(uint32_t k) override;

	std::vector<KeyValueInfo> topKeyValues(uint32_t k) override;;

	//returns true if the number of intersections is greater than minNumber
	template<typename It>
	bool hasIntersection(It beginI, It endI, It beginJ, It endJ, const std::float_t &minNumber);
};


} // end namepsace liboscar

#endif //OSCAR_WEB_KOMACLUSTERING_H
