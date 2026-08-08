#include "histogram.h"  // NOLINT(build/include_inline)
#include "base_object-inl.h"
#include "histogram-inl.h"
#include "memory_tracker-inl.h"
#include "node_debug.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util.h"

#include <vector>

namespace node {

using v8::BigInt;
using v8::CFunction;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint32;
using v8::Value;

template <typename T>
void StartHandleHistogram(Local<Value> receiver, bool reset) {
  T* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, receiver);
  histogram->OnStart(reset ? T::StartFlags::RESET : T::StartFlags::NONE);
}

template <typename T>
void StopHandleHistogram(Local<Value> receiver) {
  T* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, receiver);
  histogram->OnStop();
}

Histogram::Histogram(const Options& options) {
  hdr_histogram* histogram;
  CHECK_EQ(0, hdr_init(options.lowest,
                       options.highest,
                       options.figures,
                       &histogram));
  histogram_.reset(histogram);
}

void Histogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("histogram", GetMemorySize());
}

bool Histogram::IsCompatible(const Histogram& other) const {
  return histogram_->counts_len == other.histogram_->counts_len &&
         histogram_->lowest_discernible_value ==
             other.histogram_->lowest_discernible_value &&
         histogram_->highest_trackable_value ==
             other.histogram_->highest_trackable_value &&
         histogram_->significant_figures ==
             other.histogram_->significant_figures;
}

double Histogram::Cdf(int64_t value) const {
  RwLock::ScopedReadLock lock(mutex_);
  int64_t total = histogram_->total_count;
  if (total == 0) return 0.0;

  hdr_iter iter;
  hdr_iter_init(&iter, histogram_.get());
  while (hdr_iter_next(&iter)) {
    if (iter.highest_equivalent_value >= value) {
      return static_cast<double>(iter.cumulative_count) /
             static_cast<double>(total);
    }
    // All recorded data accounted for; remaining buckets are empty.
    if (iter.cumulative_count >= total) break;
  }
  return 1.0;
}

double Histogram::Skewness() const {
  RwLock::ScopedReadLock lock(mutex_);
  int64_t total = histogram_->total_count;
  if (total < 3) return 0.0;

  // Compute mean in one pass, then variance and skewness in a second
  // pass.  This avoids calling hdr_stddev (which internally recomputes
  // hdr_mean), reducing the total from 4 iterations to 2.
  double mean = hdr_mean(histogram_.get());

  double m2 = 0.0;
  double m3 = 0.0;
  hdr_iter iter;
  hdr_iter_recorded_init(&iter, histogram_.get());
  while (hdr_iter_next(&iter)) {
    double dev = static_cast<double>(hdr_median_equivalent_value(
                     histogram_.get(), iter.value)) -
                 mean;
    double d2 = dev * dev;
    m2 += static_cast<double>(iter.count) * d2;
    m3 += static_cast<double>(iter.count) * d2 * dev;
  }

  double n = static_cast<double>(total);
  double variance = m2 / n;
  if (variance == 0.0) return 0.0;
  double s3 = variance * std::sqrt(variance);  // stddev^3
  return (m3 / n) / s3;
}

double Histogram::Kurtosis() const {
  RwLock::ScopedReadLock lock(mutex_);
  int64_t total = histogram_->total_count;
  if (total < 4) return 0.0;

  // Same single-pass approach as Skewness: compute mean first, then
  // variance and excess kurtosis together in one iteration.
  double mean = hdr_mean(histogram_.get());

  double m2 = 0.0;
  double m4 = 0.0;
  hdr_iter iter;
  hdr_iter_recorded_init(&iter, histogram_.get());
  while (hdr_iter_next(&iter)) {
    double dev = static_cast<double>(hdr_median_equivalent_value(
                     histogram_.get(), iter.value)) -
                 mean;
    double d2 = dev * dev;
    m2 += static_cast<double>(iter.count) * d2;
    m4 += static_cast<double>(iter.count) * d2 * d2;
  }

  double n = static_cast<double>(total);
  double variance = m2 / n;
  if (variance == 0.0) return 0.0;
  double s4 = variance * variance;  // stddev^4
  return (m4 / n) / s4 - 3.0;
}

