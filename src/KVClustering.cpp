#include <liboscar/KVClustering.h>
#include <algorithm>

namespace liboscar {
namespace kvclustering {

KeyExclusions::KeyRange::KeyRange(uint32_t begin, uint32_t end) :
begin(begin),
end(end)
{
	SSERIALIZE_CHEAP_ASSERT_SMALLER_OR_EQUAL(begin, end);
}

bool
KeyExclusions::KeyRange::contains(uint32_t keyId) const {
	return begin <= keyId && end > keyId;
}

bool
KeyExclusions::KeyRange::overlap(const KeyRange & other) const {
	return contains(other.begin) || other.contains(begin);
}

KeyExclusions::KeyRange
KeyExclusions::KeyRange::merge(const KeyRange & other) const {
	SSERIALIZE_CHEAP_ASSERT(overlap(other));
	return KeyRange(std::min(begin, other.begin), std::max(end, other.end));
}

KeyExclusions::KeyExclusions(const KeyStringTable & kst) :
m_kst(kst)
{}
KeyExclusions::~KeyExclusions()
{}

void
KeyExclusions::add(const std::string & key) {
	auto keyId = m_kst.find(key);
	if (keyId != m_kst.npos) {
		add(keyId);
	}
}

void
KeyExclusions::add(uint32_t keyId) {
	m_keyRange.emplace_back(keyId, keyId+1);
	m_valid = false;
}

void
KeyExclusions::addPrefix(const std::string & keyprefix) {
	auto range = m_kst.range(keyprefix);
	if (range.second - range.first > 0) {
		addPrefix(range.first, range.second);
	}
}

void
KeyExclusions::addPrefix(uint32_t keyIdBegin, uint32_t keyIdEnd) {
	m_keyRange.emplace_back(keyIdBegin, keyIdEnd);
	m_valid = false;
}

void
KeyExclusions::preprocess() {
	if (!m_keyRange.size()) {
		m_valid = true;
		return;
	}
	
	std::sort(m_keyRange.begin(), m_keyRange.end(),
		[](const KeyRange & a, const KeyRange & b) {
			return a.begin < b.begin;
		}
	);
	
	//m_keyRange is now sorted according to begin of each range
	//overlapping ranges are hence in a contigous region
	//we now simply merge them into a single large range
	
	std::vector<KeyRange> tmp;
	auto it = m_keyRange.begin();
	
	tmp.emplace_back(*it);
	++it;
	for(auto end(m_keyRange.end()); it != end; ++it) {
		if (tmp.back().overlap(*it)) {
			tmp.back() = tmp.back().merge(*it);
		}
		else {
			tmp.emplace_back(*it);
		}
	}
	m_keyRange = std::move(tmp);
	m_valid = true;
}

bool
KeyExclusions::hasExceptions() const {
	return m_keyRange.size();
}

bool
KeyExclusions::contains(uint32_t keyId) const {
	if (!m_valid) {
		throw sserialize::InvalidAlgorithmStateException("ExceptionList: needs preprocessing");
	}
	auto cmp = [](const KeyRange & kr, uint32_t keyId) {
		return kr.end < keyId;
	};
	auto lb = std::lower_bound(m_keyRange.begin(), m_keyRange.end(), keyId, cmp);
	return lb != m_keyRange.end() && lb->contains(keyId);
}

KeyValueExclusions::KeyValueExclusions(const KeyStringTable & kst, const ValueStringTable & vst) :
m_kst(kst),
m_vst(vst)
{}

KeyValueExclusions::~KeyValueExclusions()
{}

void
KeyValueExclusions::add(const std::string & key, const std::string & value) {
	auto keyId = m_kst.find(key);
	if (keyId == m_kst.npos) {
		return;
	}
	auto valueId = m_vst.find(value);
	if (valueId == m_vst.npos) {
		return;
	}
	add(keyId, valueId);
}

void
KeyValueExclusions::add(uint32_t keyId, uint32_t valueId) {
	m_keyValues.emplace(keyId, valueId);
}

bool
KeyValueExclusions::hasExceptions() const {
	return m_keyValues.size();
}

bool
KeyValueExclusions::contains(uint32_t keyId, uint32_t valueId) const {
	return m_keyValues.count(std::make_pair(keyId, valueId));
}
	
}} //end namespace liboscar::kvclustering
