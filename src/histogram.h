#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "hdr_histogram.h"
#include <functional>
#include <map>

namespace node {

class Histogram {
 public:
  inline Histogram(int64_t lowest, int64_t highest, int figures = 3);
  inline virtual ~Histogram();

  inline bool Record(int64_t value);
  inline void Reset();
  inline int64_t Min();
  inline int64_t Max();
  inline double Mean();
  inline double Stddev();
  inline double Percentile(double percentile);
  inline void Percentiles(std::function<void(double, double)> fn);

  size_t GetMemorySize() const {
    return hdr_get_memory_size(histogram_);
  }

 private:
  hdr_histogram* histogram_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
