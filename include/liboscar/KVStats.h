#pragma once
#ifndef LIBOSCAR_KVSTATS_H
#define LIBOSCAR_KVSTATS_H

#include <sserialize/containers/CFLArray.h>
#include <liboscar/OsmKeyValueObjectStore.h>

#include <unordered_map>

namespace liboscar {
namespace detail {
namespace KVStats {
	
struct Data {
	std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t> keyValueCount;
	Data();
	Data(const Data & other) = default;
	Data(Data && other);
	Data & operator=(Data && other);
	void update(const Static::OsmKeyValueObjectStore::KVItemBase & item);
	static Data merge(Data && first, Data && second);
};

struct ValueInfo {
	ValueInfo();
	uint32_t count;
	uint32_t valueId;
};

struct KeyInfo {
	KeyInfo();
	uint32_t count;
	sserialize::CFLArray< std::vector<ValueInfo> > values;
};

struct State {
	const Static::OsmKeyValueObjectStore & store;
	const sserialize::ItemIndex & items;
	std::atomic<std::size_t> pos{0};
	
	std::mutex lock;
	std::vector<Data> d;
	
	State(const Static::OsmKeyValueObjectStore & store, const sserialize::ItemIndex & items);
};

struct Worker {
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
public:
	Stats(std::vector<ValueInfo> && valueStore, std::unordered_map<uint32_t, KeyInfo> && keyInfo);
	Stats(Stats && other);
	Stats(const Stats & other) = default;
	Stats & operator=(Stats && other);
	Stats & operator=(const Stats & other) = default;
public:
	std::unordered_map<uint32_t, KeyInfo> & keyInfo() { return m_keyInfo; }
	const std::unordered_map<uint32_t, KeyInfo> & keyInfo() const { return m_keyInfo; }
private:
	std::vector<ValueInfo> m_valueStore;
	std::unordered_map<uint32_t, KeyInfo> m_keyInfo;
};

	
}} //end namespace detail::KVStats
	
class KVStats {
public:
	using ValueInfo = detail::KVStats::ValueInfo;
	using KeyInfo = detail::KVStats::KeyInfo;
	using Stats = detail::KVStats::Stats;
public:
	KVStats(const Static::OsmKeyValueObjectStore & other);
public:
	Stats stats(const sserialize::ItemIndex & items, uint32_t threadCount = 1);
private:
	Static::OsmKeyValueObjectStore m_store;
};

}//end namespace


#endif
