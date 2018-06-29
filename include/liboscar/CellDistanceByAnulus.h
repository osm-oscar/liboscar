#ifndef LIBOSCAR_CELL_DISTANCE_BY_ANULUS_H
#define LIBOSCAR_CELL_DISTANCE_BY_ANULUS_H
#include <sserialize/spatial/CellDistance.h>
#include <sserialize/Static/TriangulationGeoHierarchyArrangement.h>

namespace liboscar {

class CellDistanceByAnulus: public sserialize::spatial::interface::CellDistance {
public:
	typedef sserialize::Static::spatial::TriangulationGeoHierarchyArrangement TriangulationGeoHierarchyArrangement;
	struct CellInfo {
		sserialize::spatial::GeoPoint center;
		double innerRadius;
		double outerRadius;
	};
public:
	CellDistanceByAnulus(const std::vector<CellInfo> & d);
	CellDistanceByAnulus(std::vector<CellInfo> && d);
	virtual ~CellDistanceByAnulus();
	virtual double distance(uint32_t cellId1, uint32_t cellId2) const;
	virtual double distance(const sserialize::spatial::GeoPoint & gp, uint32_t cellId) const;
public:
	static std::vector<CellInfo> cellInfo(const TriangulationGeoHierarchyArrangement & tra);
private:
	std::vector<CellInfo> m_ci;
};

}//end namespace

#endif
