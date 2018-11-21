#pragma once
#ifndef LIBOSCAR_KVSTATS_H
#define LIBOSCAR_KVSTATS_H

#include <sserialize/containers/CFLArray.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/containers/OADHashTable.h>

#include <liboscar/OsmKeyValueObjectStore.h>
#include <liboscar/KVClustering.h>

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

struct NoValueExclusions {
	inline bool operator()(const ValueInfo&) const {return false; }
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
	template<typename TCompare, typename TValueExclusions = NoValueExclusions>
	std::vector<uint32_t> topk(uint32_t k, TCompare compare, TValueExclusions = TValueExclusions()) const;
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

class KeyValueInfo {
public:
	KeyValueInfo() = default;
	KeyValueInfo(const KeyValueInfo & other) = default;
	KeyValueInfo(const KeyInfo & ki) :
	keyId(ki.keyId),
	keyCount(ki.count),
	valueCount(ki.count)
	{}
	KeyValueInfo(const KeyInfo & ki, const ValueInfo & vi) :
	keyId(ki.keyId),
	valueId(vi.valueId),
	keyCount(ki.count),
	valueCount(vi.count)
	{}
public:
	inline bool isKeyOnly() const { return valueId == std::numeric_limits<uint32_t>::max(); }
public:
	uint32_t keyId{std::numeric_limits<uint32_t>::max()};
	uint32_t valueId{std::numeric_limits<uint32_t>::max()};
	uint32_t keyCount{0};
	uint32_t valueCount{0};
};

struct NoKeyExclusions {
	inline bool operator()(const KeyInfo&) const {return false; }
};

struct NoKeyValueExclusions {
	inline bool operator()(const KeyInfo&, const ValueInfo&) const {return false; }
};

class Stats {
public:
	using ValueInfo = liboscar::detail::KVStats::ValueInfo;
	using KeyInfo = liboscar::detail::KVStats::KeyInfo;
	using KeyValueInfo = liboscar::detail::KVStats::KeyValueInfo;
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
	///return topk keyIds sorted according to compare
	///@param compare(KeyInfo)
	template<typename TCompare, typename TKeyExclusions = NoKeyExclusions>
	std::vector<uint32_t> topk(uint32_t k, TCompare compare, TKeyExclusions keyExclusions = TKeyExclusions()) const;
	///return topk key:value pairs, sorted according to compare
	///@param compare(KeyValueInfo, KeyValueInfo)
	///@param keyExclusions(KeyInfo) -> bool; true iff we should NOT analze key-value pairs with the given key
	///@param keyValueExclusions(KeyInfo,ValueInfo) -> bool; true iff we should NOT analze key-value pairs with the given key:value
	template<typename TCompare, typename TKeyExclusions = NoKeyExclusions, typename TKeyValueExclusions = NoKeyValueExclusions>
	std::vector<KeyValueInfo> topkv(uint32_t k, TCompare compare, TKeyExclusions keyExclusions = TKeyExclusions(), TKeyValueExclusions keyValueExclusions = TKeyValueExclusions()) const;
private:
	std::unique_ptr<std::vector<ValueInfo>> m_valueInfoStore;
	std::vector<KeyInfo> m_keyInfoStore;
	std::unordered_map<uint32_t, KeyInfoPtr> m_keyInfo; //keyId -> keyInfoStore
};

template<typename TKeyCompare, typename TKeyValueCompare>
class KVClusteringBase: public liboscar::kvclustering::Interface {
public:
	using KeyExclusions = liboscar::kvclustering::KeyExclusions;
	using KeyValueExclusions = liboscar::kvclustering::KeyValueExclusions;
	using KeyCompare = TKeyCompare;
	using KeyValueCompare = TKeyValueCompare;
	using Stats = detail::KVStats::Stats;
	using Self = KVClusteringBase<KeyCompare, KeyValueCompare>;
public:
	virtual ~KVClusteringBase() override {}
public:
	virtual std::vector<liboscar::kvclustering::KeyInfo> topKeys(uint32_t k) override {
		std::vector<uint32_t> tmp;
		if (m_ke && m_ke->hasExceptions()) {
			tmp = m_stats.topk(k, m_kc, [this](const KeyInfo & ki) {
				return this->m_ke->contains(ki.keyId);
			});
		}
		else {
			tmp = m_stats.topk(k, m_kc);
		}
		std::vector<liboscar::kvclustering::KeyInfo> result;
		result.reserve(tmp.size());
		for(uint32_t keyId : tmp) {
			result.emplace_back(keyId, m_stats.key(keyId).count);
		}
		return result;
	}
	virtual std::vector<liboscar::kvclustering::KeyValueInfo> topKeyValues(uint32_t k) override {
		std::vector<KeyValueInfo> tmp;
		if (m_ke && m_ke->hasExceptions()) {
			auto mke = [this](const KeyInfo & ki) {
				return this->m_ke->contains(ki.keyId);
			};
			if (m_kve && m_kve->hasExceptions()) {
				auto mkve = [this](const KeyInfo & ki, const ValueInfo & vi) {
					return this->m_kve->contains(ki.keyId, vi.valueId);
				};
				tmp = m_stats.topkv(k, m_kvc, mke, mkve);
			}
			else {
				tmp = m_stats.topkv(k, m_kvc, mke);
			}
		}
		else if (m_kve && m_kve->hasExceptions()) {
			auto mkve = [this](const KeyInfo & ki, const ValueInfo & vi) {
				return this->m_kve->contains(ki.keyId, vi.valueId);
			};
			tmp = m_stats.topkv(k, m_kvc, NoKeyExclusions(), mkve);
		}
		else {
			tmp = m_stats.topkv(k, m_kvc);
		}
		std::vector<liboscar::kvclustering::KeyValueInfo> result;
		result.reserve(tmp.size());
		for(const KeyValueInfo & x : tmp) {
			result.emplace_back(
				liboscar::kvclustering::KeyInfo(x.keyId, x.keyCount),
				liboscar::kvclustering::ValueInfo(x.valueId, x.valueCount)
			);
		}
		return result;
	}
public:
	virtual void exclude(const KeyExclusions & e) override {
		m_ke = std::make_unique<KeyExclusions>(e);
	}
	virtual void exclude(const KeyValueExclusions & e) override {
		m_kve = std::make_unique<KeyValueExclusions>(e);
	}
	virtual void preprocess() override {}
protected:
	KVClusteringBase(Stats && stats, KeyCompare kc = KeyCompare(), KeyValueCompare kvc = KeyValueCompare()) :
	m_stats(std::move(stats)),
	m_kc(kc),
	m_kvc(kvc)
	{}
	KVClusteringBase() = delete;
	KVClusteringBase(const KVClusteringBase &) = delete;
private:
	Stats m_stats;
	KeyCompare m_kc;
	KeyValueCompare m_kvc;
	std::unique_ptr<KeyExclusions> m_ke;
	std::unique_ptr<KeyValueExclusions> m_kve;
};

}} //end namespace detail::KVStats