double Histogram::Subtract(const Histogram& other) {
  auto do_subtract = [&]() -> double {
    int64_t dropped = 0;
    int32_t len =
        std::min(histogram_->counts_len, other.histogram_->counts_len);
    for (int32_t i = 0; i < len; i++) {
      int64_t count = histogram_->counts[i] - other.histogram_->counts[i];
      if (count < 0) {
        dropped += -count;
        count = 0;
      }
      histogram_->counts[i] = count;
    }
    hdr_reset_internal_counters(histogram_.get());
    exceeds_ = (exceeds_ > other.exceeds_) ? exceeds_ - other.exceeds_ : 0;
    return static_cast<double>(dropped);
  };

  if (this == &other) {
    RwLock::ScopedWriteLock lock(mutex_);
    return do_subtract();
  }

  if (this < &other) {
    RwLock::ScopedWriteLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_subtract();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedWriteLock lock2(mutex_);
  return do_subtract();
}

double Histogram::KsTest(const Histogram& other) const {
  auto do_ks = [&]() -> double {
    int64_t n1 = histogram_->total_count;
    int64_t n2 = other.histogram_->total_count;
    if (n1 == 0 || n2 == 0) return 0.0;

    double max_d = 0.0;
    int64_t cum1 = 0, cum2 = 0;
    int32_t len =
        std::max(histogram_->counts_len, other.histogram_->counts_len);

    for (int32_t i = 0; i < len; i++) {
      if (i < histogram_->counts_len) cum1 += histogram_->counts[i];
      if (i < other.histogram_->counts_len) cum2 += other.histogram_->counts[i];
      double cdf1 = static_cast<double>(cum1) / static_cast<double>(n1);
      double cdf2 = static_cast<double>(cum2) / static_cast<double>(n2);
      double d = cdf1 > cdf2 ? cdf1 - cdf2 : cdf2 - cdf1;
      if (d > max_d) max_d = d;
    }
    return max_d;
  };

  if (this == &other) return 0.0;

  if (this < &other) {
    RwLock::ScopedReadLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_ks();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedReadLock lock2(mutex_);
  return do_ks();
}

void Histogram::PercentilesAt(const double* percentiles,
                              int64_t* values,
                              size_t length) const {
  RwLock::ScopedReadLock lock(mutex_);
  hdr_value_at_percentiles(histogram_.get(), percentiles, values, length);
}

HistogramImpl::HistogramImpl(const Histogram::Options& options)
    : histogram_(new Histogram(options)) {}

HistogramImpl::HistogramImpl(std::shared_ptr<Histogram> histogram)
    : histogram_(std::move(histogram)) {}

CFunction HistogramImpl::fast_reset_(
    CFunction::Make(&HistogramImpl::FastReset));
CFunction HistogramImpl::fast_get_count_(
    CFunction::Make(&HistogramImpl::FastGetCount));
CFunction HistogramImpl::fast_get_min_(
    CFunction::Make(&HistogramImpl::FastGetMin));
CFunction HistogramImpl::fast_get_max_(
    CFunction::Make(&HistogramImpl::FastGetMax));
CFunction HistogramImpl::fast_get_mean_(
    CFunction::Make(&HistogramImpl::FastGetMean));
CFunction HistogramImpl::fast_get_exceeds_(
    CFunction::Make(&HistogramImpl::FastGetExceeds));
CFunction HistogramImpl::fast_get_stddev_(
    CFunction::Make(&HistogramImpl::FastGetStddev));
CFunction HistogramImpl::fast_get_percentile_(
    CFunction::Make(&HistogramImpl::FastGetPercentile));
CFunction HistogramImpl::fast_get_skewness_(
    CFunction::Make(&HistogramImpl::FastGetSkewness));
CFunction HistogramImpl::fast_get_kurtosis_(
    CFunction::Make(&HistogramImpl::FastGetKurtosis));
CFunction HistogramImpl::fast_get_cdf_(
    CFunction::Make(&HistogramImpl::FastGetCdf));
CFunction HistogramImpl::fast_get_count_at_(
    CFunction::Make(&HistogramImpl::FastGetCountAt));
CFunction HistogramBase::fast_record_(
    CFunction::Make(&HistogramBase::FastRecord));
CFunction HistogramBase::fast_record_delta_(
    CFunction::Make(&HistogramBase::FastRecordDelta));
CFunction IntervalHistogram::fast_start_(
    CFunction::Make(&IntervalHistogram::FastStart));
CFunction IntervalHistogram::fast_stop_(
    CFunction::Make(&IntervalHistogram::FastStop));
CFunction IterationHistogram::fast_start_(
    CFunction::Make(&IterationHistogram::FastStart));
CFunction IterationHistogram::fast_stop_(
    CFunction::Make(&IterationHistogram::FastStop));

void HistogramImpl::AddMethods(Isolate* isolate, Local<FunctionTemplate> tmpl) {
  // TODO(@jasnell): The bigint API variations do not yet support fast
  // variations since v8 will not return a bigint value from a fast method.
  SetProtoMethodNoSideEffect(isolate, tmpl, "countBigInt", GetCountBigInt);
  SetProtoMethodNoSideEffect(isolate, tmpl, "exceedsBigInt", GetExceedsBigInt);
  SetProtoMethodNoSideEffect(isolate, tmpl, "minBigInt", GetMinBigInt);
  SetProtoMethodNoSideEffect(isolate, tmpl, "maxBigInt", GetMaxBigInt);
  SetProtoMethodNoSideEffect(
      isolate, tmpl, "percentileBigInt", GetPercentileBigInt);
  SetProtoMethodNoSideEffect(isolate, tmpl, "percentiles", GetPercentiles);
  SetProtoMethodNoSideEffect(
      isolate, tmpl, "percentilesBigInt", GetPercentilesBigInt);
  auto instance = tmpl->InstanceTemplate();
  SetFastMethodNoSideEffect(
      isolate, instance, "count", GetCount, &fast_get_count_);
  SetFastMethodNoSideEffect(
      isolate, instance, "exceeds", GetExceeds, &fast_get_exceeds_);
  SetFastMethodNoSideEffect(isolate, instance, "min", GetMin, &fast_get_min_);
  SetFastMethodNoSideEffect(isolate, instance, "max", GetMax, &fast_get_max_);
  SetFastMethodNoSideEffect(
      isolate, instance, "mean", GetMean, &fast_get_mean_);
  SetFastMethodNoSideEffect(
      isolate, instance, "stddev", GetStddev, &fast_get_stddev_);
  SetFastMethodNoSideEffect(
      isolate, instance, "percentile", GetPercentile, &fast_get_percentile_);
  SetFastMethodNoSideEffect(
      isolate, instance, "skewness", GetSkewness, &fast_get_skewness_);
  SetFastMethodNoSideEffect(
      isolate, instance, "kurtosis", GetKurtosis, &fast_get_kurtosis_);
  SetFastMethodNoSideEffect(isolate, instance, "cdf", GetCdf, &fast_get_cdf_);
  SetFastMethodNoSideEffect(
      isolate, instance, "countAt", GetCountAt, &fast_get_count_at_);
  SetProtoMethodNoSideEffect(isolate, tmpl, "ksTest", GetKsTest);
  SetProtoMethodNoSideEffect(isolate, tmpl, "percentilesAt", GetPercentilesAt);
  SetProtoMethodNoSideEffect(isolate, tmpl, "linearBuckets", GetLinearBuckets);
  SetProtoMethodNoSideEffect(isolate, tmpl, "logBuckets", GetLogBuckets);
  SetFastMethod(isolate, instance, "reset", DoReset, &fast_reset_);
}

void HistogramImpl::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  static bool is_registered = false;
  if (is_registered) return;
  registry->Register(GetCount);
  registry->Register(GetCountBigInt);
  registry->Register(GetExceeds);
  registry->Register(GetExceedsBigInt);
  registry->Register(GetMin);
  registry->Register(GetMinBigInt);
  registry->Register(GetMax);
  registry->Register(GetMaxBigInt);
  registry->Register(GetMean);
  registry->Register(GetStddev);
  registry->Register(GetPercentile);
  registry->Register(GetPercentileBigInt);
  registry->Register(GetPercentiles);
  registry->Register(GetPercentilesBigInt);
  registry->Register(DoReset);
  registry->Register(fast_reset_);
  registry->Register(fast_get_count_);
  registry->Register(fast_get_min_);
  registry->Register(fast_get_max_);
  registry->Register(fast_get_mean_);
  registry->Register(fast_get_exceeds_);
  registry->Register(fast_get_stddev_);
  registry->Register(fast_get_percentile_);
  registry->Register(GetSkewness);
  registry->Register(GetKurtosis);
  registry->Register(GetCdf);
  registry->Register(GetCountAt);
  registry->Register(GetKsTest);
  registry->Register(GetPercentilesAt);
  registry->Register(GetLinearBuckets);
  registry->Register(GetLogBuckets);
  registry->Register(fast_get_skewness_);
  registry->Register(fast_get_kurtosis_);
  registry->Register(fast_get_cdf_);
  registry->Register(fast_get_count_at_);
  is_registered = true;
}

HistogramBase::HistogramBase(
    Environment* env,
    Local<Object> wrap,
    const Histogram::Options& options)
    : BaseObject(env, wrap),
      HistogramImpl(options) {
  MakeWeak();
  wrap->SetAlignedPointerInInternalField(
      HistogramImpl::InternalFields::kImplField,
      static_cast<HistogramImpl*>(this),
      EmbedderDataTag::kDefault);
}

HistogramBase::HistogramBase(
    Environment* env,
    Local<Object> wrap,
    std::shared_ptr<Histogram> histogram)
    : BaseObject(env, wrap),
      HistogramImpl(std::move(histogram)) {
  MakeWeak();
  wrap->SetAlignedPointerInInternalField(
      HistogramImpl::InternalFields::kImplField,
      static_cast<HistogramImpl*>(this),
      EmbedderDataTag::kDefault);
}

void HistogramBase::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram());
}

