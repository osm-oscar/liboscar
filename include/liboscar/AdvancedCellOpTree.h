#ifndef LIBOSCAR_ADVANCED_CELL_OP_TREE_H
#define LIBOSCAR_ADVANCED_CELL_OP_TREE_H
#include <liboscar/AdvancedOpTree.h>
#include <liboscar/CQRFromComplexSpatialQuery.h>

#include <sserialize/spatial/CellQueryResult.h>
#include <sserialize/Static/CellTextCompleter.h>
#include <sserialize/Static/CQRDilator.h>
#include <sserialize/Static/GeoHierarchySubGraph.h>
#include <sserialize/strings/stringfunctions.h>
#include <sserialize/utility/assert.h>

namespace liboscar {

class AdvancedCellOpTree: public AdvancedOpTree {
public:
	struct CalcBase {
		CalcBase(sserialize::Static::CellTextCompleter & ctc,
			const sserialize::Static::CQRDilator & cqrd,
			const CQRFromComplexSpatialQuery & csq,
			const sserialize::spatial::GeoHierarchySubGraph & ghsg,
			uint32_t threadCount) :
		m_ctc(ctc),
		m_cqrd(cqrd),
		m_csq(csq),
		m_ghsg(ghsg),
		m_threadCount(threadCount)
		{}
		sserialize::Static::CellTextCompleter & m_ctc;
		const sserialize::Static::CQRDilator & m_cqrd;
		const CQRFromComplexSpatialQuery & m_csq;
		const sserialize::spatial::GeoHierarchySubGraph & m_ghsg;
		uint32_t m_threadCount;
		
		const sserialize::Static::ItemIndexStore & idxStore() const;
		const sserialize::CellQueryResult::CellInfo & ci() const;
		const sserialize::Static::spatial::GeoHierarchy & gh() const;
		const liboscar::Static::OsmKeyValueObjectStore & store() const;
		const sserialize::spatial::GeoHierarchySubGraph & ghsg() const;
		sserialize::Static::CellTextCompleter & ctc();
		uint32_t threadCount() const;
		sserialize::CellQueryResult toCQR(const sserialize::TreedCellQueryResult & cqr) const;
		sserialize::CellQueryResult calcBetweenOp(const sserialize::CellQueryResult & c1, const sserialize::CellQueryResult & c2);
		sserialize::CellQueryResult calcCompassOp(Node * node, const sserialize::CellQueryResult & cqr);
		sserialize::CellQueryResult calcRelevantElementOp(Node * node, const sserialize::CellQueryResult & cqr);
		sserialize::CellQueryResult calcInOp(Node * node, const sserialize::CellQueryResult & cqr);
		///th in [0, 1]
		sserialize::ItemIndex calcDilateRegionOp(double th, const sserialize::CellQueryResult & cqr);
		sserialize::ItemIndex calcDilateRegionOp(Node * node, const sserialize::CellQueryResult & cqr);
		static std::vector<double> asDoubles(const std::string & str);
	};