namespace kvclustering {
namespace shannon {
	
struct CompareBase {
	CompareBase(const CompareBase&) = default;
	CompareBase(uint32_t splitThreshold) :
	splitThreshold(splitThreshold)
	{}
	inline uint32_t map(uint32_t count) const {
		if (splitThreshold < count) {
			return count - splitThreshold;
		}
		else {
			return splitThreshold - count;
		}
	}
	uint32_t splitThreshold;
};
	
class KeyCompare: CompareBase {
public:
	using argument_type = liboscar::detail::KVStats::KeyInfo;
public:
	KeyCompare(const KeyCompare &) = default;
	KeyCompare(uint32_t splitThreshold) :
	CompareBase(splitThreshold)
	{}
	bool operator()(const argument_type & a, const argument_type & b) const {
		return !( CompareBase::map(a.count) < CompareBase::map(b.count));
	}
};

class KeyValueCompare: CompareBase {
public:
	using argument_type = liboscar::detail::KVStats::KeyValueInfo;
public:
	KeyValueCompare(const KeyValueCompare&) = default;
	KeyValueCompare(uint32_t splitThreshold) :
	CompareBase(splitThreshold)
	{}
	bool operator()(const argument_type & a, const argument_type & b) const {
		return !( CompareBase::map(a.valueCount) < CompareBase::map(b.valueCount));
	}
};

}//end namespace shannon
	
class ShannonClustering: public liboscar::detail::KVStats::KVClusteringBase<shannon::KeyCompare, shannon::KeyValueCompare> {
public:
	using MyBase = liboscar::detail::KVStats::KVClusteringBase<shannon::KeyCompare, shannon::KeyValueCompare>;
	using KeyCompare = MyBase::KeyCompare;
	using KeyValueCompare = MyBase::KeyValueCompare;
	using Stats = MyBase::Stats;
	using Self = ShannonClustering;
public:
	template<typename... TArgs>
	static
	std::shared_ptr<Self>
	make_shared(TArgs... args) {
		return std::make_shared<Self>(std::forward<TArgs>(args)...);
	}
	template<typename... TArgs>
	static
	std::unique_ptr<Self>
	make_unique(TArgs... args) {
		return std::make_unique<Self>(std::forward<TArgs>(args)...);
	}
	virtual ~ShannonClustering() override {}
public:
	using MyBase::topKeys;
	using MyBase::topKeyValues;
	using MyBase::exclude;
	using MyBase::preprocess;
protected:
public: //actually protected, but make_shared/make_unique need this to be public
	explicit ShannonClustering(Stats && stats, uint32_t keySplitThreshold, uint32_t keyValueSplitThreshold) :
	MyBase(std::move(stats), KeyCompare(keySplitThreshold), KeyValueCompare(keyValueSplitThreshold))
	{}
	ShannonClustering() = delete;
	ShannonClustering(const ShannonClustering&) = delete;
};
	
}//end namespace kvclustering

	
class KVStats final {
public:
	using ValueInfo = detail::KVStats::ValueInfo;
	using KeyInfo = detail::KVStats::KeyInfo;
	using KeyValueInfo = detail::KVStats::KeyValueInfo;
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

}//end namespace livoscar

