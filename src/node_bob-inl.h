#ifndef SRC_NODE_BOB_INL_H_
#define SRC_NODE_BOB_INL_H_

#include "node_bob.h"

#include <functional>

namespace node {
namespace bob {

template <typename T>
int SourceImpl<T>::Pull(
    Next<T> next,
    int options,
    T* data,
    size_t count,
    size_t max_count_hint) {

  int status;
  if (eos_) {
    status = bob::Status::STATUS_EOS;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  status = DoPull(std::move(next), options, data, count, max_count_hint);

  if (status == bob::Status::STATUS_END)
    eos_ = true;

  return status;
}

}  // namespace bob
}  // namespace node

#endif  // SRC_NODE_BOB_INL_H_
