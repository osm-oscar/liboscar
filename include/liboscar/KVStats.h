#pragma once
#ifndef LIBOSCAR_KVSTATS_H
#define LIBOSCAR_KVSTATS_H

#include <sserialize/containers/CFLArray.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/containers/OADHashTable.h>

#include <liboscar/OsmKeyValueObjectStore.h>

#include <unordered_map>
#include <queue>

namespace liboscar {
namespace detail {
namespace KVStats {

struct Data;

struct SortedData {
	using KeyValue = std::pair<uint32_t, uint32_t>;
	using KeyValueCount = std::pair<KeyValue, uint32_t>;
	using KeyValueCountContainer = std::vector<KeyValueCount>;
	KeyValueCountContainer keyValueCount;
	SortedData();
	SortedData(const SortedData & other) = default;
	SortedData(Data && other);
	SortedData(SortedData && other);
	SortedData & operator=(SortedData && other);
	static SortedData merge(SortedData && first, SortedData && second);
};

struct Data {
	using KeyValueCountUnorderedMap = std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t>;
	using KeyValueCountOADMap = sserialize::OADHashTable<std::pair<uint32_t, uint32_t>, uint32_t>;
	using KeyValueCountSortedMap = std::map<std::pair<uint32_t, uint32_t>, uint32_t>;
	using KeyValueCountMap = KeyValueCountOADMap;
	KeyValueCountMap keyValueCount;
	Data();
	Data(const Data & other) = default;
	Data(Data && other);
	Data & operator=(Data && other);
	void update(const Static::OsmKeyValueObjectStore::KVItemBase & item);
	static Data merge(Data && first, Data && second);
};


struct ValueInfo {
	uint32_t valueId{ std::numeric_limits<uint32_t>::max() };
	uint32_t count{0};
};

class KeyInfo {
public:
	using ValuesContainer = sserialize::CFLArray< std::vector<ValueInfo> >;
public:
	KeyInfo();
	KeyInfo(uint32_t keyId, std::vector<ValueInfo> * valuesContainer, uint64_t offset, uint32_t size);
	///get the topk entries in value (sorted in arbitrary order)
	///This function returns offsets into values!
	///@complexity O( log(k)*values.size() )
	///@storage O(k)
	///@param compare(first, second) weak order returning true if first comes before second
	template<typename TCompare>
	std::vector<uint32_t> topk(uint32_t k, TCompare compare) const;
public:
	uint32_t keyId{ std::numeric_limits<uint32_t>::max() };
	uint32_t count{0};
	ValuesContainer values;
};

struct KeyInfoPtr {
	uint32_t offset{ std::numeric_limits<uint32_t>::max() };
	inline bool valid() const { return offset != std::numeric_limits<uint32_t>::max(); }
};

struct State {
	using DataType = SortedData;
	const Static::OsmKeyValueObjectStore & store;
	const sserialize::ItemIndex & items;
	std::atomic<std::size_t> pos{0};
	
	std::mutex lock;
	std::vector<DataType> d;
	
