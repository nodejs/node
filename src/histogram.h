#ifndef SRC_HISTOGRAM_H_
#define SRC_HISTOGRAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <hdr/hdr_histogram.h>
#include "base_object.h"
#include "memory_tracker.h"
#include "node_messaging.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <limits>

namespace node {

class ExternalReferenceRegistry;

template <typename T>
void StartHandleHistogram(v8::Local<v8::Value> receiver, bool reset);
template <typename T>
void StopHandleHistogram(v8::Local<v8::Value> receiver);

constexpr int kDefaultHistogramFigures = 3;

class Histogram : public MemoryRetainer {
 public:
  struct Options {
    int64_t lowest = 1;
    int64_t highest = std::numeric_limits<int64_t>::max();
    int figures = kDefaultHistogramFigures;
    double half_life = 0;   // EWMA half-life in number of samples (0 = off)
    int64_t threshold = 0;  // SLO threshold (0 = off). When set with
                            // half_life, tracks EWMA error rate for values
                            // exceeding this threshold.
  };

  explicit Histogram(const Options& options);
  virtual ~Histogram() = default;

  inline bool Record(int64_t value);
  inline void Reset();
  inline int64_t Min() const;
  inline int64_t Max() const;
  inline double Mean() const;
  inline double Stddev() const;
  inline double EwmaMean() const;
  inline double EwmaStddev() const;
  inline double EwmaErrorRate() const;
  inline int64_t Percentile(double percentile) const;
  inline size_t Exceeds() const;
  inline size_t Count() const;

  inline uint64_t RecordDelta();

  inline double Add(const Histogram& other);

  // Iterator is a function type that takes two doubles as argument, one for
  // percentile and one for the value at that percentile.
  template <typename Iterator>
  inline void Percentiles(Iterator&& fn) const;

  inline size_t GetMemorySize() const;

  // Analysis methods
  inline int64_t CountAt(int64_t value) const;
  double Cdf(int64_t value) const;
  double Skewness() const;
  double Kurtosis() const;
  double KsTest(const Histogram& other) const;
  double Subtract(const Histogram& other);
  void PercentilesAt(const double* percentiles,
                     int64_t* values,
                     size_t length) const;

  // Statistical hypothesis testing
  struct WelchTestResult {
    double t_statistic;
    double degrees_of_freedom;
    double p_value;
    double ci_lower;
    double ci_upper;
  };

  struct MannWhitneyResult {
    double u_statistic;
    double z_score;
    double p_value;
  };

  struct PercentileCIResult {
    int64_t value;
    int64_t lower;
    int64_t upper;
  };

  WelchTestResult WelchTest(const Histogram& other,
                            double confidence = 0.95) const;
  MannWhitneyResult MannWhitneyTest(const Histogram& other) const;
  double CohensD(const Histogram& other) const;
  double CliffsD(const Histogram& other) const;
  PercentileCIResult PercentileCI(double percentile,
                                  double confidence = 0.95) const;

  inline bool RecordCorrected(int64_t value, int64_t expected_interval);

  template <typename Iterator>
  void LinearBuckets(int64_t step_size, Iterator&& fn) const;

  template <typename Iterator>
  void LogBuckets(int64_t first_bucket, double log_base, Iterator&& fn) const;

  bool IsCompatible(const Histogram& other) const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Histogram)
  SET_SELF_SIZE(Histogram)

 private:
  inline void UpdateEwma(double value);

  using HistogramPointer = DeleteFnPtr<hdr_histogram, hdr_close>;
  HistogramPointer histogram_;
  uint64_t prev_ = 0;
  size_t exceeds_ = 0;

  // EWMA state (active when ewma_alpha_ > 0)
  double ewma_alpha_ = 0;
  double ewma_mean_ = 0;
  double ewma_variance_ = 0;
  bool ewma_initialized_ = false;

  // SLO error rate EWMA (active when threshold_ > 0 and ewma_alpha_ > 0)
  int64_t threshold_ = 0;
  double ewma_error_rate_ = 0;

  RwLock mutex_;
};

class HistogramImpl {
 public:
  enum InternalFields {
    kSlot = BaseObject::kSlot,
    kImplField = HandleWrap::kInternalFieldCount,
    kInternalFieldCount
  };

  explicit HistogramImpl(
      const Histogram::Options& options = Histogram::Options {});
  explicit HistogramImpl(std::shared_ptr<Histogram> histogram);

  Histogram* operator->() { return histogram_.get(); }

