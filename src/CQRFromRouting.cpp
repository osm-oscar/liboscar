#include <liboscar/CQRFromRouting.h>

namespace liboscar::interface {
	
CQRFromRouting::CQRFromRouting() {}
CQRFromRouting::~CQRFromRouting() {}
	
} //end namespace liboscar::interface

namespace liboscar::adaptors {

CQRFromRoutingFromCellList::CQRFromRoutingFromCellList(CellQueryResult::ItemIndexStore const & idxStore, CellQueryResult::CellInfo const & cellInfo, Operator op) :
m_idxStore(idxStore),
m_ci(cellInfo),
m_op(op)
{}

CQRFromRoutingFromCellList::~CQRFromRoutingFromCellList() {}

CQRFromRoutingFromCellList::CellQueryResult
CQRFromRoutingFromCellList::cqr(sserialize::spatial::GeoPoint const & source, sserialize::spatial::GeoPoint const & target, int flags, double radius) const {
    return CellQueryResult(
        m_op(source, target, flags, radius),
        m_ci,
        m_idxStore,
        CellQueryResult::FF_DEFAULTS
    );
}

} //end namespace liboscar::adaptors

namespace liboscar::impl {

CQRFromRoutingNoOp::CellQueryResult
CQRFromRoutingNoOp::cqr(sserialize::spatial::GeoPoint const &, sserialize::spatial::GeoPoint const &, int, double) const {
    return CellQueryResult();
}

} //end namespace liboscar::impl
