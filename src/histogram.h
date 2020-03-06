#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "hdr_histogram.h"
#include "base_object.h"
#include "util.h"

#include <functional>
#include <limits>
#include <map>

namespace node {

constexpr int kDefaultHistogramFigures = 3;

class Histogram {
 public:
  Histogram(
      int64_t lowest = std::numeric_limits<int64_t>::min(),
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);
  virtual ~Histogram() = default;

  inline bool Record(int64_t value);
  inline void Reset();
  inline int64_t Min();
  inline int64_t Max();
  inline double Mean();
  inline double Stddev();
  inline double Percentile(double percentile);

  // Iterator is a function type that takes two doubles as argument, one for
  // percentile and one for the value at that percentile.
  template <typename Iterator>
  inline void Percentiles(Iterator&& fn);

  size_t GetMemorySize() const {
    return hdr_get_memory_size(histogram_.get());
  }

 private:
  using HistogramPointer = DeleteFnPtr<hdr_histogram, hdr_close>;
  HistogramPointer histogram_;
};

class HistogramBase : public BaseObject, public Histogram {
 public:
  virtual ~HistogramBase() = default;

  virtual void TraceDelta(int64_t delta) {}
  virtual void TraceExceeds(int64_t delta) {}

  inline bool RecordDelta();
  inline void ResetState();

  int64_t Exceeds() const { return exceeds_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(HistogramBase)
  SET_SELF_SIZE(HistogramBase)

  static void GetMin(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMax(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMean(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetExceeds(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStddev(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentile(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentiles(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoReset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Initialize(Environment* env);

  static BaseObjectPtr<HistogramBase> New(
      Environment* env,
      int64_t lowest = std::numeric_limits<int64_t>::min(),
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

  HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      int64_t lowest = std::numeric_limits<int64_t>::min(),
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

 private:
  int64_t exceeds_ = 0;
  uint64_t prev_ = 0;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
