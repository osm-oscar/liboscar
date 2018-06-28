#include <liboscar/CellDistanceByAnulus.h>
#include <sserialize/algorithm/hashspecializations.h>

#include <CGAL/Min_annulus_d.h>
#include <CGAL/Min_sphere_annulus_d_traits_2.h>
#include <CGAL/Homogeneous.h>
#include <CGAL/Exact_integer.h>
#include <CGAL/Gmpzf.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>



namespace liboscar {

CellDistanceByAnulus::CellDistanceByAnulus(const std::vector<CellInfo> & d) :
m_ci(d)
{}

CellDistanceByAnulus::CellDistanceByAnulus(std::vector<CellInfo> && d) :
m_ci(std::move(d))
{}

CellDistanceByAnulus::~CellDistanceByAnulus() {}

double CellDistanceByAnulus::distance(uint32_t cellId1, uint32_t cellId2) const {
	const CellInfo & ci1 = m_ci.at(cellId1);
	const CellInfo & ci2 = m_ci.at(cellId2);
	
	double centerDistance = CellDistance::distance(ci1.center, ci2.center);
	
	return std::max<double>(0.0, centerDistance - ci1.outerDiameter - ci2.outerDiameter);
}

double CellDistanceByAnulus::distance(const sserialize::spatial::GeoPoint & gp, uint32_t cellId) const {
	const CellInfo & ci = m_ci.at(cellId);
	double centerDistance = CellDistance::distance(ci.center, gp);
	return std::max<double>(0.0, centerDistance - ci.outerDiameter);
}

std::vector<CellDistanceByAnulus::CellInfo>
CellDistanceByAnulus::cellInfo(const TriangulationGeoHierarchyArrangement & tra) {
// 	typedef CGAL::Exact_integer ET; //this breaks
	
	typedef CGAL::Gmpzf ET;
	typedef CGAL::Homogeneous<double> K;
	typedef CGAL::Min_sphere_annulus_d_traits_2<K, ET, double> Traits;
	
	//the following is slow
// 	typedef CGAL::Homogeneous<CGAL::Gmpzf> K;
// 	typedef CGAL::Min_sphere_annulus_d_traits_2<K> Traits;
	
	//and this is slower, breaks?
// 	typedef CGAL::Exact_predicates_exact_constructions_kernel K;
// 	typedef CGAL::Min_sphere_annulus_d_traits_2<K> Traits;
	
	typedef K::Point_2 Point;
	typedef CGAL::Min_annulus_d<Traits> Min_annulus;
	
	std::unordered_set<std::pair<double, double>> pts;
	std::vector<Point> cgalpts;
	std::vector<CellDistanceByAnulus::CellInfo> d;
	d.reserve(tra.cellCount());
	for(uint32_t cellId(0), s(tra.cellCount()); cellId < s; ++cellId) {
		pts.clear();
		cgalpts.clear();
		tra.cfGraph(cellId).visitCB([&pts](const auto & face) {
			for(uint32_t i(0); i < 3; ++i) {
				pts.emplace( face.point(i) );
			}
		});
		for(const auto & x : pts) {
			cgalpts.push_back( Point(x.first, x.second) );
		}
		Min_annulus ma(cgalpts.begin(), cgalpts.end());
		CellInfo ci;
// 		auto center = ma.center();
// 		ci.center = sserialize::spatial::GeoPoint(CGAL::to_double(center.x()), CGAL::to_double(center.y()));
// 		ci.innerDiameter = CGAL::sqrt( ma.squared_inner_radius() );
// 		ci.outerDiameter = CGAL::sqrt( ma.squared_outer_radius() );
		
		d.push_back(ci);
	}
	return d;
}

}//end namespace
