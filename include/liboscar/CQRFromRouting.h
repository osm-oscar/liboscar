#pragma once

#ifndef LIBOSCAR_CQR_FROM_ROUTING_H
#define LIBOSCAR_CQR_FROM_ROUTING_H
#include <sserialize/spatial/CellQueryResult.h>
#include <sserialize/Static/ItemIndexStore.h>
#include <sserialize/spatial/GeoPoint.h>

namespace liboscar::interface {

class CQRFromRouting {
public:
    using CellQueryResult = sserialize::CellQueryResult;
public:
    enum {
        F_PEDESTRIAN=0x1,
        F_BIKE=0x2,
        F_CAR=0x4,

        //pedestrian options
        F_PEDESTRIAN_NO_STAIRS=0x10,
        F_PEDESTRIAN_NO_UNPAVED=0x20,

        //bike options
        F_BIKE_RACER=0x100,
        F_BIKE_MTB=0x200,
        F_BIKE_CYCLEWAYS=0x400,

        //car options
        F_CAR_AVOID_TOLL_ROADS=0x1000,
        F_CAR_AVOID_HIGHWAYS=0x2000
    } Flags;
public:
	template<typename TSubClass, typename... TArgs>
	static
	std::shared_ptr<typename 
		std::enable_if<
			std::is_base_of<liboscar::interface::CQRFromRouting, TSubClass>::value,
			TSubClass
		>
		::type
	>
	make_shared(TArgs... args) { return std::make_shared<TSubClass>(std::forward<TArgs>(args)...); }
	
	template<typename TSubClass, typename... TArgs>
	static
	std::unique_ptr<typename 
		std::enable_if<
			std::is_base_of<liboscar::interface::CQRFromRouting, TSubClass>::value,
			TSubClass
		>
		::type
	>
	make_unique(TArgs... args) { return std::make_unique<TSubClass>(std::forward<TArgs>(args)...); }
public:
    inline CellQueryResult operator()(sserialize::spatial::GeoPoint const & source, sserialize::spatial::GeoPoint const & target, int flags, double radius) const {
        return cqr(source, target, flags, radius);
    }
public:
    virtual CellQueryResult cqr(sserialize::spatial::GeoPoint const & source, sserialize::spatial::GeoPoint const & target, int flags, double radius) const = 0;
protected:
    CQRFromRouting();
    virtual ~CQRFromRouting();

};

} //end namespace liboscar::interface

namespace liboscar::adaptors {

class CQRFromRoutingFromCellList: public liboscar::interface::CQRFromRouting  {
public:
    using Self = CQRFromRoutingFromCellList;
    using MyBaseClass = liboscar::interface::CQRFromRouting;
    using Operator = std::function<sserialize::ItemIndex(sserialize::spatial::GeoPoint const &, sserialize::spatial::GeoPoint const &, int, double)>;
public:
    inline static auto make_shared(CellQueryResult::ItemIndexStore const & idxStore, CellQueryResult::CellInfo const & cellInfo, Operator op) {
        return MyBaseClass::make_shared<Self>(idxStore, cellInfo, op);
    }
    inline static auto make_unique(CellQueryResult::ItemIndexStore const & idxStore, CellQueryResult::CellInfo const & cellInfo, Operator op) {
        return MyBaseClass::make_unique<Self>(idxStore, cellInfo, op);
    }
public:
    CellQueryResult cqr(sserialize::spatial::GeoPoint const & source, sserialize::spatial::GeoPoint const & target, int flags, double radius) const override;
public:
    CQRFromRoutingFromCellList(CellQueryResult::ItemIndexStore const & idxStore, CellQueryResult::CellInfo const & cellInfo, Operator op);
    ~CQRFromRoutingFromCellList() override;
private:
    CellQueryResult::ItemIndexStore m_idxStore;
    CellQueryResult::CellInfo m_ci;
    Operator m_op;
};

} //end namespace liboscar::adaptors

namespace liboscar::impl {

class CQRFromRoutingNoOp: public liboscar::interface::CQRFromRouting {
public:
    using Self = CQRFromRoutingNoOp;
    using MyBaseClass = liboscar::interface::CQRFromRouting;
public:
    inline static auto make_shared() { return MyBaseClass::make_shared<Self>(); }
    inline static auto make_unique() { return MyBaseClass::make_unique<Self>(); }
public:
    CellQueryResult cqr(sserialize::spatial::GeoPoint const & source, sserialize::spatial::GeoPoint const & target, int flags, double radius) const override;
public:
    CQRFromRoutingNoOp() {}
    ~CQRFromRoutingNoOp() override {}
};

} //end namespace liboscar

#endif
