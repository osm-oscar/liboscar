#include <liboscar/StaticOsmCompleter.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <istream>
#include <fstream>
#include <liboscar/constants.h>
#include <liboscar/SetOpTreePrivateGeo.h>
#include <liboscar/tagcompleters.h>
#include <liboscar/CellOpTree.h>
#include <liboscar/AdvancedCellOpTree.h>
#include <sserialize/search/StringCompleterPrivateMulti.h>
#include <sserialize/search/StringCompleterPrivateGeoHierarchyUnclustered.h>
#include <sserialize/Static/StringCompleter.h>
#include <sserialize/Static/GeoCompleter.h>
#include <sserialize/storage/MmappedFile.h>
#include <sserialize/stats/TimeMeasuerer.h>
#ifdef __ANDROID__
	#define MEMORY_BASED_SUBSET_CREATOR_MIN_CELL_COUNT static_cast<uint32_t>(0xFFFFFFFFF)
#else
	#define MEMORY_BASED_SUBSET_CREATOR_MIN_CELL_COUNT static_cast<uint32_t>(100)
#endif

namespace liboscar {
namespace Static {

std::ostream & OsmCompleter::printStats(std::ostream & out) const {
	m_tagCompleter->tagStore().printStats(out);
	return out;
}

OsmCompleter::OsmCompleter() :
m_selectedGeoCompleter(0)
{}

OsmCompleter::~OsmCompleter() {
#ifdef LIBOSCAR_NO_DATA_REFCOUNTING
	for(auto & d : m_data) {
		d.second.enableRefCounting();
	}
	m_store.enableRefCounting();
#endif
}

sserialize::UByteArrayAdapter OsmCompleter::data(FileConfig fc) const {
	if (m_data.count(fc)) {
		return m_data.at(fc);
	}
	return sserialize::UByteArrayAdapter();
}

bool OsmCompleter::setAllFilesFromPrefix(const std::string & filePrefix) {
	if (sserialize::MmappedFile::isDirectory(filePrefix)) {
		m_filesDir = filePrefix;
		return true;
	}
	return false;
}

bool OsmCompleter::setGeoCompleter(uint8_t pos) {
	if (pos < m_geoCompleters.size()) {
		m_selectedGeoCompleter = pos;
		return true;
	}
	return false;
}

bool OsmCompleter::setTextSearcher(TextSearch::Type t, uint8_t pos) {
	return m_textSearch.select(t, pos);
}

sserialize::StringCompleter OsmCompleter::getItemsCompleter() const {
	sserialize::StringCompleter strCmp;
	if (m_data.count(FC_TAGSTORE)) {
		sserialize::StringCompleterPrivateMulti * myscmp = new sserialize::StringCompleterPrivateMulti();
		myscmp->addCompleter( sserialize::RCPtrWrapper<sserialize::StringCompleterPrivate>(textSearch().get<liboscar::TextSearch::ITEMS>().getPrivate()) );
		myscmp->addCompleter( sserialize::RCPtrWrapper<sserialize::StringCompleterPrivate>(new StringCompleterPrivateTagPhrase(m_tagPhraseCompleter)) );
		strCmp = sserialize::StringCompleter(myscmp);
	}
	else {
		strCmp = textSearch().get<liboscar::TextSearch::Type::ITEMS>();
	}
	return strCmp;
}


OsmItemSet OsmCompleter::complete(const std::string & query) {
	OsmItemSet itemSet(query, getItemsCompleter(), store(), sserialize::SetOpTree::SOT_COMPLEX);
	registerFilters(itemSet);
	itemSet.execute();
	return itemSet;
}

OsmItemSet OsmCompleter::simpleComplete(const std::string & query, uint32_t maxResultSetSize, uint32_t minStrLen) {
	OsmItemSet itemSet(query, getItemsCompleter(), store(), sserialize::SetOpTree::SOT_SIMPLE);
	registerFilters(itemSet);
	itemSet.setMaxResultSetSize(maxResultSetSize);
	itemSet.setMinStrLen(minStrLen);
	itemSet.execute();
	return itemSet;
}

OsmItemSet OsmCompleter::simpleComplete(const std::string & query, uint32_t maxResultSetSize, uint32_t minStrLen, const sserialize::spatial::GeoRect & rect) {
	sserialize::StringCompleter strCmp(getItemsCompleter());
	std::shared_ptr<sserialize::ItemIndex::ItemFilter> geoFilter(new GeoConstraintFilter<liboscar::Static::OsmKeyValueObjectStore>(store(), rect));
	if (rect.length() < 0.1) {
		OsmItemSet itemSet(sserialize::toString(query," $GEO[",rect.minLat(),";",rect.maxLat(),";",rect.minLon(),";",rect.maxLon(),";]"), store(), sserialize::SetOpTree(new SetOpTreePrivateGeo(geoFilter)));
		itemSet.registerStringCompleter(strCmp);
		registerFilters(itemSet);
		itemSet.setMaxResultSetSize(maxResultSetSize);
		itemSet.setMinStrLen(minStrLen);
		itemSet.execute();
		return itemSet;
	}
	else {
		OsmItemSet itemSet(query, store(), sserialize::SetOpTree(new SetOpTreePrivateGeo(geoFilter)));
		itemSet.registerStringCompleter(strCmp);
		registerFilters(itemSet);
		itemSet.setMaxResultSetSize(maxResultSetSize);
		itemSet.setMinStrLen(minStrLen);
		itemSet.execute();
		return itemSet;
	}
}

OsmItemSetIterator OsmCompleter::partialComplete(const std::string & query, const sserialize::spatial::GeoRect & rect) {
	sserialize::StringCompleter strCmp(getItemsCompleter());
	std::shared_ptr<sserialize::ItemIndex::ItemFilter> geoFilter(new GeoConstraintFilter<liboscar::Static::OsmKeyValueObjectStore>(store(), rect));
	if (rect.length() < 0.1) {
		OsmItemSetIterator itemSet(sserialize::toString(query," $GEO[",rect.minLat(),";",rect.maxLat(),";",rect.minLon(),";",rect.maxLon(),";]"), store(), sserialize::SetOpTree::SOT_COMPLEX, geoFilter);
		itemSet.registerStringCompleter(strCmp);
		registerFilters(itemSet);
		itemSet.execute();
		return itemSet;
	}
	else {
		OsmItemSetIterator itemSet(query, store(), sserialize::SetOpTree::SOT_COMPLEX, geoFilter);
		itemSet.registerStringCompleter(strCmp);
		registerFilters(itemSet);
		itemSet.execute();
		return itemSet;
	}
}

void OsmCompleter::energize(sserialize::spatial::GeoHierarchySubGraph::Type ghsgType) {
	#ifdef __LP64__
	//use up to 1 TiB of address space on 64 Bit machines
	uint64_t maxFullMmapSize = uint64_t(1024*1024)*uint64_t(1024*1024);
	#else
	//use up to 1 GiB of address space on 32 Bit machines
	uint64_t maxFullMmapSize = 1024*1024*1024; 
	#endif
	
	for(uint32_t i = FC_BEGIN; i < FC_END; ++i) {
		bool cmp;
		std::string fn;
		if (fileNameFromPrefix(m_filesDir, (FileConfig)i, fn, cmp)) {
			uint64_t fileSize = sserialize::MmappedFile::fileSize(fn);
			
			if (fileSize >= maxFullMmapSize) {
				m_data[i] = sserialize::UByteArrayAdapter::openRo(fn, cmp, 0, 0);
			}
			else {
				m_data[i] = sserialize::UByteArrayAdapter::openRo(fn, cmp, maxFullMmapSize, 0);
				maxFullMmapSize -= fileSize;
			}
		}
	}

#ifdef LIBOSCAR_NO_DATA_REFCOUNTING
	for(auto & d : m_data) {
		d.second.disableRefCounting();
	}
#endif
	
	bool haveNeededData = m_data.count(FC_KV_STORE);
	if (haveNeededData) {
		try {
			m_store = liboscar::Static::OsmKeyValueObjectStore(m_data[FC_KV_STORE]);
		}
		catch( sserialize::Exception & e) {
			sserialize::err("Static::OsmCompleter", std::string("Failed to initialize kvstore with the following error:\n") + std::string(e.what()));
			haveNeededData = false;
		}
	}
	if (!haveNeededData) {
		throw sserialize::MissingDataException("OsmCompleter needs a KeyValueStore");
	}
	
#ifdef LIBOSCAR_NO_DATA_REFCOUNTING
	m_store.disableRefCounting();
#endif
	
	m_geoCompleters.push_back(
		sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter>(
			new sserialize::spatial::GeoConstraintSetOpTreeSF<sserialize::GeoCompleter>(
				sserialize::Static::GeoCompleter::fromDB(m_store)
			)
		)
	);
	
	haveNeededData = m_data.count(FC_INDEX);
	if (haveNeededData) {
		try {
			m_indexStore = sserialize::Static::ItemIndexStore(m_data[FC_INDEX]);
		}
		catch ( sserialize::Exception & e) {
			sserialize::err("liboscar::Static::OsmCompleter", std::string("Failed to initialize index with the following error:\n") + e.what());
			haveNeededData = false;
		}
	}
	if(!haveNeededData) {
		std::cout << "OsmCompleter: No index available" << std::endl;
		return;
	}

	if (m_data.count(FC_TEXT_SEARCH)) {
		try {
			m_textSearch = liboscar::TextSearch(m_data[FC_TEXT_SEARCH], m_indexStore, m_store.geoHierarchy(), m_store.regionArrangement());
		}
		catch (sserialize::Exception & e) {
			sserialize::err("liboscar::Static::OsmCompleter", std::string("Failed to initialize textsearch with the following error:\n") + e.what());
		}
	}
	
	if (m_data.count(FC_GEO_SEARCH)) {
		try {
			m_geoSearch = liboscar::GeoSearch(m_data[FC_GEO_SEARCH], m_indexStore, m_store);
			if (m_geoSearch.hasSearch(liboscar::GeoSearch::ITEMS)) {
				for(const auto & x : m_geoSearch.get(liboscar::GeoSearch::ITEMS)) {
					m_geoCompleters.push_back(
						sserialize::RCPtrWrapper<sserialize::SetOpTree::SelectableOpFilter>(
							new sserialize::spatial::GeoConstraintSetOpTreeSF<sserialize::GeoCompleter>(x)
						)
					);
				}
			}
		}
		catch (sserialize::Exception & e) {
			sserialize::err("liboscar::Static::OsmCompleter", std::string("Failed to initialize geosearch with the following error:\n") + e.what());
		}
	}
	
	if (m_data.count(FC_TAGSTORE)) {
		try {
			TagStore tagStore(m_data[FC_TAGSTORE], m_indexStore);
			m_tagCompleter = sserialize::RCPtrWrapper<TagCompleter>( new TagCompleter(tagStore) );
			m_tagNameCompleter = sserialize::RCPtrWrapper<TagNameCompleter>( new TagNameCompleter(tagStore) );
			std::string tagStorePhrasesFn;
			bool cmp;
			if (fileNameFromPrefix(m_filesDir, FC_TAGSTORE_PHRASES, tagStorePhrasesFn, cmp) && !cmp) {
				std::ifstream iFile;
				iFile.open(tagStorePhrasesFn);
				m_tagPhraseCompleter = sserialize::RCPtrWrapper<TagPhraseCompleter>( new TagPhraseCompleter(tagStore, iFile) );
				iFile.close();
			}
			else {
				m_tagPhraseCompleter = sserialize::RCPtrWrapper<TagPhraseCompleter>( new TagPhraseCompleter() );
			}
		}
		catch (sserialize::Exception & e) {
			sserialize::err("liboscar::Static::OsmCompleter", std::string("Failed to initialize tagstore with the following error:\n") + e.what());
		}
	}
	else {
		m_tagCompleter = sserialize::RCPtrWrapper<TagCompleter>( new TagCompleter() );
		m_tagNameCompleter = sserialize::RCPtrWrapper<TagNameCompleter>( new TagNameCompleter() );
		m_tagPhraseCompleter = sserialize::RCPtrWrapper<TagPhraseCompleter>( new TagPhraseCompleter() );
	}

	if (ghsgType == sserialize::spatial::GeoHierarchySubGraph::T_INVALID && m_store.geoHierarchy().cellSize() >= MEMORY_BASED_SUBSET_CREATOR_MIN_CELL_COUNT) {
		ghsgType = sserialize::spatial::GeoHierarchySubGraph::T_IN_MEMORY;
	}
	m_ghsg = sserialize::spatial::GeoHierarchySubGraph(m_store.geoHierarchy(), indexStore(), ghsgType);
}

void processCompletionToken(std::string & q, sserialize::StringCompleter::QuerryType & qt) {
	qt = sserialize::StringCompleter::normalize(q);
}

inline std::ostream & operator<<(std::ostream & out, const liboscar::Static::OsmKeyValueObjectStore::Item & item) {
	item.print(out, false);
	return out;
}

sserialize::CellQueryResult
OsmCompleter::cqrComplete(
	const std::string& query,
	const sserialize::spatial::GeoHierarchySubGraph & ghsg,
	bool treedCQR,
	uint32_t threadCount)
{
	if (!m_textSearch.hasSearch(liboscar::TextSearch::Type::GEOCELL)) {
		throw sserialize::UnsupportedFeatureException("OsmCompleter::cqrComplete data has no CellTextCompleter");
	}
	sserialize::Static::CellTextCompleter cmp( m_textSearch.get<liboscar::TextSearch::Type::GEOCELL>() );
	sserialize::Static::CQRDilator cqrd(store().cellCenterOfMass(), store().cellGraph());
	CQRFromPolygon cqrfp(store(), indexStore());
	CQRFromComplexSpatialQuery csq(ghsg, cqrfp);
	if (!treedCQR) {
		AdvancedCellOpTree opTree(cmp, cqrd, csq, ghsg);
		opTree.parse(query);
		return opTree.calc<sserialize::CellQueryResult>();
	}
	else {
		AdvancedCellOpTree opTree(cmp, cqrd, csq, ghsg);
		opTree.parse(query);
		return opTree.calc<sserialize::TreedCellQueryResult>(threadCount).toCQR(threadCount);
	}
}

sserialize::CellQueryResult
OsmCompleter::cqrComplete(
	const std::string& query,
	bool treedCQR,
	uint32_t threadCount)
{
	return this->cqrComplete(query, m_ghsg, treedCQR, threadCount);
}

sserialize::Static::spatial::GeoHierarchy::SubSet
OsmCompleter::clusteredComplete(
	const std::string& query,
	const sserialize::spatial::GeoHierarchySubGraph & ghsg,
	uint32_t minCq4SparseSubSet,
	bool treedCQR,
	uint32_t threadCount)
{
	sserialize::CellQueryResult r = cqrComplete(query, ghsg, treedCQR, threadCount);
	return ghsg.subSet(r, r.cellCount() > minCq4SparseSubSet, threadCount);
}

sserialize::Static::spatial::GeoHierarchy::SubSet
OsmCompleter::clusteredComplete(
	const std::string& query,
	uint32_t minCq4SparseSubSet,
	bool treedCQR,
	uint32_t threadCount)
{
	return this->clusteredComplete(query, m_ghsg, minCq4SparseSubSet, treedCQR, threadCount);
}

TagStore OsmCompleter::tagStore() const {
	if (m_tagCompleter.priv()) {
		return m_tagCompleter->tagStore();
	}
	return TagStore();
}

}}//end namespace