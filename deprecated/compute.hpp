#ifndef COMPUTE_HPP_
#define COMPUTE_HPP_

#include <functional>
#include <stack>
#include <utility>

#include <tbb/concurrent_unordered_set.h>

#include "ascend_task.hpp"
#include "descend_task.hpp"
#include "flow_complex.hpp"
#include "logger.hpp"
#include "point_cloud.hpp"
#include "utility.hpp"

namespace FC {

template <class task_t, class... Params>
void process_task(std::stack<task_t> & ts, Params && ... p) {
  task_t t(std::move(ts.top()));
  ts.pop();
  t.execute(std::forward<Params>(p)...);
}

template <typename size_type,    // type that is capable of holding the indices
                                 // of the critical points
          bool Aligned = false,
          typename PointIterator,
          typename dim_type,
          typename num_threads_t>
flow_complex<typename base_t<decltype((*PointIterator())[0])>::type,
             size_type>
compute_flow_complex (PointIterator begin, PointIterator end,
                      dim_type dim,
                      num_threads_t /*num_threads*/) {
  Logger() << "*****************COMPUTE-START*************************\n";
  using number_type = typename base_t<decltype((*PointIterator())[0])>::type;
  using fc_type = flow_complex<number_type, size_type>;
  using pc_type = point_cloud<number_type, size_type, Aligned>;
  using at_type = ascend_task<pc_type>;
  using dt_type = descend_task<pc_type>;
  using ci_type = circumsphere_ident<size_type>;

  // 1) init data structures
  pc_type pc(begin, end, dim);
  fc_type fc(dim, pc.size());
  std::stack<at_type>  qa;
  std::stack<dt_type>  qd;
  using ci_container = tbb::concurrent_unordered_set<ci_type, CIHash>;
  ci_container infproxy_cont;
  ci_container dci;
  // 2) create the handlers for task communication
  auto ath = [&qa] (at_type && at) {qa.push(std::move(at));};
  auto dth = [&qd] (dt_type && dt) {qd.push(std::move(dt));};
  auto cih = [] (ci_container & ci_store, ci_type ci) {
    return ci_store.insert(ci).second;
  };
  auto acih = std::bind(cih, std::ref(infproxy_cont), std::placeholders::_1);
  auto dcih = std::bind(cih, std::ref(dci), std::placeholders::_1);
  // 3) seed initial ascend task(s)
  qa.emplace(pc);
  // 4) process all tasks - qd first = depth first search
  while (not qa.empty() or not qd.empty()) {
    if (not qd.empty())
      process_task(qd, ath, dth, fc, dcih);
    else
      process_task(qa, ath, dth, fc, acih);
  }
  return fc;
}

}  // namespace FC

#endif  // COMPUTE_HPP_