void HistogramBase::RecordDelta(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());
  (*histogram)->RecordDelta();
}

void HistogramBase::FastRecordDelta(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.recordDelta");
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, receiver);
  (*histogram)->RecordDelta();
}

void HistogramBase::Record(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  bool lossless = true;
  int64_t value = args[0]->IsBigInt()
      ? args[0].As<BigInt>()->Int64Value(&lossless)
      : static_cast<int64_t>(args[0].As<Number>()->Value());
  if (!lossless || value < 1)
    return THROW_ERR_OUT_OF_RANGE(env, "value is out of range");
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());
  (*histogram)->Record(value);
}

void HistogramBase::FastRecord(Local<Value> receiver, const int64_t value) {
  CHECK_GE(value, 1);
  TRACK_V8_FAST_API_CALL("histogram.record");
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, receiver);
  (*histogram)->Record(value);
}

void HistogramBase::Add(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());

  CHECK(GetConstructorTemplate(env->isolate_data())->HasInstance(args[0]));
  HistogramBase* other;
  ASSIGN_OR_RETURN_UNWRAP(&other, args[0]);

  double count = (*histogram)->Add(*(other->histogram()));
  args.GetReturnValue().Set(count);
}

void HistogramBase::Subtract(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());

  CHECK(GetConstructorTemplate(env->isolate_data())->HasInstance(args[0]));
  HistogramBase* other;
  ASSIGN_OR_RETURN_UNWRAP(&other, args[0]);

  double dropped = (*histogram)->Subtract(*(other->histogram()));
  args.GetReturnValue().Set(dropped);
}

