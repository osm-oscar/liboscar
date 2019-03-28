#pragma once
#ifndef OSCAR_WEB_KOMACLUSTERING_H
#define OSCAR_WEB_KOMACLUSTERING_H

#include <unordered_map>
#include <vector>
#include "OsmKeyValueObjectStore.h"
#include "KVClustering.h"
#include <sserialize/containers/OADHashTable.h>

namespace liboscar {
namespace detail {
namespace KoMaClustering {
struct Data {
	using KeyValue = std::pair<uint32_t, uint32_t>;
	using KeyValueItemMap = sserialize::OADHashTable<KeyValue, std::vector<uint32_t>>;
	KeyValueItemMap keyValueItemMap;
	//const kvclustering::KeyExclusions &keyExclusions;
	//const kvclustering::KeyValueExclusions &keyValueExclusions;
	Data();
	void update(const KeyValue &key, const uint32_t& itemId);
};

struct State {
	const Static::OsmKeyValueObjectStore &store;
	const sserialize::ItemIndex &items;
	const kvclustering::KeyExclusions &keyExclusions;
	const kvclustering::KeyValueExclusions &keyValueExclusions;
	Data::KeyValueItemMap &keyValueItemMap;
	std::vector<Data> d;
	size_t numberOfThreads;
	std::mutex m;
	std::atomic<std::size_t> pos{0};
	void merge();
	State(const Static::OsmKeyValueObjectStore &store,
		const sserialize::ItemIndex &items,
		const kvclustering::KeyExclusions &keyExclusions,
		const kvclustering::KeyValueExclusions &keyValueExclusions,
		Data::KeyValueItemMap &keyValueItemMap,
		size_t numberOfThreads);
};

struct Worker {
	//number of items to fetch at once
	static constexpr std::size_t BlockSize = 1000;
	//number of queued key-value pairs before flushing
	static constexpr std::size_t FlushSize = BlockSize * 1000;
	State * state;
	Data d;
	void operator()();
	void flush();
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
			sserialize::ItemIndex &items,
			kvclustering::KeyExclusions &keyExclusions,
			kvclustering::KeyValueExclusions &keyValueExclusions,
			uint32_t threadCount);

	~KoMaClustering() override = default;

private:
	const Static::OsmKeyValueObjectStore &store;
	const sserialize::ItemIndex &items;
	kvclustering::KeyExclusions &keyExclusions;
	kvclustering::KeyValueExclusions &keyValueExclusions;
	using KeyValueCountVec = std::vector<std::pair<detail::KoMaClustering::Data::KeyValue, uint32_t>>;
	detail::KoMaClustering::Data::KeyValueItemMap keyValueItemMap;
	KeyValueCountVec keyValueCountVec;
	KeyValueCountVec keyValueCountVecSortedByIds;
	uint32_t threadCount;

	void sort();
public:
	void preprocess() override;

	std::vector<std::pair<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>> facets(uint32_t k, std::map<std::uint32_t, std::uint32_t> dynFacetSize, std::uint32_t defaultFacetSize);

	std::list<ValueCountPair> findValuesToKey(std::uint32_t keyId, std::uint32_t facetSize);

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