  const std::shared_ptr<Histogram>& histogram() const { return histogram_; }

  static void DoReset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCountBigInt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMinBigInt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMaxBigInt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetExceedsBigInt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCount(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMin(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMax(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMean(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetExceeds(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStddev(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentileBigInt(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentiles(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentilesBigInt(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetSkewness(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetKurtosis(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCdf(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCountAt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetKsTest(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentilesAt(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetLinearBuckets(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetLogBuckets(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetWelchTest(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMannWhitneyTest(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCohensD(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCliffsD(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPercentileCI(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEwmaMean(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEwmaStddev(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEwmaErrorRate(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void FastReset(v8::Local<v8::Value> receiver);
  static double FastGetCount(v8::Local<v8::Value> receiver);
  static double FastGetMin(v8::Local<v8::Value> receiver);
  static double FastGetMax(v8::Local<v8::Value> receiver);
  static double FastGetMean(v8::Local<v8::Value> receiver);
  static double FastGetExceeds(v8::Local<v8::Value> receiver);
  static double FastGetStddev(v8::Local<v8::Value> receiver);
  static double FastGetPercentile(v8::Local<v8::Value> receiver,
                                  const double percentile);
  static double FastGetSkewness(v8::Local<v8::Value> receiver);
  static double FastGetKurtosis(v8::Local<v8::Value> receiver);
  static double FastGetCdf(v8::Local<v8::Value> receiver, const int64_t value);
  static double FastGetCountAt(v8::Local<v8::Value> receiver,
                               const int64_t value);
  static double FastGetEwmaMean(v8::Local<v8::Value> receiver);
  static double FastGetEwmaStddev(v8::Local<v8::Value> receiver);
  static double FastGetEwmaErrorRate(v8::Local<v8::Value> receiver);

  static void AddMethods(v8::Isolate* isolate,
                         v8::Local<v8::FunctionTemplate> tmpl);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static HistogramImpl* FromJSObject(v8::Local<v8::Value> value);

 private:
  std::shared_ptr<Histogram> histogram_;

  static v8::CFunction fast_reset_;
  static v8::CFunction fast_get_count_;
  static v8::CFunction fast_get_min_;
  static v8::CFunction fast_get_max_;
  static v8::CFunction fast_get_mean_;
  static v8::CFunction fast_get_exceeds_;
  static v8::CFunction fast_get_stddev_;
  static v8::CFunction fast_get_percentile_;
  static v8::CFunction fast_get_skewness_;
  static v8::CFunction fast_get_kurtosis_;
  static v8::CFunction fast_get_cdf_;
  static v8::CFunction fast_get_count_at_;
  static v8::CFunction fast_get_ewma_mean_;
  static v8::CFunction fast_get_ewma_stddev_;
  static v8::CFunction fast_get_ewma_error_rate_;
};

class HistogramBase final : public BaseObject, public HistogramImpl {
 public:
  enum InternalFields {
    kInternalFieldCount = std::max<uint32_t>(
        BaseObject::kInternalFieldCount, HistogramImpl::kInternalFieldCount),
  };

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      IsolateData* isolate_data);
  static void Initialize(IsolateData* isolate_data,
                         v8::Local<v8::ObjectTemplate> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static BaseObjectPtr<HistogramBase> Create(
      Environment* env,
      const Histogram::Options& options = Histogram::Options {});

  static BaseObjectPtr<HistogramBase> Create(
      Environment* env,
      std::shared_ptr<Histogram> histogram);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(HistogramBase)
  SET_SELF_SIZE(HistogramBase)

  static void Record(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecordDelta(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RecordCorrected(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Add(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Subtract(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void FastRecord(v8::Local<v8::Value> receiver, const int64_t value);
  static void FastRecordDelta(v8::Local<v8::Value> receiver);

  HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      const Histogram::Options& options = Histogram::Options {});

  HistogramBase(
      Environment* env,
      v8::Local<v8::Object> wrap,
      std::shared_ptr<Histogram> histogram);

  BaseObject::TransferMode GetTransferMode() const override {
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

 private:
  static v8::CFunction fast_record_;
  static v8::CFunction fast_record_delta_;
};

// CRTP mixin for HandleWrap-based histograms with start/stop support.
// Provides: StartFlags enum, Start/Stop slow-path handlers, enabled_ flag,
// and InitTemplate (shared GetConstructorTemplate body).
// Derived must provide: fast_start_, fast_stop_ (static CFunction),
//   FastStart, FastStop, OnStart, OnStop.
template <typename Derived>
class HandleHistogramMixin {
 public:
  enum class StartFlags { NONE, RESET };

  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args) {
    StartHandleHistogram<Derived>(args.This(), args[0]->IsTrue());
  }

  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args) {
    StopHandleHistogram<Derived>(args.This());
  }

 protected:
  static void InitTemplate(v8::Isolate* isolate,
                           v8::Local<v8::FunctionTemplate> tmpl,
                           uint32_t internal_field_count) {
    auto instance = tmpl->InstanceTemplate();
    instance->SetInternalFieldCount(internal_field_count);
    HistogramImpl::AddMethods(isolate, tmpl);
    SetFastMethod(isolate, instance, "start", Start, &Derived::fast_start_);
    SetFastMethod(isolate, instance, "stop", Stop, &Derived::fast_stop_);
  }

  bool enabled_ = false;
};

class IntervalHistogram final : public HandleWrap,
                                public HistogramImpl,
                                public HandleHistogramMixin<IntervalHistogram> {
 public:
  enum InternalFields {
    kInternalFieldCount = std::max<uint32_t>(
        HandleWrap::kInternalFieldCount, HistogramImpl::kInternalFieldCount),
  };

  using OnInterval = void (*)(Histogram&);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<IntervalHistogram> Create(
      Environment* env,
      int32_t interval,
      OnInterval on_interval,
      const Histogram::Options& options,
      AsyncWrap::ProviderType type = AsyncWrap::PROVIDER_ELDHISTOGRAM);

  IntervalHistogram(Environment* env,
                    v8::Local<v8::Object> wrap,
                    AsyncWrap::ProviderType type,
                    int32_t interval,
                    OnInterval on_interval,
                    const Histogram::Options& options = Histogram::Options{});

  static void FastStart(v8::Local<v8::Value> receiver, bool reset);
  static void FastStop(v8::Local<v8::Value> receiver);

  BaseObject::TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(IntervalHistogram)
  SET_SELF_SIZE(IntervalHistogram)

 private:
  static void TimerCB(uv_timer_t* handle);
  void OnStart(StartFlags flags = StartFlags::RESET);
  void OnStop();

  friend class HandleHistogramMixin<IntervalHistogram>;
  template <typename T>
  friend void StartHandleHistogram(v8::Local<v8::Value>, bool);
  template <typename T>
  friend void StopHandleHistogram(v8::Local<v8::Value>);

  int32_t interval_ = 0;
  OnInterval on_interval_ = nullptr;
  uv_timer_t timer_;

  static v8::CFunction fast_start_;
  static v8::CFunction fast_stop_;
};

class IterationHistogram final
    : public HandleWrap,
      public HistogramImpl,
      public HandleHistogramMixin<IterationHistogram> {
 public:
  enum InternalFields {
    kInternalFieldCount = std::max<uint32_t>(
        HandleWrap::kInternalFieldCount, HistogramImpl::kInternalFieldCount),
  };

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<IterationHistogram> Create(
      Environment* env,
      const Histogram::Options& options,
      AsyncWrap::ProviderType type = AsyncWrap::PROVIDER_ELDHISTOGRAM);

  IterationHistogram(Environment* env,
                     v8::Local<v8::Object> wrap,
                     AsyncWrap::ProviderType type,
                     const Histogram::Options& options = Histogram::Options{});

  static void FastStart(v8::Local<v8::Value> receiver, bool reset);
  static void FastStop(v8::Local<v8::Value> receiver);

  BaseObject::TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  void Close(
      v8::Local<v8::Value> close_callback = v8::Local<v8::Value>()) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(IterationHistogram)
  SET_SELF_SIZE(IterationHistogram)

 private:
  static void PrepareCB(uv_prepare_t* handle);
  static void CheckCB(uv_check_t* handle);
  void OnStart(StartFlags flags = StartFlags::RESET);
  void OnStop();

  friend class HandleHistogramMixin<IterationHistogram>;
  template <typename T>
  friend void StartHandleHistogram(v8::Local<v8::Value>, bool);
  template <typename T>
  friend void StopHandleHistogram(v8::Local<v8::Value>);

  uv_prepare_t prepare_handle_;
  uv_check_t check_handle_;
  uint64_t prepare_time_ = 0;
  uint64_t check_time_ = 0;
  int64_t timeout_ = 0;

  static v8::CFunction fast_start_;
  static v8::CFunction fast_stop_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_H_