	template<typename T_CQR_TYPE>
	struct Calc: public CalcBase {
		typedef T_CQR_TYPE CQRType;
		Calc(sserialize::Static::CellTextCompleter & ctc,
			const sserialize::Static::CQRDilator & cqrd,
			const CQRFromComplexSpatialQuery & csq,
			const sserialize::spatial::GeoHierarchySubGraph & ghsg,
			uint32_t threadCount) :
		CalcBase(ctc, cqrd, csq, ghsg, threadCount)
		{}
		CQRType calc(Node * node);
		CQRType calcItem(Node * node);
		CQRType calcString(Node * node);
		CQRType calcRect(Node * node);
		CQRType calcPolygon(Node * node);
		CQRType calcPath(Node * node);
		CQRType calcPoint(Node * node);
		CQRType calcRegionExclusiveCells(Node * node);
		CQRType calcRegion(Node * node);
		CQRType calcCell(Node * node);
		CQRType calcCells(Node * node);
		CQRType calcTriangle(Node * node);
		CQRType calcTriangles(Node * node);
		CQRType calcUnaryOp(Node * node);
		CQRType calcDilationOp(Node * node);
		CQRType calcRegionDilationOp(Node * node);
		CQRType calcQueryExclusiveCells(Node * node);
		CQRType calcCompassOp(Node * node);
		CQRType calcNearOp(Node * node);
		CQRType calcInOp(Node * node);
		CQRType calcRelevantElementOp(Node * node);
		CQRType calcBinaryOp(Node * node);
		CQRType calcBetweenOp(Node * node);
	};
public:
	AdvancedCellOpTree(
		const sserialize::Static::CellTextCompleter & ctc,
		const sserialize::Static::CQRDilator & cqrd,
		const CQRFromComplexSpatialQuery & csq,
		const sserialize::spatial::GeoHierarchySubGraph & ghsg);
	AdvancedCellOpTree(const AdvancedCellOpTree &) = delete;
	virtual ~AdvancedCellOpTree();
	AdvancedCellOpTree & operator=(const AdvancedCellOpTree&) = delete;
	///remove potential harmless queries
	void clean(double maxDilation);
	template<typename T_CQR_TYPE>
	T_CQR_TYPE calc(uint32_t threadCount = 1);
	
public:
	sserialize::Static::CellTextCompleter & ctc() { return m_ctc; }
	const sserialize::Static::CellTextCompleter & ctc() const { return m_ctc; }
	const sserialize::Static::CQRDilator & cqrd() const { return m_cqrd; }
	const CQRFromComplexSpatialQuery & csq() const { return m_csq; }
	const sserialize::spatial::GeoHierarchySubGraph & ghsg() const { return m_ghsg; }

private:
	sserialize::Static::CellTextCompleter m_ctc;
	sserialize::Static::CQRDilator m_cqrd;
	CQRFromComplexSpatialQuery m_csq;
	sserialize::spatial::GeoHierarchySubGraph m_ghsg;
};

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::calc(uint32_t threadCount) {
	typedef T_CQR_TYPE CQRType;
	if (root()) {
		Calc<CQRType> calculator(m_ctc, m_cqrd, m_csq, m_ghsg, threadCount);
		return calculator.calc( root() );
	}
	else {
		return CQRType();
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcRect(AdvancedCellOpTree::Node* node) {
	sserialize::spatial::GeoRect rect(node->value, true);
	if (rect.lengthInM() < liboscar::CQRFromPolygon::ACT_POLYGON_ITEM_BBOX) {
		return T_CQR_TYPE( m_csq.cqrfp().cqr(sserialize::spatial::GeoPolygon::fromRect(rect), CQRFromPolygon::AC_AUTO, m_ctc.flags(), m_threadCount) );
	}
	else {
		return m_ctc.cqrFromRect<CQRType>(rect);
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcPolygon(AdvancedCellOpTree::Node* node) {
	//first construct the polygon out of the values
	std::vector<sserialize::spatial::GeoPoint> gps;
	{
		struct MyOut {
			std::vector<sserialize::spatial::GeoPoint> * dest;
			double m_firstCoord;
			MyOut & operator++() { return *this; }
			MyOut & operator*() { return *this; }
			MyOut & operator=(const std::string & str) {
				double t = sserialize::stod(str);
				if (m_firstCoord == std::numeric_limits<double>::max()) {
					m_firstCoord = t;
				}
				else {
					dest->emplace_back(m_firstCoord, t);
					m_firstCoord = std::numeric_limits<double>::max();
				}
				return *this;
			}
			MyOut(std::vector<sserialize::spatial::GeoPoint> * d) : dest(d), m_firstCoord(std::numeric_limits<double>::max()) {}
		};
		typedef sserialize::OneValueSet<uint32_t> MyS;
		sserialize::split<std::string::const_iterator, MyS, MyS, MyOut>(node->value.begin(), node->value.end(), MyS(','), MyS('\\'), MyOut(&gps));
	}
	if (gps.size() < 3) {
		return T_CQR_TYPE();
	}
	//check if back and front are the same, if not, close the polygon:
	if (!sserialize::spatial::equal(gps.back(), gps.front(), 0.0)) {
		gps.push_back(gps.front());
	}
	
	sserialize::CellQueryResult cqr = m_csq.cqrfp().cqr(sserialize::spatial::GeoPolygon(std::move(gps)), CQRFromPolygon::AC_AUTO, m_ctc.flags(), m_threadCount);
	return T_CQR_TYPE(cqr);
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcPath(AdvancedCellOpTree::Node* node) {
	std::vector<double> tmp( asDoubles(node->value) );
	if (tmp.size() < 3 || tmp.size() % 2 == 0) {
		return CQRType();
	}
	double radius(tmp[0]);
	if (tmp.size() == 3) {
		return CQRType( m_csq.cqrfp().cqr(sserialize::spatial::GeoPoint(tmp[1], tmp[2]), radius, CQRFromPolygon::AC_AUTO, m_ctc.flags(), m_threadCount) );
	}
	else if (tmp.size() == 5) {
		sserialize::spatial::GeoPoint startPoint(tmp[1], tmp[2]), endPoint(tmp[3], tmp[4]);
		return m_ctc.cqrBetween<CQRType>(startPoint, endPoint, radius);
	}
	else {
		std::vector<sserialize::spatial::GeoPoint> gp;
		gp.reserve(tmp.size()/2);
		for(std::vector<double>::const_iterator it(tmp.begin()+1), end(tmp.end()); it != end; it += 2) {
			gp.emplace_back(*it, *(it+1));
		}
		sserialize::spatial::GeoWay gw(gp);
		double gwLen = gw.length();
		if (radius > 0.0 && gwLen < 5*1000 && radius < 5000) {
			return m_ctc.cqrAlongPath<CQRType>(radius, gp.begin(), gp.end());
		}
		else {
			auto tmp = m_ctc.cqrAlongPath<sserialize::CellQueryResult>(0.0, gp.begin(), gp.end());
			if (radius > 0.0) {
				return CQRType(m_cqrd.dilate(tmp, radius, m_threadCount), ci(), idxStore(), tmp.flags()) + CQRType(tmp);
			}
			else {
				return CQRType(tmp);
			}
		}
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcPoint(AdvancedCellOpTree::Node* node) {
	return calcPath(node);
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcRegion(AdvancedCellOpTree::Node * node) {
	uint32_t id = atoi(node->value.c_str());
	return m_ctc.cqrFromRegionStoreId<CQRType>(id);
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcRegionExclusiveCells(AdvancedCellOpTree::Node * node) {
	uint32_t storeId = atoi(node->value.c_str());
	uint32_t ghId = m_ctc.geoHierarchy().storeIdToGhId(storeId);
	return CQRType(m_ghsg.regionExclusiveCells(ghId), ci(), idxStore(), m_ctc.flags() & sserialize::CellQueryResult::FF_MASK_CELL_ITEM_IDS);
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcCell(AdvancedCellOpTree::Node* node) {
	if (node->value.find(',') != std::string::npos) {
		std::vector<double> tmp( asDoubles(node->value) );
		if (tmp.size() != 2) {
			return CQRType();
		}
		return m_ctc.cqrFromPoint<CQRType>(sserialize::spatial::GeoPoint(tmp.front(), tmp.back()), 0.0);
	}
	else {
		uint32_t id = atoi(node->value.c_str());
		return m_ctc.cqrFromCellId<CQRType>(id);
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcCells(AdvancedCellOpTree::Node* node) {
	std::vector<uint32_t> cellIds;
	if (node->value.size()) {
		
	}
	return m_ctc.cqrFromCellIds<CQRType>(cellIds.cbegin(), cellIds.cend());
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcTriangle(AdvancedCellOpTree::Node* node) {
	if (node->value.find(',') != std::string::npos) {
		std::vector<double> tmp( asDoubles(node->value) );
		if (tmp.size() != 2) {
			return CQRType();
		}
		return m_ctc.cqrFromPoint<CQRType>(sserialize::spatial::GeoPoint(tmp.front(), tmp.back()), 0.0);
	}
	else {
		uint32_t id = atoi(node->value.c_str());
		return m_ctc.cqrFromTriangleId<CQRType>(id);
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcItem(AdvancedCellOpTree::Node * node) {
	uint32_t id = atoi(node->value.c_str());
	liboscar::Static::OsmKeyValueObjectStore::Item item(store().at(id));
	if (!item.valid()) {
		return CQRType();
	}
	sserialize::ItemIndex idx(std::vector<uint32_t>(1, id));
	sserialize::ItemIndex pmIdx;
	if (item.isRegion()) {
		pmIdx = idxStore().at( gh().regionCellIdxPtr( gh().storeIdToGhId(item.id()) ) );
	}
	else {
		auto itemCells( item.cells() );
		pmIdx = sserialize::ItemIndex(std::vector<uint32_t>(itemCells.begin(), itemCells.end()));
	}
	std::vector<sserialize::ItemIndex> cellIdx(pmIdx.size(), idx);
	return CQRType(sserialize::ItemIndex(), pmIdx, cellIdx.begin(), ci(), idxStore(), sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS);
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcString(AdvancedCellOpTree::Node* node) {
	if (!node->value.size()) {
		return CQRType();
	}
	const std::string & str = node->value;
	std::string qstr(str);
	sserialize::StringCompleter::QuerryType qt = sserialize::StringCompleter::QT_NONE;
	qt = sserialize::StringCompleter::normalize(qstr);
	if (node->subType == Node::STRING_ITEM) {
		return m_ctc.items<T_CQR_TYPE>(qstr, qt);
	}
	else if (node->subType == Node::STRING_REGION) {
		return m_ctc.regions<T_CQR_TYPE>(qstr, qt);
	}
	else {
		return m_ctc.complete<T_CQR_TYPE>(qstr, qt);
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcBinaryOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT_EQUAL((std::string::size_type)1, node->value.size());
	switch (node->value.front()) {
	case '+':
		return calc(node->children.front()) + calc(node->children.back());
	case '/':
	case ' ':
		return calc(node->children.front()) / calc(node->children.back());
	case '-':
		return calc(node->children.front()) - calc(node->children.back());
	case '^':
		return calc(node->children.front()) ^ calc(node->children.back());
	default:
		return CQRType();
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcUnaryOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT_EQUAL((std::string::size_type)1, node->value.size());
	switch (node->value.front()) {
	case '%':
		return calc(node->children.front()).allToFull();
	default:
		return CQRType();
	}
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcQueryExclusiveCells(AdvancedCellOpTree::Node* node) {
	CQRType cqr( calc(node->children.front()) );
	uint32_t dpMin = 0;
	uint32_t dpMax = std::numeric_limits<uint32_t>::max();
	{
		std::vector<std::string> tmp = sserialize::split< std::vector<std::string> >(node->value, ':');
		if (tmp.size() > 1) {
			dpMin = std::atoi(tmp.front().c_str());
			dpMax = std::atoi(tmp.at(1).c_str());
		}
		else if (tmp.size()) {
			dpMax = std::atoi(tmp.front().c_str());
		}
	}
	if (!dpMax) {
		return CQRType();
	}
	std::vector<uint32_t> tmp;
	for(uint32_t i(0), s(cqr.cellCount()); i < s; ++i) {
		uint32_t cellId = cqr.cellId(i);
		uint32_t dps = ghsg().directParentsSize(cellId);
		if (dps >= dpMin && dps <= dpMax) {
			tmp.push_back(cellId);
		}
	}
	sserialize::ItemIndex idx(std::move(tmp));
	return CQRType(idx, ci(), idxStore(), cqr.flags()) / cqr;
}

template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calcNearOp(AdvancedCellOpTree::Node* node) {
	SSERIALIZE_CHEAP_ASSERT(node->children.size() == 1);
	return calc(node->children.front()).allToFull();
}

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcBetweenOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcCompassOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcInOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcInOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRelevantElementOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcDilationOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::CellQueryResult
AdvancedCellOpTree::Calc<sserialize::CellQueryResult>::calcRegionDilationOp(AdvancedCellOpTree::Node* node);

template<>
sserialize::TreedCellQueryResult
AdvancedCellOpTree::Calc<sserialize::TreedCellQueryResult>::calcRegionDilationOp(AdvancedCellOpTree::Node* node);


template<typename T_CQR_TYPE>
T_CQR_TYPE
AdvancedCellOpTree::Calc<T_CQR_TYPE>::calc(AdvancedCellOpTree::Node* node) {
	if (!node) {
		return CQRType();
	}
	switch (node->baseType) {
	case Node::LEAF:
		switch (node->subType) {
		case Node::STRING:
		case Node::STRING_REGION:
		case Node::STRING_ITEM:
			return calcString(node);
		case Node::REGION:
			return calcRegion(node);
		case Node::REGION_EXCLUSIVE_CELLS:
			return calcRegionExclusiveCells(node);
		case Node::CELL:
			return calcCell(node);
		case Node::CELLS:
			return calcCells(node);
		case Node::RECT:
			return calcRect(node);
		case Node::POLYGON:
			return calcPolygon(node);
		case Node::PATH:
			return calcPath(node);
		case Node::POINT:
			return calcPoint(node);
		case Node::ITEM:
			return calcItem(node);
		default:
			break;
		};
		break;
	case Node::UNARY_OP:
		switch(node->subType) {
		case Node::FM_CONVERSION_OP:
			return calcUnaryOp(node);
		case Node::CELL_DILATION_OP:
			return calcDilationOp(node);
		case Node::REGION_DILATION_OP:
			return calcRegionDilationOp(node);
		case Node::COMPASS_OP:
			return calcCompassOp(node);
		case Node::IN_OP:
			return calcInOp(node);
		case Node::NEAR_OP:
			return calcNearOp(node);
		case Node::RELEVANT_ELEMENT_OP:
			return calcRelevantElementOp(node);
		case Node::QUERY_EXCLUSIVE_CELLS:
			return calcQueryExclusiveCells(node);
		default:
			break;
		};
		break;
	case Node::BINARY_OP:
		switch(node->subType) {
		case Node::SET_OP:
			return calcBinaryOp(node);
		case Node::BETWEEN_OP:
			return calcBetweenOp(node);
		default:
			break;
		};
		break;
	default:
		break;
	};
	return CQRType();
}

}//end namespace

#endif
