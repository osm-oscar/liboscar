#include <liboscar/CellDistanceBySphere.h>
#include <sserialize/algorithm/hashspecializations.h>
#include <sserialize/mt/ThreadPool.h>

#include <CGAL/Cartesian.h>
#include <CGAL/Random.h>
#include <CGAL/Exact_rational.h>
#include <CGAL/Min_sphere_of_spheres_d.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/config.h>

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
CellDistanceBySphere::minSpheres(const TriangulationGeoHierarchyArrangement & tra, uint32_t threadCount) {
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
		std::atomic<uint32_t> cellId{0};
		const TriangulationGeoHierarchyArrangement & tra;
		std::vector<CellDistanceBySphere::CellInfo> d;
		State(const TriangulationGeoHierarchyArrangement & tra) : tra(tra), d(tra.cellCount()) {}
	};
	
	struct Worker {
		State * state;
		std::unordered_set<std::pair<double, double>> pts;
		std::vector<Sphere> cgalpts;
		Worker(const Worker & other) : state(other.state) {}
		Worker(Worker && other) : state(other.state) {}
		Worker(State * state) : state(state) {}
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
				cgalpts.emplace_back(Point(x.first, x.second), 0);
			}
			Min_sphere ma(cgalpts.begin(), cgalpts.end());
			CellInfo & ci = state->d.at(cellId);
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
			if (ci.radius > state->maxRadius) {
				state->maxRadius = ci.radius;
				state->maxRadiusCellId = cellId;
			}
		}
	};
	
	State state(tra);
	
#if CGAL_VERSION_NR < 1041111000
	if (threadCount > 1) {
		std::cout << "Your CGAL version is too low to support multi-threading. Using only 1 thread" << std::endl;
		threadCount = 1;
	}
#endif
	
	sserialize::ThreadPool::execute(Worker(&state), threadCount, sserialize::ThreadPool::CopyTaskTag());
	
	std::cout << "Largest sphere: cellId=" << state.maxRadiusCellId << "; center:" << state.d.at(state.maxRadiusCellId).center << "; radius=" << state.d.at(state.maxRadiusCellId).radius << std::endl;
	return state.d;
}

std::vector<CellDistanceBySphere::CellInfo>
CellDistanceBySphere::spheres(const TriangulationGeoHierarchyArrangement & tra, uint32_t threadCount) {
	
	struct State {
		double maxRadius = 0;
		uint32_t maxRadiusCellId = 0;
		std::atomic<uint32_t> cellId{0};
		const TriangulationGeoHierarchyArrangement & tra;
		std::vector<CellDistanceBySphere::CellInfo> d;
		State(const TriangulationGeoHierarchyArrangement & tra) : tra(tra), d(tra.cellCount()) {}
	};
	
	struct Worker {
		State * state;
		std::unordered_set<std::pair<double, double>> pts;
		Worker(const Worker & other) : state(other.state) {}
		Worker(Worker && other) : state(other.state) {}
		Worker(State * state) : state(state) {}
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
			CellInfo & ci = state->d.at(cellId);
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
			if (ci.radius > state->maxRadius) {
				state->maxRadius = ci.radius;
				state->maxRadiusCellId = cellId;
			}
		}
	};
	
	State state(tra);
	
#if CGAL_VERSION_NR < 1041111000
	if (threadCount > 1) {
		std::cout << "Your CGAL version is too low to support multi-threading. Using only 1 thread" << std::endl;
		threadCount = 1;
	}
#endif
	
	sserialize::ThreadPool::execute(Worker(&state), threadCount, sserialize::ThreadPool::CopyTaskTag());
	
	std::cout << "Largest sphere: cellId=" << state.maxRadiusCellId << "; center:" << state.d.at(state.maxRadiusCellId).center << "; radius=" << state.d.at(state.maxRadiusCellId).radius << std::endl;
	return state.d;
}

}//end namespace