void HistogramBase::RecordCorrected(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  CHECK_IMPLIES(!args[1]->IsNumber(), args[1]->IsBigInt());
  bool lossless = true;
  int64_t value = args[0]->IsBigInt()
                      ? args[0].As<BigInt>()->Int64Value(&lossless)
                      : static_cast<int64_t>(args[0].As<Number>()->Value());
  if (!lossless || value < 1)
    return THROW_ERR_OUT_OF_RANGE(env, "value is out of range");
  int64_t expected_interval =
      args[1]->IsBigInt() ? args[1].As<BigInt>()->Int64Value(&lossless)
                          : static_cast<int64_t>(args[1].As<Number>()->Value());
  if (!lossless || expected_interval < 1)
    return THROW_ERR_OUT_OF_RANGE(env, "expected_interval is out of range");
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());
  (*histogram)->RecordCorrected(value, expected_interval);
}

BaseObjectPtr<HistogramBase> HistogramBase::Create(
    Environment* env,
    const Histogram::Options& options) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env->isolate_data())
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<HistogramBase>(env, obj, options);
}

BaseObjectPtr<HistogramBase> HistogramBase::Create(
    Environment* env,
    std::shared_ptr<Histogram> histogram) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env->isolate_data())
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }
  return MakeBaseObject<HistogramBase>(env, obj, std::move(histogram));
}

