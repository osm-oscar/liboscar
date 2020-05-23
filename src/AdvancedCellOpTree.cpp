#include <liboscar/AdvancedCellOpTree.h>

namespace liboscar {

AdvancedCellOpTree::
AdvancedCellOpTree(
	const sserialize::Static::CellTextCompleter & ctc,
	const sserialize::Static::CQRDilator & cqrd,
	const CQRFromComplexSpatialQuery & csq,
	const sserialize::spatial::GeoHierarchySubGraph & ghsg) :
AdvancedOpTree(),
m_ctc(ctc),
m_cqrd(cqrd),
m_csq(csq),
m_ghsg(ghsg)
{}

AdvancedCellOpTree::~AdvancedCellOpTree() {}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcBetweenOp(const sserialize::CellQueryResult& c1, const sserialize::CellQueryResult& c2) {
	sserialize::CellQueryResult result;
#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
	result = m_csq.betweenOp(c1, c2, 1);
	for(const sserialize::ItemIndex & x : result) {
		SSERIALIZE_EXPENSIVE_ASSERT(std::is_sorted(x.begin(), x.end()));
		SSERIALIZE_EXPENSIVE_ASSERT(sserialize::is_strong_monotone_ascending(x.begin(), x.end()));
		for(uint32_t y : x) {
			SSERIALIZE_EXPENSIVE_ASSERT_NOT_EQUAL(y, uint32_t(16800296));
		}
	}
	sserialize::ItemIndex tmp2 =  result.flaten();
	for(uint32_t y : tmp2) {
		SSERIALIZE_EXPENSIVE_ASSERT_NOT_EQUAL(y, uint32_t(16800296));
	}
#else
	result = m_csq.betweenOp(c1, c2, m_threadCount);
#endif
	int resultFlags = sserialize::CellQueryResult::FF_NONE;
	if ((c1.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS) != (c2.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS)) {
		resultFlags = m_ctc.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS;
	}
	else {
		resultFlags = c1.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS;
	}
	if ((result.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS) != resultFlags) {
		result = result.convert(resultFlags);
	}
	return result;
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcCompassOp(liboscar::AdvancedCellOpTree::Node* node, const sserialize::CellQueryResult& cqr) {
	CQRFromComplexSpatialQuery::UnaryOp direction = CQRFromComplexSpatialQuery::UO_INVALID;
	if (node->value == "^" || node->value == "north-of") {
		direction = CQRFromComplexSpatialQuery::UO_NORTH_OF;
	}
	else if (node->value == ">" || node->value == "east-of") {
		direction = CQRFromComplexSpatialQuery::UO_EAST_OF;
	}
	else if (node->value == "v" || node->value == "south-of") {
		direction = CQRFromComplexSpatialQuery::UO_SOUTH_OF;
	}
	else if (node->value == "<" || node->value == "west-of") {
		direction = CQRFromComplexSpatialQuery::UO_WEST_OF;
	}
	return m_csq.compassOp(cqr, direction, m_threadCount);
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcRelevantElementOp(liboscar::AdvancedCellOpTree::Node* SSERIALIZE_CHEAP_ASSERT_EXEC(node), const sserialize::CellQueryResult& cqr) {
	SSERIALIZE_CHEAP_ASSERT(node && node->value == "*");
	return m_csq.relevantElementOp(cqr);
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::calcInOp(Node *, const sserialize::CellQueryResult & cqr) {
	return sserialize::CellQueryResult(
		calcDilateRegionByItemCoverageOp(0.9, cqr),
		cqr.cellInfo(),
		cqr.idxStore(),
		cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
	);
}

sserialize::ItemIndex AdvancedCellOpTree::CalcBase::calcDilateRegionByCellCoverageOp(double th, const sserialize::CellQueryResult & cqr) {
	const sserialize::Static::spatial::GeoHierarchy & gh = m_ctc.geoHierarchy();
	const sserialize::Static::ItemIndexStore & idxStore = this->idxStore();

	std::vector<uint32_t> regions;

	std::unordered_map<uint32_t, uint32_t> r2cc;
	
	for(auto it(cqr.cbegin()), end(cqr.cend()); it != end; ++it) {
		uint32_t cellId = it.cellId();
		for(uint32_t cP(gh.cellParentsBegin(cellId)), cE(gh.cellParentsEnd(cellId)); cP != cE; ++cP) {
			uint32_t rId = gh.cellPtr(cP);
			r2cc[rId] += 1; //default init is 0 for uint32_t
		}
	}
	for(auto x : r2cc) {
		if ( (double)x.second / (double)idxStore.idxSize( gh.regionCellIdxPtr(x.first) ) > th) {
			regions.push_back(x.first);
		}
	}
	
	//regions need to be unique
	sserialize::ItemIndex res = sserialize::treeReduceMap<std::vector<uint32_t>::iterator, sserialize::ItemIndex>(regions.begin(), regions.end(),
		[](const sserialize::ItemIndex & a, const sserialize::ItemIndex & b) { return a+b; },
		[&gh, &idxStore](uint32_t x) { return idxStore.at( gh.regionCellIdxPtr(x) ); }
	);
	return res;
}

sserialize::ItemIndex AdvancedCellOpTree::CalcBase::calcDilateRegionByCellCoverageOp(AdvancedCellOpTree::Node * node, const sserialize::CellQueryResult & cqr) {
	double th = sserialize::stod(node->value.c_str());
	if (th <= 0.0) {
		return sserialize::ItemIndex();
	}
	th /= 100.0;
	
	return calcDilateRegionByCellCoverageOp(th, cqr);
}

sserialize::ItemIndex AdvancedCellOpTree::CalcBase::calcDilateRegionByItemCoverageOp(double th, const sserialize::CellQueryResult & cqr) {
	//Note that we may overshoot the number of items in a region
	//since we may count items multiple times if they are in multiple cells of the same region

	const sserialize::Static::spatial::GeoHierarchy & gh = m_ctc.geoHierarchy();
	const sserialize::Static::ItemIndexStore & idxStore = this->idxStore();

	std::vector<uint32_t> regions;

	std::unordered_map<uint32_t, uint32_t> r2cc;
	
	for(auto it(cqr.cbegin()), end(cqr.cend()); it != end; ++it) {
		uint32_t cellId = it.cellId();
		uint32_t cellIdxSize = it.idxSize();
		for(uint32_t cP(gh.cellParentsBegin(cellId)), cE(gh.cellParentsEnd(cellId)); cP != cE; ++cP) {
			uint32_t rId = gh.cellPtr(cP);
			r2cc[rId] +=  cellIdxSize; //default init is 0 for uint32_t
		}
	}
	for(auto x : r2cc) {
		if ( (double(x.second) / gh.regionItemsCount(x.first)) > th) {
			regions.push_back(x.first);
		}
	}
	
	//regions need to be unique
	sserialize::ItemIndex res = sserialize::treeReduceMap<std::vector<uint32_t>::iterator, sserialize::ItemIndex>(regions.begin(), regions.end(),
		[](const sserialize::ItemIndex & a, const sserialize::ItemIndex & b) { return a+b; },
		[&gh, &idxStore](uint32_t x) { return idxStore.at( gh.regionCellIdxPtr(x) ); }
	);
	return res;
}

sserialize::ItemIndex AdvancedCellOpTree::CalcBase::calcDilateRegionByItemCoverageOp(AdvancedCellOpTree::Node * node, const sserialize::CellQueryResult & cqr) {
	double th = sserialize::stod(node->value.c_str());
	if (th <= 0.0) {
		return sserialize::ItemIndex();
	}
	th /= 100.0;
	
	return calcDilateRegionByItemCoverageOp(th, cqr);
}

std::vector<double> AdvancedCellOpTree::CalcBase::asDoubles(const std::string & str) {
	std::vector<double> tmp;
	{
		struct MyOut {
			std::vector<double> * dest;
			MyOut & operator++() { return *this; }
			MyOut & operator*() { return *this; }
			MyOut & operator=(const std::string & str) {
				try {
					double t = sserialize::stod(str);
					dest->push_back(t);
				}
				catch (std::invalid_argument & e) {}
				return *this;
			}
			MyOut(std::vector<double> * d) : dest(d) {}
		};
		typedef sserialize::OneValueSet<uint32_t> MyS;
		sserialize::split<std::string::const_iterator, MyS, MyS, MyOut>(str.begin(), str.end(), MyS(','), MyS('\\'), MyOut(&tmp));
	}
	return tmp;
}

const sserialize::CellQueryResult::CellInfo & AdvancedCellOpTree::CalcBase::ci() const {
	return m_ctc.cellInfo();
}

const sserialize::Static::spatial::GeoHierarchy& AdvancedCellOpTree::CalcBase::gh() const {
	return m_ctc.geoHierarchy();
}

const sserialize::Static::ItemIndexStore& AdvancedCellOpTree::CalcBase::idxStore() const {
	return m_ctc.idxStore();
}

const liboscar::Static::OsmKeyValueObjectStore & AdvancedCellOpTree::CalcBase::store() const {
	return m_csq.cqrfp().store();
}

const sserialize::spatial::GeoHierarchySubGraph & AdvancedCellOpTree::CalcBase::ghsg() const {
	return m_ghsg;
}

sserialize::Static::CellTextCompleter & AdvancedCellOpTree::CalcBase::ctc() {
		return m_ctc;
}

uint32_t AdvancedCellOpTree::CalcBase::threadCount() const {
	return m_threadCount;
}

sserialize::CellQueryResult AdvancedCellOpTree::CalcBase::toCQR(const sserialize::TreedCellQueryResult & cqr) const {
	return cqr.toCQR( this->threadCount() );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node) {
	double diameter = sserialize::stod(node->value.c_str())*1000;
	sserialize::CellQueryResult cqr( calc(node->children.front()) );
	return cqr +
		sserialize::CellQueryResult(
									m_cqrd.dilate(cqr, diameter, m_threadCount),
									cqr.cellInfo(),
									cqr.idxStore(),
									cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
									);
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node) {
	double diameter = sserialize::stod(node->value.c_str())*1000;
	sserialize::TreedCellQueryResult cqr( calc(node->children.front()) );
	return cqr +
		sserialize::TreedCellQueryResult(
										m_cqrd.dilate(toCQR(cqr), diameter, m_threadCount),
										cqr.cellInfo(),
										cqr.idxStore(),
										cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
										);
}


template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRegionDilationByCellCoverageOp(AdvancedCellOpTree::Node* node) {
	sserialize::CellQueryResult cqr( calc(node->children.front()) );
	return sserialize::CellQueryResult(
										CalcBase::calcDilateRegionByCellCoverageOp(node, cqr),
										cqr.cellInfo(),
										cqr.idxStore(),
										cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
									  );
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRegionDilationByCellCoverageOp(AdvancedCellOpTree::Node* node) {
	sserialize::TreedCellQueryResult cqr( calc(node->children.front()) );
	return sserialize::TreedCellQueryResult(
											CalcBase::calcDilateRegionByCellCoverageOp(node, cqr.toCQR()),
											cqr.cellInfo(),
											cqr.idxStore(),
											cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
											);
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRegionDilationByItemCoverageOp(AdvancedCellOpTree::Node* node) {
	sserialize::CellQueryResult cqr( calc(node->children.front()) );
	return sserialize::CellQueryResult(
										CalcBase::calcDilateRegionByItemCoverageOp(node, cqr),
										cqr.cellInfo(),
										cqr.idxStore(),
										cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
									  );
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRegionDilationByItemCoverageOp(AdvancedCellOpTree::Node* node) {
	sserialize::TreedCellQueryResult cqr( calc(node->children.front()) );
	return sserialize::TreedCellQueryResult(
											CalcBase::calcDilateRegionByItemCoverageOp(node, cqr.toCQR()),
											cqr.cellInfo(),
											cqr.idxStore(),
											cqr.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS
											);
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 2);
	return CalcBase::calcBetweenOp(calc(node->children.front()), calc(node->children.back()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 2);
	return sserialize::TreedCellQueryResult( CalcBase::calcBetweenOp(toCQR(calc(node->children.front())), toCQR(calc(node->children.back()))) );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return CalcBase::calcCompassOp(node, calc(node->children.front()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return sserialize::TreedCellQueryResult( CalcBase::calcCompassOp(node, toCQR(calc(node->children.front()))) );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcInOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return CalcBase::calcInOp(node, calc(node->children.front()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcInOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return sserialize::TreedCellQueryResult( CalcBase::calcInOp(node, toCQR(calc(node->children.front()))) );
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return CalcBase::calcRelevantElementOp(node, calc(node->children.front()));
}

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return sserialize::TreedCellQueryResult( CalcBase::calcRelevantElementOp(node, toCQR(calc(node->children.front()))) );
}


}//end namespace
