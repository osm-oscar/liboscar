#include <liboscar/CellDistanceBySphere.h>
#include <sserialize/algorithm/hashspecializations.h>

#include <CGAL/Cartesian.h>
#include <CGAL/Random.h>
#include <CGAL/Exact_rational.h>
#include <CGAL/Min_sphere_of_spheres_d.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

namespace liboscar {

CellDistanceBySphere::CellDistanceBySphere(const std::vector<CellInfo> & d) :
m_ci(d)
{}

CellDistanceBySphere::CellDistanceBySphere(std::vector<CellInfo> && d) :
m_ci(std::move(d))
{}

CellDistanceBySphere::~CellDistanceBySphere() {}

double CellDistanceBySphere::distance(uint32_t cellId1, uint32_t cellId2) const {
	const CellInfo & ci1 = m_ci.at(cellId1);
	const CellInfo & ci2 = m_ci.at(cellId2);
	
	double centerDistance = CellDistance::distance(ci1.center, ci2.center);
	
	return std::max<double>(0.0, centerDistance - ci1.radius - ci2.radius);
}

double CellDistanceBySphere::distance(const sserialize::spatial::GeoPoint & gp, uint32_t cellId) const {
	const CellInfo & ci = m_ci.at(cellId);
	double centerDistance = CellDistance::distance(ci.center, gp);
	return std::max<double>(0.0, centerDistance - ci.radius);
}

std::vector<CellDistanceBySphere::CellInfo>
CellDistanceBySphere::minSpheres(const TriangulationGeoHierarchyArrangement & tra) {
	 //then center_cartesian_begin returns (a,b) = std::pair<FT, FT>
	 //and real coordinate is a + b * sqrt(Min_sphere.discriminant())
// 	typedef CGAL::Exact_rational              FT;
	typedef double                          FT;
	typedef CGAL::Cartesian<FT>               K;
	typedef CGAL::Min_sphere_of_spheres_d_traits_2<K,FT> Traits;
	typedef CGAL::Min_sphere_of_spheres_d<Traits> Min_sphere;

	typedef K::Point_2 Point;
	typedef Traits::Sphere Sphere;
	
	struct State {
		double maxRadius = 0;
		uint32_t maxRadiusCellId = 0;
	} state;
	
	std::unordered_set<std::pair<double, double>> pts;
	std::vector<Sphere> cgalpts;
	std::vector<CellDistanceBySphere::CellInfo> d;
	d.reserve(tra.cellCount());
	for(uint32_t cellId(0), s(tra.cellCount()); cellId < s; ++cellId) {
		pts.clear();
		cgalpts.clear();
		tra.cfGraph(cellId).visitCB([&pts, &tra, cellId](const auto & face) {
			for (uint32_t j(0); j < 3; ++j) {
				uint32_t nId = face.neighborId(j);
				uint32_t ncId = tra.cellIdFromFaceId(nId);
				if (ncId != cellId) { //only insert points of border edges
					pts.emplace( face.point( tra.tds().ccw(j) ) );
					pts.emplace( face.point( tra.tds().cw(j) ) );
				}
			}
		});
		for(const auto & x : pts) {
			cgalpts.emplace_back(Point(x.first, x.second), 0);
		}
		Min_sphere ma(cgalpts.begin(), cgalpts.end());
		CellInfo ci;
		{
			auto ccit = ma.center_cartesian_begin();
			auto center_x = *ccit;
			++ccit;
			auto center_y = *ccit;
			ci.center.lat() = CGAL::to_double(center_x);
			ci.center.lon() = CGAL::to_double(center_y);
		}
		{ //get the outer/inner radius by iterating over the support points
			ci.radius = 0.0;
			sserialize::spatial::GeoPoint gp;
			for(auto it(ma.support_begin ()), sit(ma.support_end()); it != sit; ++it) {
				auto p = *it;
				gp.lat() = CGAL::to_double( p.first.x() );
				gp.lon() = CGAL::to_double( p.first.y() );
				ci.radius = std::max(ci.radius, CellDistance::distance(ci.center, gp));
			}
		}
		if (ci.radius > state.maxRadius) {
			state.maxRadius = ci.radius;
			state.maxRadiusCellId = cellId;
		}
		d.push_back(ci);
	}
	
	std::cout << "Largest sphere: cellId=" << state.maxRadiusCellId << "; center:" << d.at(state.maxRadiusCellId).center << "; radius=" << d.at(state.maxRadiusCellId).radius << std::endl;
	return d;
}

std::vector<CellDistanceBySphere::CellInfo>
CellDistanceBySphere::spheres(const TriangulationGeoHierarchyArrangement & tra) {
	
	struct State {
		double maxRadius = 0;
		uint32_t maxRadiusCellId = 0;
	} state;
	
	std::unordered_set<std::pair<double, double>> pts;
	std::vector<CellDistanceBySphere::CellInfo> d;
	d.reserve(tra.cellCount());
	for(uint32_t cellId(0), s(tra.cellCount()); cellId < s; ++cellId) {
		pts.clear();
		tra.cfGraph(cellId).visitCB([&pts, &tra, cellId](const auto & face) {
			for (uint32_t j(0); j < 3; ++j) {
				uint32_t nId = face.neighborId(j);
				uint32_t ncId = tra.cellIdFromFaceId(nId);
				if (ncId != cellId) { //only insert points of border edges
					pts.emplace( face.point( tra.tds().ccw(j) ) );
					pts.emplace( face.point( tra.tds().cw(j) ) );
				}
			}
		});
		CellInfo ci;
		ci.center.lat() = 0;
		ci.center.lon() = 0;
		ci.radius = 0;
		for(const auto & x : pts) {
			ci.center.lat() += x.first;
			ci.center.lon() += x.second;
		}
		ci.center.lat() /= pts.size();
		ci.center.lon() /= pts.size();
		for(const auto & x : pts) {
			ci.radius = std::max(ci.radius, CellDistance::distance(ci.center, sserialize::spatial::GeoPoint(x)));
		}
		if (ci.radius > state.maxRadius) {
			state.maxRadius = ci.radius;
			state.maxRadiusCellId = cellId;
		}
		d.push_back(ci);
	}
	
	std::cout << "Largest sphere: cellId=" << state.maxRadiusCellId << "; center:" << d.at(state.maxRadiusCellId).center << "; radius=" << d.at(state.maxRadiusCellId).radius << std::endl;
	return d;
}

}//end namespace