void HistogramBase::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);

  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  CHECK_IMPLIES(!args[1]->IsNumber(), args[1]->IsBigInt());
  CHECK(args[2]->IsUint32());

  int64_t lowest = 1;
  int64_t highest = std::numeric_limits<int64_t>::max();

  bool lossless = true;

  if (args[0]->IsNumber()) {
    lowest = args[0].As<Integer>()->Value();
  } else if (args[0]->IsBigInt()) {
    lowest = args[0].As<BigInt>()->Int64Value(&lossless);
    if (!lossless)
      return THROW_ERR_OUT_OF_RANGE(env, "options.lowest is out of range");
  }

  if (args[1]->IsNumber()) {
    highest = args[1].As<Integer>()->Value();
  } else if (args[1]->IsBigInt()) {
    highest = args[1].As<BigInt>()->Int64Value(&lossless);
    if (!lossless)
      return THROW_ERR_OUT_OF_RANGE(env, "options.highest is out of range");
  }

  int32_t figures = args[2].As<Uint32>()->Value();
  new HistogramBase(env, args.This(), Histogram::Options {
    lowest, highest, figures
  });
}

Local<FunctionTemplate> HistogramBase::GetConstructorTemplate(
    IsolateData* isolate_data) {
  Local<FunctionTemplate> tmpl = isolate_data->histogram_ctor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = isolate_data->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    Local<String> classname = FIXED_ONE_BYTE_STRING(isolate, "Histogram");
    tmpl->SetClassName(classname);
    auto instance = tmpl->InstanceTemplate();
    instance->SetInternalFieldCount(HistogramBase::kInternalFieldCount);
    SetFastMethod(isolate, instance, "record", Record, &fast_record_);
    SetFastMethod(
        isolate, instance, "recordDelta", RecordDelta, &fast_record_delta_);
    SetProtoMethod(isolate, tmpl, "add", Add);
    SetProtoMethod(isolate, tmpl, "subtract", Subtract);
    SetProtoMethod(isolate, tmpl, "recordCorrected", RecordCorrected);
    HistogramImpl::AddMethods(isolate, tmpl);
    isolate_data->set_histogram_ctor_template(tmpl);
  }
  return tmpl;
}

void HistogramBase::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Add);
  registry->Register(Subtract);
  registry->Register(Record);
  registry->Register(RecordDelta);
  registry->Register(RecordCorrected);
  registry->Register(fast_record_);
  registry->Register(fast_record_delta_);
  HistogramImpl::RegisterExternalReferences(registry);
}

void HistogramBase::Initialize(IsolateData* isolate_data,
                               Local<ObjectTemplate> target) {
  SetConstructorFunction(isolate_data->isolate(),
                         target,
                         "Histogram",
                         GetConstructorTemplate(isolate_data),
                         SetConstructorFunctionFlag::NONE);
}

BaseObjectPtr<BaseObject> HistogramBase::HistogramTransferData::Deserialize(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<worker::TransferData> self) {
  return Create(env, std::move(histogram_));
}

std::unique_ptr<worker::TransferData> HistogramBase::CloneForMessaging() const {
  return std::make_unique<HistogramTransferData>(this);
}

void HistogramBase::HistogramTransferData::MemoryInfo(
    MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram_);
}

Local<FunctionTemplate> IntervalHistogram::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->intervalhistogram_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Histogram"));
    InitTemplate(isolate, tmpl, IntervalHistogram::kInternalFieldCount);
    env->set_intervalhistogram_constructor_template(tmpl);
  }
  return tmpl;
}

void IntervalHistogram::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Start);
  registry->Register(Stop);
  registry->Register(fast_start_);
  registry->Register(fast_stop_);
  HistogramImpl::RegisterExternalReferences(registry);
}

IntervalHistogram::IntervalHistogram(Environment* env,
                                     Local<Object> wrap,
                                     AsyncWrap::ProviderType type,
                                     int32_t interval,
                                     OnInterval on_interval,
                                     const Histogram::Options& options)
    : HandleWrap(env, wrap, reinterpret_cast<uv_handle_t*>(&timer_), type),
      HistogramImpl(options),
      interval_(interval),
      on_interval_(on_interval) {
  MakeWeak();
  wrap->SetAlignedPointerInInternalField(
      HistogramImpl::InternalFields::kImplField,
      static_cast<HistogramImpl*>(this),
      EmbedderDataTag::kDefault);
  uv_timer_init(env->event_loop(), &timer_);
}

