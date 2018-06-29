#ifndef LIBOSCAR_OSM_COMPLETER_H
#define LIBOSCAR_OSM_COMPLETER_H
#include <string>
#include <ostream>
#include <unordered_map>
#include <liboscar/constants.h>
#include <liboscar/StaticOsmItemSet.h>
#include <liboscar/tagcompleters.h>
#include <liboscar/TextSearch.h>
#include <liboscar/GeoSearch.h>
#include <sserialize/spatial/CellDistance.h>
#include <sserialize/Static/CellTextCompleter.h>
#include <sserialize/search/GeoCompleter.h>
#include <sserialize/spatial/GeoConstraintSetOpTreeEF.h>
#include <sserialize/Static/GeoHierarchySubGraph.h>

namespace liboscar {
namespace Static {

class OsmCompleter {
protected:
	template<typename TCompleter>
	class GeoCompleterOp: public sserialize::spatial::GeoConstraintSetOpTreeSF<TCompleter> {
	public:
		typedef sserialize::spatial::GeoConstraintSetOpTreeSF<TCompleter> MyBaseClass;
	public:
		GeoCompleterOp(const TCompleter & completer) : MyBaseClass(completer) {}
		virtual ~GeoCompleterOp() {}
		virtual std::string describe() const { return MyBaseClass::completer().getName(); }
	};
private:
	std::string m_filesDir;
	std::unordered_map<uint32_t, sserialize::UByteArrayAdapter> m_data;
	liboscar::Static::OsmKeyValueObjectStore m_store;
	sserialize::Static::ItemIndexStore m_indexStore;
	TextSearch m_textSearch;
	GeoSearch m_geoSearch;
	std::vector<sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter> > m_geoCompleters;
	///maps from TextSearch::Type->(Position, StringCompleter)
	uint8_t m_selectedGeoCompleter;
	sserialize::spatial::GeoHierarchySubGraph m_ghsg;
	std::shared_ptr<sserialize::spatial::interface::CellDistance> m_cellDistance;
	
private:
	sserialize::RCPtrWrapper<TagCompleter> m_tagCompleter;
	sserialize::RCPtrWrapper<TagNameCompleter> m_tagNameCompleter;
	sserialize::RCPtrWrapper<TagPhraseCompleter> m_tagPhraseCompleter;
	
private:
	template<typename T_ITEM_SET_TYPE>
	void registerFilters(T_ITEM_SET_TYPE & itemSet) {
		itemSet.registerSelectableOpFilter(geoCompleter().priv());
		itemSet.registerSelectableOpFilter(m_tagCompleter.priv());
		itemSet.registerSelectableOpFilter( m_tagNameCompleter.priv() );
		itemSet.registerSelectableOpFilter( m_tagPhraseCompleter.priv() );
	}
	sserialize::StringCompleter getItemsCompleter() const;
public:
	typedef enum {CDT_CENTER_OF_MASS, CDT_ANULUS, CDT_MIN_SPHERE, CDT_SPHERE} CellDistanceType;
public:
	OsmCompleter();
	virtual ~OsmCompleter();
#ifdef LIBOSCAR_NO_DATA_REFCOUNTING
	OsmCompleter(const OsmCompleter & other) = delete;
	OsmCompleter & operator=(const OsmCompleter & other) = delete;
#endif
	sserialize::UByteArrayAdapter data(liboscar::FileConfig fc) const;
	bool setAllFilesFromPrefix(const std::string & fileName);
	///throws an exception if something goes wrong
	void energize(sserialize::spatial::GeoHierarchySubGraph::Type ghsgType = sserialize::spatial::GeoHierarchySubGraph::T_INVALID);

	inline const TextSearch & textSearch() const { return m_textSearch; }
	inline const GeoSearch & geoSearch() const { return m_geoSearch; }
	inline const sserialize::spatial::GeoHierarchySubGraph & ghsg() const { return m_ghsg; }
	inline const std::vector<sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter> > & geoCompleters() const { return m_geoCompleters; }
	inline const liboscar::Static::OsmKeyValueObjectStore & store() const { return m_store; }
	inline const sserialize::Static::ItemIndexStore & indexStore() const { return m_indexStore; }
	inline const sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter> & geoCompleter() const { return m_geoCompleters.at(m_selectedGeoCompleter); }
	inline sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter> & geoCompleter() { return m_geoCompleters.at(m_selectedGeoCompleter); }
	
	bool setTextSearcher(TextSearch::Type t, uint8_t pos);
	bool setGeoCompleter(uint8_t pos);
	bool setCellDistance(CellDistanceType cdt);
	
	inline uint8_t selectedGeoCompleter() { return m_selectedGeoCompleter; }
	inline uint8_t selectedTextSearcher(TextSearch::Type t) { return m_textSearch.selectedTextSearcher(t); }
	
public: //Stats
	std::ostream & printStats(std::ostream& out) const;
	
public:
	Static::OsmItemSet complete(const std::string& query);
	//Do a simple result set size limited search (this currently only supports string queries)
	Static::OsmItemSet simpleComplete(const std::string & query, uint32_t maxResultSetSize, uint32_t minStrLen);
	Static::OsmItemSet simpleComplete(const std::string & query, uint32_t maxResultSetSize, uint32_t minStrLen, const sserialize::spatial::GeoRect & rect);
	Static::OsmItemSetIterator partialComplete(const std::string& query, const sserialize::spatial::GeoRect & rect = sserialize::spatial::GeoRect());
	sserialize::CellQueryResult cqrComplete(const std::string & query, const sserialize::spatial::GeoHierarchySubGraph & ghsg, bool treedCQR = false, uint32_t threadCount = 1);
	///@param threadCount: the number of threads used to flatten a TreedCQR
	sserialize::CellQueryResult cqrComplete(const std::string & query, bool treedCQR = false, uint32_t threadCount = 1);
	sserialize::Static::spatial::GeoHierarchy::SubSet clusteredComplete(const std::string& query, const sserialize::spatial::GeoHierarchySubGraph & ghs, uint32_t minCq4SparseSubSet, bool treedCQR = false, uint32_t threadCount = 1);
	sserialize::Static::spatial::GeoHierarchy::SubSet clusteredComplete(const std::string& query, uint32_t minCq4SparseSubSet, bool treedCQR = false, uint32_t threadCount = 1);
	sserialize::Static::spatial::GeoHierarchy::SubSet clusteredComplete(const std::string & query);
	Static::TagStore tagStore() const;
};

}}//end namespace

#endif
