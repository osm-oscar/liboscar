#include <liboscar/CellDistanceByAnulus.h>

#include <sserialize/algorithm/hashspecializations.h>
#include <sserialize/mt/ThreadPool.h>

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
	
	return std::max<double>(0.0, centerDistance - ci1.outerRadius - ci2.outerRadius);
}

double CellDistanceByAnulus::distance(const sserialize::spatial::GeoPoint & gp, uint32_t cellId) const {
	const CellInfo & ci = m_ci.at(cellId);
	double centerDistance = CellDistance::distance(ci.center, gp);
	return std::max<double>(0.0, centerDistance - ci.outerRadius);
}

std::vector<CellDistanceByAnulus::CellInfo>
CellDistanceByAnulus::cellInfo(const TriangulationGeoHierarchyArrangement & tra, uint32_t threadCount) {
// 	typedef CGAL::Exact_integer ET; //this breaks
	
	typedef CGAL::Gmpzf ET;
// 	typedef CGAL::Gmpq ET;
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
	
	struct State {
		double maxRadius = 0;
		uint32_t maxRadiusCellId = 0;
		const TriangulationGeoHierarchyArrangement & tra;
		std::atomic<uint32_t> cellId{0};
		std::vector<CellDistanceByAnulus::CellInfo> d;
		State(const TriangulationGeoHierarchyArrangement & tra) : tra(tra), d(tra.cellCount()) {}
	};
	
	struct Worker {
		State * state;
		std::unordered_set<std::pair<double, double>> pts;
		std::vector<Point> cgalpts;
		Worker(State * state) : state(state) {}
		Worker(Worker && other) : state(other.state) {}
		Worker(const Worker & other) : state(other.state) {}
		void operator()() {
			while(true) {
				uint32_t cellId = state->cellId.fetch_add(1, std::memory_order_relaxed);
				if (cellId >= state->d.size()) {
					break;
				}
				process(cellId);
			}
		}
		void process(uint32_t cellId) {
			pts.clear();
			cgalpts.clear();
			state->tra.cfGraph(cellId).visitCB([this, cellId](const auto & face) {
				for (uint32_t j(0); j < 3; ++j) {
					auto nId = face.neighborId(j);
					auto ncId = state->tra.cellIdFromFaceId(nId);
					if (ncId != cellId) { //only insert points of border edges
						pts.emplace( face.point( state->tra.tds().ccw(j) ) );
						pts.emplace( face.point( state->tra.tds().cw(j) ) );
					}
				}
			});
			for(const auto & x : pts) {
				cgalpts.push_back( Point(x.first, x.second) );
			}
			Min_annulus ma(cgalpts.begin(), cgalpts.end());
			CellInfo & ci = state->d.at(cellId);
			{
				auto ccit = ma.center_coordinates_begin();
				auto center_x = *ccit;
				++ccit;
				auto center_y = *ccit;
				++ccit;
				auto center_h = *ccit;
				ci.center.lat() = CGAL::to_double(center_x) / CGAL::to_double(center_h);
				ci.center.lon() = CGAL::to_double(center_y) / CGAL::to_double(center_h);
			}
			{ //get the outer/inner radius by iterating over the support points
				ci.outerRadius = 0.0;
				ci.innerRadius = std::numeric_limits<double>::max();
				sserialize::spatial::GeoPoint gp;
				for(auto it(ma.outer_support_points_begin()), sit(ma.outer_support_points_end()); it != sit; ++it) {
					auto p = *it;
					gp.lat() = CGAL::to_double( p.x() );
					gp.lon() = CGAL::to_double( p.y() );
					ci.outerRadius = std::max(ci.outerRadius, CellDistance::distance(ci.center, gp));
				}
				for(auto it(ma.inner_support_points_begin()), sit(ma.inner_support_points_end()); it != sit; ++it) {
					auto p = *it;
					gp.lat() = CGAL::to_double( p.x() );
					gp.lon() = CGAL::to_double( p.y() );
					ci.innerRadius = std::min(ci.outerRadius, CellDistance::distance(ci.center, gp));
				}
			}
			if (ci.outerRadius > state->maxRadius) {
				state->maxRadius = ci.outerRadius;
				state->maxRadiusCellId = cellId;
			}
		};
	};
	
	State state(tra);
	sserialize::ThreadPool::execute(Worker(&state), threadCount, sserialize::ThreadPool::CopyTaskTag() );
	std::cout << "Largest sphere: cellId=" << state.maxRadiusCellId << "; center:" << state.d.at(state.maxRadiusCellId).center << "; radius=" << state.d.at(state.maxRadiusCellId).outerRadius << std::endl;
	return state.d;
}

}//end namespace