BaseObjectPtr<IntervalHistogram> IntervalHistogram::Create(
    Environment* env,
    int32_t interval,
    OnInterval on_interval,
    const Histogram::Options& options,
    AsyncWrap::ProviderType type) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<IntervalHistogram>(
      env, obj, type, interval, on_interval, options);
}

void IntervalHistogram::TimerCB(uv_timer_t* handle) {
  IntervalHistogram* histogram =
      ContainerOf(&IntervalHistogram::timer_, handle);

  Histogram* h = histogram->histogram().get();

  histogram->on_interval_(*h);
}

void IntervalHistogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram());
}

void IntervalHistogram::OnStart(StartFlags flags) {
  if (enabled_ || IsHandleClosing()) return;
  enabled_ = true;
  if (flags == StartFlags::RESET)
    histogram()->Reset();
  uv_timer_start(&timer_, TimerCB, interval_, interval_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
}

void IntervalHistogram::OnStop() {
  if (!enabled_ || IsHandleClosing()) return;
  enabled_ = false;
  uv_timer_stop(&timer_);
}

void IntervalHistogram::FastStart(Local<Value> receiver, bool reset) {
  TRACK_V8_FAST_API_CALL("histogram.start");
  StartHandleHistogram<IntervalHistogram>(receiver, reset);
}

void IntervalHistogram::FastStop(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.stop");
  StopHandleHistogram<IntervalHistogram>(receiver);
}

Local<FunctionTemplate> IterationHistogram::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->iterationhistogram_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Histogram"));
    InitTemplate(isolate, tmpl, IterationHistogram::kInternalFieldCount);
    env->set_iterationhistogram_constructor_template(tmpl);
  }
  return tmpl;
}

void IterationHistogram::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Start);
  registry->Register(Stop);
  registry->Register(fast_start_);
  registry->Register(fast_stop_);
  HistogramImpl::RegisterExternalReferences(registry);
}

IterationHistogram::IterationHistogram(Environment* env,
                                       Local<Object> wrap,
                                       AsyncWrap::ProviderType type,
                                       const Histogram::Options& options)
    : HandleWrap(
          env, wrap, reinterpret_cast<uv_handle_t*>(&check_handle_), type),
      HistogramImpl(options) {
  MakeWeak();
  wrap->SetAlignedPointerInInternalField(
      HistogramImpl::InternalFields::kImplField,
      static_cast<HistogramImpl*>(this),
      EmbedderDataTag::kDefault);
  uv_check_init(env->event_loop(), &check_handle_);
  uv_prepare_init(env->event_loop(), &prepare_handle_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_handle_));
}

BaseObjectPtr<IterationHistogram> IterationHistogram::Create(
    Environment* env,
    const Histogram::Options& options,
    AsyncWrap::ProviderType type) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<IterationHistogram>(env, obj, type, options);
}

void IterationHistogram::PrepareCB(uv_prepare_t* handle) {
  IterationHistogram* self =
      ContainerOf(&IterationHistogram::prepare_handle_, handle);
  if (!self->enabled_) return;
  self->prepare_time_ = uv_hrtime();
  self->timeout_ = uv_backend_timeout(handle->loop);
}

void IterationHistogram::CheckCB(uv_check_t* handle) {
  IterationHistogram* self =
      ContainerOf(&IterationHistogram::check_handle_, handle);
  if (!self->enabled_) return;

  uint64_t check_time = uv_hrtime();
  uint64_t poll_time = check_time - self->prepare_time_;
  uint64_t latency = self->prepare_time_ - self->check_time_;

  if (self->timeout_ >= 0) {
    uint64_t timeout_ns = static_cast<uint64_t>(self->timeout_) * 1000 * 1000;
    if (poll_time > timeout_ns) {
      latency += poll_time - timeout_ns;
    }
  }

  self->histogram()->Record(latency == 0 ? 1 : latency);
  self->check_time_ = check_time;
}

void IterationHistogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram());
}

void IterationHistogram::OnStart(StartFlags flags) {
  if (enabled_ || IsHandleClosing()) return;
  enabled_ = true;
  if (flags == StartFlags::RESET) histogram()->Reset();
  check_time_ = uv_hrtime();
  prepare_time_ = check_time_;
  timeout_ = 0;
  uv_check_start(&check_handle_, CheckCB);
  uv_prepare_start(&prepare_handle_, PrepareCB);
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_handle_));
}

