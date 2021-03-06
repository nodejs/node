#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "hdr_histogram.h"
#include "base_object.h"
#include "memory_tracker.h"
#include "node_messaging.h"
#include "util.h"
#include "v8.h"
#include "uv.h"

#include <functional>
#include <limits>
#include <map>
#include <string>

namespace node {

constexpr int kDefaultHistogramFigures = 3;

class Histogram : public MemoryRetainer {
 public:
  Histogram(
      int64_t lowest = 1,
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
  inline int64_t Exceeds() const { return exceeds_; }

  inline uint64_t RecordDelta();

  // Iterator is a function type that takes two doubles as argument, one for
  // percentile and one for the value at that percentile.
  template <typename Iterator>
  inline void Percentiles(Iterator&& fn);

  inline size_t GetMemorySize() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Histogram)
  SET_SELF_SIZE(Histogram)

 private:
  using HistogramPointer = DeleteFnPtr<hdr_histogram, hdr_close>;
  HistogramPointer histogram_;
  int64_t exceeds_ = 0;
  uint64_t prev_ = 0;

  Mutex mutex_;
};

class HistogramImpl {
 public:
  HistogramImpl(int64_t lowest, int64_t highest, int figures);
  explicit HistogramImpl(std::shared_ptr<Histogram> histogram);

  Histogram* operator->() { return histogram_.get(); }

 protected:
  const std::shared_ptr<Histogram>& histogram() const { return histogram_; }

 private:
  std::shared_ptr<Histogram> histogram_;
};

class HistogramBase : public BaseObject, public HistogramImpl {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
    Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  static BaseObjectPtr<HistogramBase> Create(
      Environment* env,
      int64_t lowest = 1,
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

  static BaseObjectPtr<HistogramBase> Create(
      Environment* env,
      std::shared_ptr<Histogram> histogram);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

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
  static void Record(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecordDelta(const v8::FunctionCallbackInfo<v8::Value>& args);

  HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      int64_t lowest = 1,
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

  HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      std::shared_ptr<Histogram> histogram);

  TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  class HistogramTransferData : public worker::TransferData {
   public:
    explicit HistogramTransferData(const HistogramBase* histogram)
        : histogram_(histogram->histogram()) {}

    explicit HistogramTransferData(std::shared_ptr<Histogram> histogram)
        : histogram_(std::move(histogram)) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(HistogramTransferData)
    SET_SELF_SIZE(HistogramTransferData)

   private:
    std::shared_ptr<Histogram> histogram_;
  };
};

class IntervalHistogram : public HandleWrap, public HistogramImpl {
 public:
  enum class StartFlags {
    NONE,
    RESET
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<IntervalHistogram> Create(
      Environment* env,
      int64_t lowest = 1,
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

  virtual void OnInterval() = 0;

  void MemoryInfo(MemoryTracker* tracker) const override;

  IntervalHistogram(
      Environment* env,
      v8::Local<v8::Object> wrap,
      AsyncWrap::ProviderType type,
      int32_t interval,
      int64_t lowest = 1,
      int64_t highest = std::numeric_limits<int64_t>::max(),
      int figures = kDefaultHistogramFigures);

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
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);

  TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  static void TimerCB(uv_timer_t* handle);
  void OnStart(StartFlags flags = StartFlags::RESET);
  void OnStop();

  bool enabled_ = false;
  int32_t interval_ = 0;
  uv_timer_t timer_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
