#include <liboscar/CQRFromPolygon.h>

namespace liboscar {

CQRFromPolygon::CQRFromPolygon(const CQRFromPolygon & other) :
m_priv(other.m_priv)
{}

CQRFromPolygon::CQRFromPolygon(const Static::OsmKeyValueObjectStore & store, const sserialize::Static::ItemIndexStore & idxStore) :
m_priv(new detail::CQRFromPolygon(store, idxStore))
{}

CQRFromPolygon::~CQRFromPolygon() {}

const Static::OsmKeyValueObjectStore& CQRFromPolygon::store() const {
	return m_priv->store();
}

const CQRFromPolygon::CellInfo &
CQRFromPolygon::cellInfo() const {
	return m_priv->cellInfo();
}

const sserialize::Static::spatial::GeoHierarchy & CQRFromPolygon::geoHierarchy() const {
	return m_priv->geoHierarchy();
}

const sserialize::Static::ItemIndexStore & CQRFromPolygon::idxStore() const {
	return m_priv->idxStore();
}

sserialize::ItemIndex CQRFromPolygon::fullMatches(const sserialize::spatial::GeoPolygon & gp, Accuracy ac, uint32_t threadCount) const {
	return m_priv->fullMatches(gp, ac, threadCount);
}

sserialize::CellQueryResult CQRFromPolygon::cqr(const sserialize::spatial::GeoPolygon& gp, Accuracy ac, int cqrFlags, uint32_t threadCount) const {
	return m_priv->cqr(gp, ac, cqrFlags, threadCount);
}

sserialize::CellQueryResult CQRFromPolygon::cqr(const sserialize::spatial::GeoPoint& gp, double radius, CQRFromPolygon::Accuracy ac, int cqrFlags, uint32_t threadCount) const {
	return m_priv->cqr(gp, radius, ac, cqrFlags, threadCount);
}

namespace detail {

CQRFromPolygon::CQRFromPolygon(const Static::OsmKeyValueObjectStore& store, const sserialize::Static::ItemIndexStore & idxStore) :
m_store(store),
m_idxStore(idxStore),
m_ci(sserialize::Static::spatial::GeoHierarchyCellInfo::makeRc(m_store.geoHierarchy()))
{}

CQRFromPolygon::~CQRFromPolygon() {}

const Static::OsmKeyValueObjectStore& CQRFromPolygon::store() const {
	return m_store;
}

const CQRFromPolygon::CellInfo & CQRFromPolygon::cellInfo() const {
	return m_ci;
}

const sserialize::Static::spatial::GeoHierarchy& CQRFromPolygon::geoHierarchy() const {
	return m_store.geoHierarchy();
}

const sserialize::Static::ItemIndexStore & CQRFromPolygon::idxStore() const {
	return m_idxStore;
}

sserialize::ItemIndex CQRFromPolygon::fullMatches(const sserialize::spatial::GeoPolygon & gp, liboscar::CQRFromPolygon::Accuracy ac, uint32_t threadCount) const {
	switch (ac) {
	case liboscar::CQRFromPolygon::AC_POLYGON_CELL_BBOX:
	case liboscar::CQRFromPolygon::AC_POLYGON_CELL:
	case liboscar::CQRFromPolygon::AC_POLYGON_ITEM_BBOX:
	case liboscar::CQRFromPolygon::AC_POLYGON_ITEM:
	{
		return intersectingCellsPolygonCellBBox(gp);
		break;
	}
	case liboscar::CQRFromPolygon::AC_POLYGON_BBOX_CELL_BBOX:
	default:
		return m_store.geoHierarchy().intersectingCells(idxStore(), gp.boundary(), threadCount);
		break;
	};
}

sserialize::CellQueryResult CQRFromPolygon::cqr(const sserialize::spatial::GeoPolygon& gp, liboscar::CQRFromPolygon::Accuracy ac, int cqrFlags, uint32_t threadCount) const {
	if (ac == liboscar::CQRFromPolygon::AC_AUTO) {
		double th;
		{
			double gpLen = gp.length();
			double gpDiag = gp.boundary().diagInM();
			if (gpLen > liboscar::CQRFromPolygon::ACT_USE_LENGTH_OVER_DIAGONAL_RATIO*gpDiag) {
				th = gpLen / liboscar::CQRFromPolygon::ACT_USE_LENGTH_OVER_DIAGONAL_RATIO;
			}
			else {
				th = gpDiag;
			}
		}
		
		if (th < liboscar::CQRFromPolygon::ACT_POLYGON_ITEM) {
			ac = liboscar::CQRFromPolygon::AC_POLYGON_ITEM;
		}
		else if (th < liboscar::CQRFromPolygon::ACT_POLYGON_ITEM_BBOX) {
			ac = liboscar::CQRFromPolygon::AC_POLYGON_ITEM_BBOX;
		}
		else if (th < liboscar::CQRFromPolygon::ACT_POLYGON_CELL_BBOX) {
			ac = liboscar::CQRFromPolygon::AC_POLYGON_CELL_BBOX;
		}
		else { //really large, use fast test
			ac = liboscar::CQRFromPolygon::AC_POLYGON_BBOX_CELL_BBOX;
		}
	}
	switch (ac) {
	case liboscar::CQRFromPolygon::AC_POLYGON_ITEM:
		return intersectingCellsPolygonItem<detail::CQRFromPolygonHelpers::PolyCellItemIntersectOp>(gp).convert(cqrFlags);
	case liboscar::CQRFromPolygon::AC_POLYGON_ITEM_BBOX:
		return intersectingCellsPolygonItem<detail::CQRFromPolygonHelpers::PolyCellItemBBoxIntersectOp>(gp).convert(cqrFlags);
	case liboscar::CQRFromPolygon::AC_POLYGON_BBOX_ITEM:
		return intersectingCellsPolygonItem<detail::CQRFromPolygonHelpers::PolyBBoxCellItemIntersectOp>(gp).convert(cqrFlags);
	case liboscar::CQRFromPolygon::AC_POLYGON_BBOX_ITEM_BBOX:
		return intersectingCellsPolygonItem<detail::CQRFromPolygonHelpers::PolyBBoxCellItemBBoxIntersectOp>(gp).convert(cqrFlags);

	case liboscar::CQRFromPolygon::AC_POLYGON_CELL:
	case liboscar::CQRFromPolygon::AC_POLYGON_CELL_BBOX:
		return sserialize::CellQueryResult(intersectingCellsPolygonCellBBox(gp), cellInfo(), idxStore(), cqrFlags);
	case liboscar::CQRFromPolygon::AC_POLYGON_BBOX_CELL:
	case liboscar::CQRFromPolygon::AC_POLYGON_BBOX_CELL_BBOX:
		return sserialize::CellQueryResult(geoHierarchy().intersectingCells(idxStore(), gp.boundary()), cellInfo(), idxStore(), cqrFlags);
	default:
		throw sserialize::InvalidEnumValueException("CQRFromPolygon::Accuracy does not have " + std::to_string(ac) + " as value");
		return sserialize::CellQueryResult();;
	};
}

sserialize::CellQueryResult CQRFromPolygon::cqr(const sserialize::spatial::GeoPoint& gp, double radius, liboscar::CQRFromPolygon::Accuracy ac, int cqrFlags, uint32_t threadCount) const {
	if (radius <= 0) { //radius is 0
		uint32_t cellId = m_store.regionArrangement().cellId(gp);
		
		if (cellId == m_store.regionArrangement().NullCellId) {
			return sserialize::CellQueryResult();
		}

		//there should be exactly one
		sserialize::ItemIndex idx( m_idxStore.at( m_store.geoHierarchy().cellItemsPtr(cellId) ) );
		//now check all those items in that cell (this may be very very expensive)
		//though we can avoid checking non-polygon items
		//since the probability that the test point really intersects points/ways is close to zero
		std::vector<uint32_t> tmp;
		for(uint32_t itemId : idx) {
			sserialize::spatial::GeoShapeType gst = m_store.geoShapeType(itemId);
			bool contained = false;
			if (gst == sserialize::spatial::GS_POLYGON) {
				contained = m_store.geoShape(itemId).get<sserialize::spatial::GS_POLYGON>()->contains(gp);
			}
			else if (gst == sserialize::spatial::GS_MULTI_POLYGON) {
				contained = m_store.geoShape(itemId).get<sserialize::spatial::GS_MULTI_POLYGON>()->contains(gp);
			}
			if (contained) {
				tmp.emplace_back(itemId);
			}
		}
		std::vector<sserialize::ItemIndex> pmIdcs(1, sserialize::ItemIndex(std::move(tmp)));
		sserialize::CellQueryResult result(
			sserialize::ItemIndex(),
			sserialize::ItemIndex( std::vector<uint32_t>(1, cellId) ),
			pmIdcs.begin(),
			cellInfo(),
			idxStore(),
			sserialize::CellQueryResult::FF_CELL_GLOBAL_ITEM_IDS
		);
		return result.convert(cqrFlags);
	}
	else {
		return cqr(sserialize::spatial::GeoPolygon::fromRect(sserialize::spatial::GeoRect(gp.lat(), gp.lon(), radius)), ac, cqrFlags, threadCount);
	}
}

sserialize::Static::spatial::GeoPolygon detail::CQRFromPolygon::toStatic(const sserialize::spatial::GeoPolygon& gp) const {
	//as of now intersection between static and non-static geometry is not available
	sserialize::UByteArrayAdapter d(sserialize::UByteArrayAdapter::createCache(1, sserialize::MM_PROGRAM_MEMORY));
	gp.append(d);
	d.resetPtrs();
	return sserialize::Static::spatial::GeoPolygon(d);
}

sserialize::ItemIndex CQRFromPolygon::intersectingCellsPolygonCellBBox(const sserialize::spatial::GeoPolygon& gp) const {
	std::vector<uint32_t> intersectingCells;
	
	struct MyOperator {
		const sserialize::spatial::GeoPolygon & gp;
		const sserialize::Static::spatial::GeoHierarchy & gh;
		std::vector<uint32_t> & intersectingCells;
		
		void enclosed(const sserialize::ItemIndex & enclosedCells) {
			intersectingCells.insert(intersectingCells.end(), enclosedCells.cbegin(), enclosedCells.cend());
		}
		void candidates(const sserialize::ItemIndex & candidateCells) {
			for(uint32_t cellId : candidateCells) {
				if (gp.intersects(gh.cellBoundary(cellId))) {
					intersectingCells.push_back(cellId);
				}
			}
		}
		MyOperator(const sserialize::spatial::GeoPolygon & gp, const sserialize::Static::spatial::GeoHierarchy & gh, std::vector<uint32_t> & intersectingCells) :
		gp(gp), gh(gh), intersectingCells(intersectingCells)
		{}
	};
	MyOperator myOp(gp, m_store.geoHierarchy(), intersectingCells);

	visit(gp, toStatic(gp), myOp);

	std::sort(intersectingCells.begin(), intersectingCells.end());
	intersectingCells.resize(std::unique(intersectingCells.begin(), intersectingCells.end())-intersectingCells.begin());
	return sserialize::ItemIndex(std::move(intersectingCells));
}

}}//end namespace liboscar::detail
