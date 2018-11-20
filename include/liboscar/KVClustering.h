#pragma once
#ifndef LIBOSCAR_KVCLUSTERING_H
#define LIBOSCAR_KVCLUSTERING_H
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <sserialize/Static/StringTable.h>
#include <sserialize/algorithm/hashspecializations.h>

namespace liboscar {
namespace kvclustering {
	
class KeyExclusions final {
public:
	using KeyStringTable = sserialize::Static::SortedStringTable;
public:
	KeyExclusions(const KeyStringTable & kst);
	~KeyExclusions();
public:
	void add(const std::string & key);
	void add(uint32_t keyId);
	void addPrefix(const std::string & keyprefix);
	void addPrefix(uint32_t keyIdBegin, uint32_t keyIdEnd);
public:
	///create search data structures
	void preprocess();
public:
	bool hasExceptions() const;
	bool contains(uint32_t keyId) const;
private:
	struct KeyRange {
		uint32_t begin;
		uint32_t end;
		KeyRange(uint32_t begin, uint32_t end);
		KeyRange(const KeyRange &) = default;
		bool contains(uint32_t keyId) const;
		bool overlap(const KeyRange & other) const;
		KeyRange merge(const KeyRange & other) const;
	};
private:
	KeyStringTable m_kst;
	bool m_valid{true};
	std::vector<KeyRange> m_keyRange;
};

class KeyValueExclusions final {
public:
	using KeyStringTable = sserialize::Static::SortedStringTable;
	using ValueStringTable = sserialize::Static::SortedStringTable;
public:
	KeyValueExclusions(const KeyStringTable & kst, const ValueStringTable & vst);
	~KeyValueExclusions();
	void add(const std::string & key, const std::string & value);
	void add(uint32_t keyId, uint32_t valueId);
public:
	bool hasExceptions() const;
	bool contains(uint32_t keyId, uint32_t valueId) const;
private:
	using KeyValue = std::pair<uint32_t, uint32_t>;
private:
	KeyStringTable m_kst;
	ValueStringTable m_vst;
	std::unordered_set<KeyValue> m_keyValues;
};

class KeyInfo {
public:
	KeyInfo() = default;
	KeyInfo(uint32_t keyId, uint32_t keyStats) :
	keyId(keyId),
	keyStats(keyStats)
	{}
	KeyInfo(const KeyInfo &) = default;
public:
	uint32_t keyId;
	uint32_t keyStats; //usually the number of keys but may also be an offset into a larger stats structure
};

class ValueInfo {
public:
	ValueInfo() = default;
	ValueInfo(uint32_t valueId, uint32_t valueStats) :
	valueId(valueId),
	valueStats(valueStats)
	{}
	ValueInfo(const ValueInfo &) = default;
public:
	uint32_t valueId;
	uint32_t valueStats; //usually the number of value but may also be an offset into a larger stats structure
};

class KeyValueInfo {
public:
	KeyValueInfo() = default;
	KeyValueInfo(const KeyInfo & ki, const ValueInfo & vi) :
	ki(ki),
	vi(vi)
	{}
	KeyValueInfo(const KeyValueInfo&) = default;
public:
	KeyInfo ki;
	ValueInfo vi;
};

class Interface {
public:
	template<typename TSubClass, typename... TArgs>
	static
	std::shared_ptr<typename 
		std::enable_if<
			std::is_base_of<Interface, TSubClass>::value,
			TSubClass
		>
		::type
	>
	make_shared(TArgs... args) { return std::make_shared<TSubClass>(std::forward<TArgs>(args)...); }
	
	template<typename TSubClass, typename... TArgs>
	static
	std::unique_ptr<typename 
		std::enable_if<
			std::is_base_of<Interface, TSubClass>::value,
			TSubClass
		>
		::type
	>
	make_unique(TArgs... args) { return std::make_unique<TSubClass>(std::forward<TArgs>(args)...); }
public:
	virtual ~Interface() {}
public:
	virtual std::vector<KeyInfo> topKeys(uint32_t k) = 0;
	virtual std::vector<KeyValueInfo> topKeyValues(uint32_t k) = 0;
public:
	virtual void apply(const std::shared_ptr<KeyExclusions> & e) = 0;
	virtual void apply(const std::shared_ptr<KeyValueExclusions> & e) = 0;
protected:
	Interface() {}
};

}}//end namespace liboscar::kvclustering

#endif