void IterationHistogram::OnStop() {
  if (!enabled_ || IsHandleClosing()) return;
  enabled_ = false;
  uv_check_stop(&check_handle_);
  uv_prepare_stop(&prepare_handle_);
}

void IterationHistogram::Close(Local<Value> close_callback) {
  if (IsHandleClosing()) return;
  OnStop();
  HandleWrap::Close(close_callback);
  uv_close(reinterpret_cast<uv_handle_t*>(&prepare_handle_), nullptr);
}

void IterationHistogram::FastStart(Local<Value> receiver, bool reset) {
  TRACK_V8_FAST_API_CALL("histogram.eventLoopDelay.start");
  StartHandleHistogram<IterationHistogram>(receiver, reset);
}

void IterationHistogram::FastStop(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.eventLoopDelay.stop");
  StopHandleHistogram<IterationHistogram>(receiver);
}

void HistogramImpl::GetCount(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  double value = static_cast<double>((*histogram)->Count());
  args.GetReturnValue().Set(value);
}

void HistogramImpl::GetCountBigInt(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set(
      BigInt::NewFromUnsigned(args.GetIsolate(), (*histogram)->Count()));
}

void HistogramImpl::GetMin(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  double value = static_cast<double>((*histogram)->Min());
  args.GetReturnValue().Set(value);
}

void HistogramImpl::GetMinBigInt(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set(
      BigInt::New(args.GetIsolate(), (*histogram)->Min()));
}

void HistogramImpl::GetMax(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  double value = static_cast<double>((*histogram)->Max());
  args.GetReturnValue().Set(value);
}

void HistogramImpl::GetMaxBigInt(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set(
      BigInt::New(args.GetIsolate(), (*histogram)->Max()));
}

void HistogramImpl::GetMean(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->Mean());
}

void HistogramImpl::GetExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  double value = static_cast<double>((*histogram)->Exceeds());
  args.GetReturnValue().Set(value);
}

void HistogramImpl::GetExceedsBigInt(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set(
      BigInt::New(args.GetIsolate(), (*histogram)->Exceeds()));
}

void HistogramImpl::GetStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->Stddev());
}

void HistogramImpl::GetPercentile(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  double value = static_cast<double>((*histogram)->Percentile(percentile));
  args.GetReturnValue().Set(value);
}

void HistogramImpl::GetPercentileBigInt(
    const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  int64_t value = (*histogram)->Percentile(percentile);
  args.GetReturnValue().Set(BigInt::New(args.GetIsolate(), value));
}

void HistogramImpl::GetPercentiles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();

  // Collect percentile data under the histogram lock, then populate the
  // V8 Map after releasing it to avoid V8 allocations under the lock.
  std::vector<std::pair<double, int64_t>> entries;
  (*histogram)->Percentiles([&entries](double key, int64_t value) {
    entries.emplace_back(key, value);
  });
  for (const auto& entry : entries) {
    USE(map->Set(
        env->context(),
        Number::New(env->isolate(), entry.first),
        Number::New(env->isolate(), static_cast<double>(entry.second))));
  }
}

void HistogramImpl::GetPercentilesBigInt(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();

  std::vector<std::pair<double, int64_t>> entries;
  (*histogram)->Percentiles([&entries](double key, int64_t value) {
    entries.emplace_back(key, value);
  });
  for (const auto& entry : entries) {
    USE(map->Set(env->context(),
                 Number::New(env->isolate(), entry.first),
                 BigInt::New(env->isolate(), entry.second)));
  }
}

void HistogramImpl::DoReset(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  (*histogram)->Reset();
}

void HistogramImpl::FastReset(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.reset");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  (*histogram)->Reset();
}

double HistogramImpl::FastGetCount(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.count");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->Count());
}

double HistogramImpl::FastGetMin(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.min");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->Min());
}

double HistogramImpl::FastGetMax(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.max");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->Max());
}

double HistogramImpl::FastGetMean(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.mean");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->Mean();
}

double HistogramImpl::FastGetExceeds(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.exceeds");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->Exceeds());
}

double HistogramImpl::FastGetStddev(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.stddev");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->Stddev();
}

double HistogramImpl::FastGetPercentile(Local<Value> receiver,
                                        const double percentile) {
  TRACK_V8_FAST_API_CALL("histogram.percentile");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->Percentile(percentile));
}

