#ifndef VERTEX_FILTER_HPP_
#define VERTEX_FILTER_HPP_

#include <cassert>

#include <algorithm>
#include <functional>
#include <ostream>

#include <glog/logging.h>
#include "affine_hull.hpp"

namespace FC {

/**
  @brief a vertex filter optimized specifically for the conditions of a
         descend_task
*/
template <class PointCloud, class ResultIterator>
struct vertex_filter {
  typedef typename PointCloud::size_type size_type;
  typedef typename PointCloud::eigen_map eigen_map;

  /**
    @brief for descend tasks
  */
  template <class Derived1, class Derived2, class IgnoreFn>
  vertex_filter(const PointCloud& pc,
                const Eigen::MatrixBase<Derived1>& driver,
                const Eigen::MatrixBase<Derived2>& member,
                const IgnoreFn& ignoreFn,
                ResultIterator result_begin)
  : _pc(pc), _current(result_begin), _end(result_begin) {
    _end = pc.radius_search(driver, (member - driver).squaredNorm(),
                            _current);
    _end = std::remove_if(_current, _end, ignoreFn);
    DLOG(INFO) << "VERTEX-FILTER ctor: candidate indices = ";
    for (auto it = _current; it != _end; ++it)
      DLOG(INFO) << *it << ", ";
    DLOG(INFO) << std::endl;
  }

  /**
    @brief for ascend tasks
  */
  template <typename Derived1, typename Derived2, typename Derived3>
  vertex_filter(PointCloud const& pc,
                Eigen::MatrixBase<Derived1> const& location,
                Eigen::MatrixBase<Derived2> const& direction,
                Eigen::MatrixBase<Derived3> const& point_on_sphere,
                std::function<bool(size_type)> ignoreFn,
                ResultIterator result_begin)
  : _pc(pc), _current(result_begin), _end(result_begin) {
    using number_type = typename Derived1::Scalar;
    using eigen_vector = Eigen::Matrix<number_type, Eigen::Dynamic, 1>;
    size_type num_iter = 0;
    const number_type sq_diameter = pc.diameter() * pc.diameter();
    number_type sq_radius;
    // probe for candidate stoppers by increasing the probing sphere
    // exponentially until we exceed the diameter of the dataset
    do {
      eigen_vector new_location = location + std::exp2(num_iter) * direction;
      sq_radius = (new_location - point_on_sphere).squaredNorm();
      _end = pc.radius_search(new_location, sq_radius, _current);
      _end = std::remove_if(_current, _end, ignoreFn);
      ++num_iter;
    } while(_end == _current && sq_radius < sq_diameter);
    // if we increased the probing sphere all the way to the diameter of the
    // dataset and still didn't find any containing points, it is likely we are
    // on the boundary floating to infinity, but still, it is technically 
    // possible we pick up a point very far outside because of a sliver, so we
    // have to check all points of the dataset, to be safe
    if (_end == _current) {
      _end = std::next(_current, pc.size());
      std::iota(_current, _end, 0);
      _end = std::remove_if(_current, _end, ignoreFn);
    }
  }
  
  eigen_map const* operator()(size_type * idx_ptr) {
    assert(idx_ptr);
    eigen_map const* r;
    if (_current != _end) {
      *idx_ptr = *_current++;
      r = &_pc[*idx_ptr];
    } else {
      r = nullptr;
    }
    return r;
  }
  
private:

  PointCloud const& _pc;
  ResultIterator    _current;
  ResultIterator    _end;
};

template <class PointCloud, class Derived1, class Derived2, class IgnoreFn,
          class Iterator>
vertex_filter<PointCloud, Iterator>
make_dt_filter(const PointCloud& pc,
               const Eigen::MatrixBase<Derived1>& driver,
               const Eigen::MatrixBase<Derived2>& member,
               const IgnoreFn& ignoreFn,
               Iterator result_begin) {
  using vf_type = vertex_filter<PointCloud, Iterator>;
  return vf_type(pc, driver, member, ignoreFn, result_begin);
}

}  // namespace FC

#endif  // VERTEX_FILTER_HPP_

