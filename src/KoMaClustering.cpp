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
	}
	// sort all keyValues descending by itemCount to find the top KeyValues faster
	std::sort(keyValueCountVec.begin(), keyValueCountVec.end(),
			  [](std::pair<KeyValue, std::uint32_t> const &a,
				 std::pair<KeyValue, std::uint32_t> const &b) {
				  return a.second != b.second ? a.second > b.second : a.first < b.first;
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
				if(state->keyExclusions.contains(item.keyId(i)))
					continue;
				if(state->keyValueExclusions.contains(item.keyId(i),item.valueId(i)))
					continue;
				state->d.update(std::make_pair(item.keyId(i), item.valueId(i)), x);
			}
		}
	}
	std::cout << state->d.keyValueCountVec.size();
}


} //end namespace KoMaClustering
} //end namespace detail

KoMaClustering::KoMaClustering(const Static::OsmKeyValueObjectStore &store,
							   const sserialize::CellQueryResult &cqr,
							   kvclustering::KeyExclusions &keyExclusions,
							   kvclustering::KeyValueExclusions &keyValueExclusions) :
		store(store), cqr(cqr), keyExclusions(keyExclusions), keyValueExclusions(keyValueExclusions)
{}
} //end namespace liboscar