//Implementation

namespace liboscar {

template<typename TCompare, typename TValueExclusions>
std::vector<uint32_t>
detail::KVStats::KeyInfo::topk(uint32_t k, TCompare compare, TValueExclusions valueExclusions) const {
	auto mycompare = [this, &compare](uint32_t a, uint32_t b) {
		return ! compare(this->values.at(a), this->values.at(b));
	};
	std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(mycompare)> pq(mycompare);
	//add k items to pq
	uint32_t i(0);
	for(uint32_t s(values.size()); i < s && pq.size() < k; ++i) {
		if (valueExclusions(this->values[i])) {
			continue;
		}
		pq.emplace(i);
	}
	//now add one and remove one
	for(uint32_t s(values.size()); i < s; ++i) {
		if (valueExclusions(this->values[i])) {
			continue;
		}
		pq.emplace(i);
		pq.pop();
	}
	//and retrieve them all, they are sorted from small to large, so inverse the mapping
	std::vector<uint32_t> result(pq.size());
	for(auto rit(result.rbegin()), rend(result.rend()); rit != rend; ++rit) {
		*rit = pq.top();
		pq.pop();
	}
	SSERIALIZE_CHEAP_ASSERT_EQUAL(std::size_t(0), pq.size());
	return result;
}

template<typename TCompare, typename TKeyExclusions>
std::vector<uint32_t>
detail::KVStats::Stats::topk(uint32_t k, TCompare compare, TKeyExclusions keyExclusions) const {
	auto mycompare = [this, &compare](uint32_t a, uint32_t b) -> bool {
		return ! compare(this->keys().at(a), this->keys().at(b));
	};
	std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(mycompare)> pq(mycompare);
	//add k items to pq
	uint32_t i(0);
	for(uint32_t s(keys().size()); i < s && pq.size() < k; ++i) {
		if (keyExclusions(this->keys()[i])) {
			continue;
		}
		pq.emplace(i);
	}
	//now add one and remove one
	for(uint32_t s(keys().size()); i < s; ++i) {
		if (keyExclusions(this->keys()[i])) {
			continue;
		}
		pq.emplace(i);
		pq.pop();
	}
	//and retrieve them all, they are sorted from small to large, so inverse the mapping
	std::vector<uint32_t> result(pq.size());
	for(auto rit(result.rbegin()), rend(result.rend()); rit != rend; ++rit) {
		*rit = m_keyInfoStore.at(pq.top()).keyId;
		pq.pop();
	}
	SSERIALIZE_CHEAP_ASSERT_EQUAL(std::size_t(0), pq.size());
	return result;
}


template<typename TCompare, typename TKeyExclusions, typename TKeyValueExclusions>
std::vector<detail::KVStats::Stats::KeyValueInfo>
detail::KVStats::Stats::topkv(uint32_t k, TCompare compare, TKeyExclusions keyExclusions, TKeyValueExclusions keyValueExclusions) const {
	auto mycompare = [&compare](const KeyValueInfo & a, const KeyValueInfo & b) -> bool {
		return ! compare(a, b);
	};
	std::priority_queue<KeyValueInfo, std::vector<KeyValueInfo>, decltype(mycompare)> pq(mycompare);
	//add k items to pq
	uint32_t i(0);
	uint32_t j(0);
	for(uint32_t s(keys().size()); i < s && pq.size() < k; ++i) {
		const KeyInfo & ki = m_keyInfoStore[i];
		if (keyExclusions(ki)) {
			continue;
		}
		
		uint32_t sj(ki.values.size());
		for(; j < sj && pq.size() < k; ++j) {
			if (keyValueExclusions(ki, ki.values[j])) {
				continue;
			}
			pq.emplace(ki, ki.values[j]);
		}
		if (j < ki.values.size()) { //more to come below
			break;
		}
		else {
			j = 0;
		}
	}
	//now add one and remove one
	for(uint32_t s(keys().size()); i < s; ++i) {
		const KeyInfo & ki = m_keyInfoStore[i];
		if (keyExclusions(ki)) {
			continue;
		}
		
		for(uint32_t sj(ki.values.size()); j < sj; ++j) {
			if (keyValueExclusions(ki, ki.values[j])) {
				continue;
			}
			pq.emplace(ki, ki.values[j]);
			pq.pop();
		}
		j = 0;
	}
	//and retrieve them all, they are sorted from small to large, so inverse the mapping
	std::vector<KeyValueInfo> result(pq.size());
	for(auto rit(result.rbegin()), rend(result.rend()); rit != rend; ++rit) {
		*rit = pq.top();
		pq.pop();
	}
	SSERIALIZE_CHEAP_ASSERT_EQUAL(std::size_t(0), pq.size());
	SSERIALIZE_NORMAL_ASSERT( std::is_sorted(result.begin(), result.end(), mycompare) );
	return result;
}

}//end namespace liboscar

#endif
