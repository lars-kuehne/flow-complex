#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <cassert>

#include <algorithm>
#include <ostream>
#include <iterator>
#include <utility>

#include <Eigen/Core>

#include "affine_hull.hpp"
#include "ascend_task.hpp"
#include "critical_point.hpp"
#include "descend_task.hpp"
#include "logger.hpp"
#include "update_ray.hpp"

namespace FC {

template <class Derived, class Iterator, class Condition>
Iterator get_cond_offsets(Eigen::MatrixBase<Derived> const& lambda,
                          Iterator cond_begin, Condition cond) {
  Iterator cond_end = cond_begin;
  using lambda_t = Eigen::MatrixBase<Derived>;
  for (typename lambda_t::Index i = lambda.size(); i-- > 0;)
    if (cond(lambda[i]))
      *(cond_end++) = i;
  return cond_end;
}

template <class Derived, class Iterator>
Iterator get_pos_offsets(Eigen::MatrixBase<Derived> const& lambda,
                         Iterator pos_begin) {
  using number_t = typename Eigen::MatrixBase<Derived>::Scalar;
  return get_cond_offsets(lambda, pos_begin, [](number_t a) {return a > 0;});
}

template <class Derived, class Iterator>
Iterator get_neg_offsets(Eigen::MatrixBase<Derived> const& lambda,
                         Iterator neg_begin) {
  using number_t = typename Eigen::MatrixBase<Derived>::Scalar;
  return get_cond_offsets(lambda, neg_begin, [](number_t a) {return a < 0;});
}

/**
  @brief Spawns descend tasks for every sub-simplex of ah that is obtained
         by dropping an index from the given range of positive indices.
  @param dth Function object that spawns the given descend_task
  @param drop_pos_begin, drop_pos_end iterators over the position offsets from
                                      ah.begin() that shall be dropped
  @param x the starting position of the yet to spawn descend tasks
  @param ah the simplex to drop from
  @param succ the pointer of the successor in the Hasse-diagram for the new
              descend tasks
*/
template <class DTHandler, class PointCloud, typename number_type,
          typename size_type, class Iterator>
void spawn_sub_descends(DTHandler & dth,
                        Iterator drop_pos_begin, Iterator drop_pos_end,
                        Eigen::Matrix<number_type, Eigen::Dynamic, 1> && x,
                        affine_hull<PointCloud> ah,
                        critical_point<number_type, size_type> const* succ) {
  // TODO maybe check before every descend if the cp exists already. If so
  //      the descend would be successful and does not need to be scheduled
  //      Instead, just a (possibly) new successor needs to be added.
  using dt = descend_task<PointCloud>;
  // we skip the first position to use it as the "move-case" after the loop
  assert(drop_pos_begin != drop_pos_end);
  for (Iterator it = std::next(drop_pos_begin); it != drop_pos_end; ++it) {
    auto new_ah(ah);
    new_ah.drop_point(new_ah.begin() + *it);
    auto const& dropped_idx = *(ah.begin() + *it);
    Logger() << "DT takes dropped idx = " << dropped_idx << std::endl;
    using eigen_vector = Eigen::Matrix<number_type, Eigen::Dynamic, 1>;
    dth(dt(std::move(new_ah), eigen_vector(x), succ, dropped_idx));
  }
  assert(drop_pos_begin != drop_pos_end);
  size_type dropped_idx = *(ah.begin() + *drop_pos_begin);
  Logger() << "DT takes dropped idx = " << dropped_idx << std::endl;
  ah.drop_point(ah.begin() + *drop_pos_begin);
  dth(dt(std::move(ah), std::move(x), succ, dropped_idx));
}

}  // namespace FC

#endif  // COMMON_HPP_