	State(const Static::OsmKeyValueObjectStore & store, const sserialize::ItemIndex & items);
};

struct Worker {
	//number of items to fetch at once
	static constexpr std::size_t BlockSize = 1000;
	//number of queued key-value pairs before flushing
	static constexpr std::size_t FlushSize = BlockSize * 1000;
	Data d;
	State * state;
	Worker(State * state);
	Worker(const Worker & other);
	void operator()();
	void flush();
};

class Stats {
public:
	using ValueInfo = liboscar::detail::KVStats::ValueInfo;
	using KeyInfo = liboscar::detail::KVStats::KeyInfo;
	using KeyInfoIterator = std::vector<KeyInfo>::iterator;
	using KeyInfoConstIterator = std::vector<KeyInfo>::const_iterator;
public:
	Stats(std::unique_ptr<std::vector<ValueInfo>> && valueInfoStore, std::vector<KeyInfo> && keyInfoStore, std::unordered_map<uint32_t, KeyInfoPtr> && keyInfo);
	Stats(Stats && other);
	Stats & operator=(Stats && other);
public:
	KeyInfo & key(uint32_t keyId);
	const KeyInfo & key(uint32_t keyId) const;
	const std::vector<KeyInfo> & keys() const;
	std::vector<KeyInfo> & keys();
public:
	KeyInfoIterator keysBegin();
	KeyInfoConstIterator keysBegin() const;
	KeyInfoIterator keysEnd();
	KeyInfoConstIterator keysEnd() const;
public:
	///return topk keyIds sorting accroding to compare
	template<typename TCompare>
	std::vector<uint32_t> topk(uint32_t k, TCompare compare) const;
private:
	std::unique_ptr<std::vector<ValueInfo>> m_valueInfoStore;
	std::vector<KeyInfo> m_keyInfoStore;
	std::unordered_map<uint32_t, KeyInfoPtr> m_keyInfo; //keyId -> keyInfoStore
};

	
}} //end namespace detail::KVStats
	
class KVStats final {
public:
	using ValueInfo = detail::KVStats::ValueInfo;
	using KeyInfo = detail::KVStats::KeyInfo;
	using Stats = detail::KVStats::Stats;
public:
	KVStats(const Static::OsmKeyValueObjectStore & other);
public:
	Stats stats(const sserialize::ItemIndex & items, uint32_t threadCount = 1);
private:
	Stats stats(detail::KVStats::Data && data);
	Stats stats(detail::KVStats::SortedData && data);
private:
	Static::OsmKeyValueObjectStore m_store;
};

}//end namespace

//Implementation

namespace liboscar {

template<typename TCompare>
std::vector<uint32_t>
detail::KVStats::KeyInfo::topk(uint32_t k, TCompare compare) const {
	if (values.size() <= k) {
		auto range = sserialize::RangeGenerator<uint32_t>::range(0, values.size());
		return std::vector<uint32_t>(range.begin(), range.end());
	}
	auto mycompare = [this, &compare](uint32_t a, uint32_t b) {
		return ! compare(this->values.at(a), this->values.at(b));
	};
	std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(mycompare)> pq(mycompare);
	//add k items to pq
	uint32_t i(0);
	for(; i < k; ++i) {
		pq.emplace(i);
	}
	//now add one and remove one
	for(uint32_t s(values.size()); i < s; ++i) {
		pq.emplace(i);
		pq.pop();
	}
	//and retrieve them all, they are sorted from small to large, so inverse the mapping
	std::vector<uint32_t> result(k);
	for(auto rit(result.rbegin()), rend(result.rend()); rit != rend; ++rit) {
		*rit = pq.top();
		pq.pop();
	}
	SSERIALIZE_CHEAP_ASSERT_EQUAL(std::size_t(0), pq.size());
	return result;
}

template<typename TCompare>
std::vector<uint32_t>
detail::KVStats::Stats::topk(uint32_t k, TCompare compare) const {
	auto mycompare = [this, &compare](uint32_t a, uint32_t b) -> bool {
		return ! compare(this->keys().at(a), this->keys().at(b));
	};
	auto pos2id = [this](std::vector<uint32_t> & result) -> void {
		for(uint32_t & x : result) {
			x = m_keyInfoStore[x].keyId;
		}
	};
	if (keys().size() <= k) {
		auto range = sserialize::RangeGenerator<uint32_t>::range(0, keys().size());
		std::vector<uint32_t> result(range.begin(), range.end());
		std::sort(result.begin(), result.end(), mycompare);
		pos2id(result);
		return result;
	}
	std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(mycompare)> pq(mycompare);
	//add k items to pq
	uint32_t i(0);
	for(; i < k; ++i) {
		pq.emplace(i);
	}
	//now add one and remove one
	for(uint32_t s(keys().size()); i < s; ++i) {
		pq.emplace(i);
		pq.pop();
	}
	//and retrieve them all, they are sorted from small to large, so inverse the mapping
	std::vector<uint32_t> result(k);
	for(auto rit(result.rbegin()), rend(result.rend()); rit != rend; ++rit) {
		*rit = pq.top();
		pq.pop();
	}
	SSERIALIZE_CHEAP_ASSERT_EQUAL(std::size_t(0), pq.size());
	pos2id(result);
	return result;
}

}//end namespace

#endif
