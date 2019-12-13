#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
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

  // Iterator is a function type that takes two doubles as argument, one for
  // percentile and one for the value at that percentile.
  template <typename Iterator>
  inline void Percentiles(Iterator&& fn);

  size_t GetMemorySize() const {
    return hdr_get_memory_size(histogram_);
  }

 private:
  hdr_histogram* histogram_;
};

class HistogramBase : public BaseObject, public Histogram {
 public:
  virtual ~HistogramBase() = default;

  inline virtual void TraceDelta(int64_t delta) {}
  inline virtual void TraceExceeds(int64_t delta) {}

  inline bool RecordDelta();
  inline void ResetState();

  int64_t Exceeds() { return exceeds_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(HistogramBase)
  SET_SELF_SIZE(HistogramBase)

  static void HistogramMin(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramMax(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramMean(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramExceeds(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramStddev(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramPercentile(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramPercentiles(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HistogramReset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Initialize(Environment* env);

  static inline BaseObjectPtr<HistogramBase> New(
      Environment* env,
      int64_t lowest,
      int64_t highest,
      int figures = 3);

  inline HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      int64_t lowest,
      int64_t highest,
      int figures = 3);

 private:
  int64_t exceeds_ = 0;
  uint64_t prev_ = 0;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