void HistogramImpl::GetSkewness(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->Skewness());
}

double HistogramImpl::FastGetSkewness(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.skewness");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->Skewness();
}

void HistogramImpl::GetKurtosis(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->Kurtosis());
}

double HistogramImpl::FastGetKurtosis(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.kurtosis");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->Kurtosis();
}

void HistogramImpl::GetCdf(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  int64_t value = static_cast<int64_t>(args[0].As<Number>()->Value());
  args.GetReturnValue().Set((*histogram)->Cdf(value));
}

double HistogramImpl::FastGetCdf(Local<Value> receiver, const int64_t value) {
  TRACK_V8_FAST_API_CALL("histogram.cdf");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->Cdf(value);
}

void HistogramImpl::GetCountAt(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  int64_t value = static_cast<int64_t>(args[0].As<Number>()->Value());
  double count = static_cast<double>((*histogram)->CountAt(value));
  args.GetReturnValue().Set(count);
}

double HistogramImpl::FastGetCountAt(Local<Value> receiver,
                                     const int64_t value) {
  TRACK_V8_FAST_API_CALL("histogram.countAt");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return static_cast<double>((*histogram)->CountAt(value));
}

void HistogramImpl::GetKsTest(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  HistogramImpl* other = HistogramImpl::FromJSObject(args[0]);
  args.GetReturnValue().Set((*histogram)->KsTest(*(other->histogram())));
}

void HistogramImpl::GetPercentilesAt(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  CHECK(args[1]->IsFloat64Array());
  Local<v8::Float64Array> input = args[1].As<v8::Float64Array>();
  size_t length = input->Length();
  auto backing = input->Buffer()->GetBackingStore();
  double* percentiles = reinterpret_cast<double*>(
      static_cast<char*>(backing->Data()) + input->ByteOffset());

  std::vector<int64_t> values(length);
  (*histogram)->PercentilesAt(percentiles, values.data(), length);

  for (size_t i = 0; i < length; i++) {
    USE(map->Set(env->context(),
                 Number::New(env->isolate(), percentiles[i]),
                 Number::New(env->isolate(), static_cast<double>(values[i]))));
  }
}

void HistogramImpl::GetLinearBuckets(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsMap());
  int64_t step_size = static_cast<int64_t>(args[0].As<Number>()->Value());
  Local<Map> map = args[1].As<Map>();

  std::vector<std::pair<int64_t, int64_t>> entries;
  (*histogram)
      ->LinearBuckets(step_size, [&entries](int64_t value, int64_t count) {
        entries.emplace_back(value, count);
      });
  for (const auto& entry : entries) {
    USE(map->Set(
        env->context(),
        Number::New(env->isolate(), static_cast<double>(entry.first)),
        Number::New(env->isolate(), static_cast<double>(entry.second))));
  }
}

void HistogramImpl::GetLogBuckets(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsNumber());
  CHECK(args[2]->IsMap());
  int64_t first_bucket = static_cast<int64_t>(args[0].As<Number>()->Value());
  double log_base = args[1].As<Number>()->Value();
  Local<Map> map = args[2].As<Map>();

  std::vector<std::pair<int64_t, int64_t>> entries;
  (*histogram)
      ->LogBuckets(
          first_bucket, log_base, [&entries](int64_t value, int64_t count) {
            entries.emplace_back(value, count);
          });
  for (const auto& entry : entries) {
    USE(map->Set(
        env->context(),
        Number::New(env->isolate(), static_cast<double>(entry.first)),
        Number::New(env->isolate(), static_cast<double>(entry.second))));
  }
}

HistogramImpl* HistogramImpl::FromJSObject(Local<Value> value) {
  auto obj = value.As<Object>();
  DCHECK_GE(obj->InternalFieldCount(), HistogramImpl::kInternalFieldCount);
  return static_cast<HistogramImpl*>(obj->GetAlignedPointerFromInternalField(
      HistogramImpl::kImplField, EmbedderDataTag::kDefault));
}

std::unique_ptr<worker::TransferData> IterationHistogram::CloneForMessaging()
    const {
  return std::make_unique<HistogramBase::HistogramTransferData>(histogram());
}

std::unique_ptr<worker::TransferData>
IntervalHistogram::CloneForMessaging() const {
  return std::make_unique<HistogramBase::HistogramTransferData>(histogram());
}

}  // namespace node
