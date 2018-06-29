#ifndef LIBOSCAR_CELL_DISTANCE_BY_SPHERE_H
#define LIBOSCAR_CELL_DISTANCE_BY_SPHERE_H
#include <sserialize/spatial/CellDistance.h>
#include <sserialize/Static/TriangulationGeoHierarchyArrangement.h>

namespace liboscar {

class CellDistanceBySphere: public sserialize::spatial::interface::CellDistance {
public:
	typedef sserialize::Static::spatial::TriangulationGeoHierarchyArrangement TriangulationGeoHierarchyArrangement;
	struct CellInfo {
		sserialize::spatial::GeoPoint center;
		double radius;
	};
public:
	CellDistanceBySphere(const std::vector<CellInfo> & d);
	CellDistanceBySphere(std::vector<CellInfo> && d);
	virtual ~CellDistanceBySphere();
	virtual double distance(uint32_t cellId1, uint32_t cellId2) const;
	virtual double distance(const sserialize::spatial::GeoPoint & gp, uint32_t cellId) const;
public:
	static std::vector<CellInfo> minSpheres(const TriangulationGeoHierarchyArrangement & tra);
	static std::vector<CellInfo> spheres(const TriangulationGeoHierarchyArrangement & tra);
private:
	std::vector<CellInfo> m_ci;
};

}//end namespace

#endif
