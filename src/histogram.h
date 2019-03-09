#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "hdr_histogram.h"
#include "tracing/traced_value.h"
#include "v8.h"

#include <functional>
#include <map>

namespace node {

class Histogram {
 public:
  inline Histogram(int64_t lowest, int64_t highest, int figures = 3);
  inline virtual ~Histogram();

  inline bool Record(int64_t value);
  inline void Reset();
  inline int64_t Count();
  inline int64_t Min();
  inline int64_t Max();
  inline double Mean();
  inline double Stddev();
  inline double Percentile(double percentile);
  inline void Percentiles(std::function<void(double, double)> fn);
  inline int64_t Exceeds();

  size_t GetMemorySize() const {
    return hdr_get_memory_size(histogram_);
  }

  inline void DumpToTracedValue(tracing::TracedValue* value);

  static inline void GetCount(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetMin(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetMax(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetMean(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetExceeds(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetStddev(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetPercentile(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline void GetPercentiles(
      Histogram* histogram,
      const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  hdr_histogram* histogram_;
  int64_t exceeds_;
  int64_t count_;
};

#define HISTOGRAM_FNS(V)                                                       \
  V(exceeds, Exceeds)                                                          \
  V(count, Count)                                                              \
  V(min, Min)                                                                  \
  V(max, Max)                                                                  \
  V(mean, Mean)                                                                \
  V(stddev, Stddev)                                                            \
  V(percentile, Percentile)                                                    \
  V(percentiles, Percentiles)                                                  \
  V(enable, Enable)                                                            \
  V(disable, Disable)                                                          \
  V(reset, Reset)

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
