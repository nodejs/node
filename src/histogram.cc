#include "histogram.h"  // NOLINT(build/include_inline)
#include "base_object-inl.h"
#include "histogram-inl.h"
#include "memory_tracker-inl.h"
#include "node_debug.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "threadpoolwork-inl.h"
#include "util.h"
#include "v8-typed-array.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <vector>

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::BigInt;
using v8::CFunction;
using v8::Context;
using v8::Exception;
using v8::FastApiCallbackOptions;
using v8::Float64Array;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::Name;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::String;
using v8::Uint32;
using v8::Uint8Array;
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

Histogram::Histogram(HistogramPointer histogram, const Options& options)
    : histogram_(std::move(histogram)) {
  // alpha = 1 - 2^(-1/halfLife). With halfLife <= 0, EWMA is disabled.
  if (options.half_life > 0) {
    ewma_alpha_ = 1.0 - std::exp(-std::log(2.0) / options.half_life);
  }
  threshold_ = options.threshold;
}

std::shared_ptr<Histogram> Histogram::Create(const Options& options) {
  hdr_histogram* histogram;
  if (hdr_init(options.lowest, options.highest, options.figures, &histogram) !=
      0) {
    return {};
  }
  return std::make_shared<Histogram>(HistogramPointer(histogram), options);
}

void Histogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("histogram", GetMemorySize());
  tracker->TrackFieldWithSize("qrde_snapshot",
                              GetCachedRecordedSnapshotMemorySize());
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

Histogram::RecordedSnapshot Histogram::BuildRecordedSnapshot(
    const hdr_histogram* histogram) {
  RecordedSnapshot snapshot;
  snapshot.total_count = histogram->total_count;
  if (snapshot.total_count == 0) return snapshot;

  const size_t occupied =
      std::count_if(histogram->counts,
                    histogram->counts + histogram->counts_len,
                    [](int64_t count) { return count != 0; });
  snapshot.buckets.reserve(occupied);
  hdr_iter iter;
  hdr_iter_recorded_init(&iter, histogram);
  while (hdr_iter_next(&iter)) {
    snapshot.buckets.push_back({
        static_cast<double>(iter.median_equivalent_value),
        static_cast<double>(
            hdr_size_of_equivalent_value_range(histogram, iter.value)),
        iter.count,
        iter.cumulative_count,
    });
  }
  return snapshot;
}

Histogram::RecordedSnapshotSource Histogram::GetRecordedSnapshotSource(
    bool use_cache) const {
  RecordedSnapshotSource source;
  int64_t lowest;
  int64_t highest;
  int figures;
  {
    RwLock::ScopedReadLock lock(mutex_);
    source.generation = mutation_generation_;
    if (use_cache && recorded_snapshot_cache_) {
      source.snapshot = recorded_snapshot_cache_;
      source.cache_hit = true;
      return source;
    }
    if (histogram_->total_count == 0) {
      source.snapshot = std::make_shared<const RecordedSnapshot>();
      return source;
    }
    lowest = histogram_->lowest_discernible_value;
    highest = histogram_->highest_trackable_value;
    figures = histogram_->significant_figures;
  }

  hdr_histogram* copy;
  if (hdr_init(lowest, highest, figures, &copy) != 0) return source;
  source.histogram.reset(copy);

  RwLock::ScopedReadLock lock(mutex_);
  source.generation = mutation_generation_;
  if (use_cache && recorded_snapshot_cache_) {
    source.histogram.reset();
    source.snapshot = recorded_snapshot_cache_;
    source.cache_hit = true;
    return source;
  }
  if (histogram_->total_count == 0) {
    source.histogram.reset();
    source.snapshot = std::make_shared<const RecordedSnapshot>();
    return source;
  }

  CHECK_EQ(source.histogram->counts_len, histogram_->counts_len);
  source.histogram->min_value = histogram_->min_value;
  source.histogram->max_value = histogram_->max_value;
  source.histogram->normalizing_index_offset =
      histogram_->normalizing_index_offset;
  source.histogram->conversion_ratio = histogram_->conversion_ratio;
  source.histogram->total_count = histogram_->total_count;
  std::memcpy(source.histogram->counts,
              histogram_->counts,
              histogram_->counts_len * sizeof(*histogram_->counts));
  return source;
}

void Histogram::CacheRecordedSnapshot(
    uint64_t generation, std::shared_ptr<const RecordedSnapshot> snapshot) {
  RwLock::ScopedWriteLock lock(mutex_);
  if (generation == mutation_generation_) {
    recorded_snapshot_cache_ = std::move(snapshot);
  }
}

size_t Histogram::GetCachedRecordedSnapshotMemorySize() const {
  RwLock::ScopedReadLock lock(mutex_);
  if (!recorded_snapshot_cache_) return 0;
  return recorded_snapshot_cache_->buckets.capacity() * sizeof(RecordedBucket);
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
    InvalidateRecordedSnapshot();
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

namespace {
// Continued fraction evaluation for the regularized incomplete beta
// function using Lentz's modified method. Reference: Numerical Recipes
// in C, 2nd edition, section 6.4.
static double BetaContinuedFraction(double a, double b, double x) {
  constexpr double FPMIN = 1e-30;
  constexpr int MAXIT = 200;
  constexpr double EPS = 3e-12;

  double qab = a + b;
  double qap = a + 1.0;
  double qam = a - 1.0;
  double c = 1.0;
  double d = 1.0 - qab * x / qap;
  if (std::fabs(d) < FPMIN) d = FPMIN;
  d = 1.0 / d;
  double h = d;

  for (int m = 1; m <= MAXIT; m++) {
    int m2 = 2 * m;
    // Even step.
    double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
    d = 1.0 + aa * d;
    if (std::fabs(d) < FPMIN) d = FPMIN;
    c = 1.0 + aa / c;
    if (std::fabs(c) < FPMIN) c = FPMIN;
    d = 1.0 / d;
    h *= d * c;
    // Odd step.
    aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
    d = 1.0 + aa * d;
    if (std::fabs(d) < FPMIN) d = FPMIN;
    c = 1.0 + aa / c;
    if (std::fabs(c) < FPMIN) c = FPMIN;
    d = 1.0 / d;
    double del = d * c;
    h *= del;
    if (std::fabs(del - 1.0) <= EPS) break;
  }
  return h;
}

struct BetaParameters {
  double a;
  double b;
  double log_normalization;
  double mean;
  double standard_deviation;
  double skewness;
  double excess_kurtosis;
  double log_scale;
  double symmetry_point;
  bool use_asymptotic_front;
  bool use_asymptotic_cdf;
};

static double StirlingCorrection(double x) {
  const double inverse = 1.0 / x;
  const double inverse_squared = inverse * inverse;
  return inverse * (1.0 / 12.0 +
                    inverse_squared *
                        (-1.0 / 360.0 +
                         inverse_squared *
                             (1.0 / 1260.0 +
                              inverse_squared *
                                  (-1.0 / 1680.0 + inverse_squared / 1188.0))));
}

static BetaParameters MakeBetaParameters(double a,
                                         double b,
                                         bool approximate_large_shapes) {
  const double sum = a + b;
  const double mean = a / sum;
  const bool use_asymptotic_front =
      approximate_large_shapes && a >= 8.0 && b >= 8.0;
  // The continued fraction needs progressively more iterations as both
  // beta shapes grow. At this concentration the Edgeworth error is smaller
  // than the continued-fraction truncation error.
  const bool use_asymptotic_cdf =
      approximate_large_shapes && a * b / sum >= 25'000.0;
  const double variance = mean * (1.0 - mean) / (sum + 1.0);
  const double skewness =
      2.0 * (b - a) * std::sqrt(sum + 1.0) / ((sum + 2.0) * std::sqrt(a * b));
  const double excess_kurtosis =
      6.0 * ((a - b) * (a - b) * (sum + 1.0) - a * b * (sum + 2.0)) /
      (a * b * (sum + 2.0) * (sum + 3.0));
  return {
      a,
      b,
      use_asymptotic_front ? 0.0
                           : std::lgamma(sum) - std::lgamma(a) - std::lgamma(b),
      mean,
      std::sqrt(variance),
      skewness,
      excess_kurtosis,
      use_asymptotic_front ? 0.5 * std::log(sum * mean * (1.0 - mean) /
                                            (2.0 * std::numbers::pi)) +
                                 StirlingCorrection(sum) -
                                 StirlingCorrection(a) - StirlingCorrection(b)
                           : 0.0,
      (a + 1.0) / (sum + 2.0),
      use_asymptotic_front,
      use_asymptotic_cdf,
  };
}

static BetaParameters ReflectBetaParameters(const BetaParameters& params) {
  return {
      params.b,
      params.a,
      params.log_normalization,
      1.0 - params.mean,
      params.standard_deviation,
      -params.skewness,
      params.excess_kurtosis,
      params.log_scale,
      1.0 - params.symmetry_point,
      params.use_asymptotic_front,
      params.use_asymptotic_cdf,
  };
}

static double BetaFront(const BetaParameters& params, double x) {
  if (x <= 0.0 || x >= 1.0) return 0.0;
  double log_front;
  if (params.use_asymptotic_front) {
    log_front = params.a * std::log1p((x - params.mean) / params.mean) +
                params.b * std::log1p((params.mean - x) / (1.0 - params.mean)) +
                params.log_scale;
  } else {
    log_front = params.log_normalization + params.a * std::log(x) +
                params.b * std::log(1.0 - x);
  }
  return std::exp(log_front);
}

static double AsymptoticBetaCdf(const BetaParameters& params,
                                double x,
                                double* front_out) {
  const double z = (x - params.mean) / params.standard_deviation;
  const double normal_cdf = 0.5 * std::erfc(-z / std::numbers::sqrt2);
  if (std::fabs(z) >= 8.0) {
    if (front_out != nullptr) *front_out = 0.0;
    return normal_cdf;
  }

  const double z2 = z * z;
  const double z3 = z2 * z;
  const double normal_pdf =
      std::exp(-0.5 * z2) / std::sqrt(2.0 * std::numbers::pi);
  if (front_out != nullptr) {
    const double z4 = z2 * z2;
    const double z6 = z3 * z3;
    const double density_correction =
        1.0 + params.skewness / 6.0 * (z3 - 3.0 * z) +
        params.excess_kurtosis / 24.0 * (z4 - 6.0 * z2 + 3.0) +
        params.skewness * params.skewness / 72.0 *
            (z6 - 15.0 * z4 + 45.0 * z2 - 15.0);
    *front_out = x * (1.0 - x) * normal_pdf / params.standard_deviation *
                 std::max(0.0, density_correction);
  }
  const double correction = params.skewness / 6.0 * (1.0 - z2) -
                            params.excess_kurtosis / 24.0 * (z3 - 3.0 * z) -
                            params.skewness * params.skewness / 72.0 *
                                (z3 * z2 - 10.0 * z3 + 15.0 * z);
  return std::clamp(normal_cdf + normal_pdf * correction, 0.0, 1.0);
}

// Regularized incomplete beta function I_x(a, b).
// Returns the probability that a Beta(a,b) random variable is <= x.
static double RegularizedIncompleteBeta(const BetaParameters& params,
                                        double x,
                                        double* front_out = nullptr) {
  if (front_out != nullptr) *front_out = 0.0;
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  if (params.use_asymptotic_cdf) {
    return AsymptoticBetaCdf(params, x, front_out);
  }
  const double front = BetaFront(params, x);
  if (front_out != nullptr) *front_out = front;

  // Use the symmetry relation to ensure the continued fraction
  // converges in the region where it is most accurate.
  if (x < params.symmetry_point) {
    return front * BetaContinuedFraction(params.a, params.b, x) / params.a;
  }
  return 1.0 -
         front * BetaContinuedFraction(params.b, params.a, 1.0 - x) / params.b;
}

static double RegularizedIncompleteBeta(double a, double b, double x) {
  return RegularizedIncompleteBeta(MakeBetaParameters(a, b, false), x);
}

// Standard normal CDF: Phi(x) = P(Z <= x).
static double NormalCdf(double x) {
  return 0.5 * std::erfc(-x * std::numbers::sqrt2 / 2.0);
}

// Student's t-distribution CDF: P(T <= t) for df degrees of freedom.
static double StudentTCdf(double t, double df) {
  double squared = t * t;
  double denominator = df + squared;
  if (squared < df) {
    double x = squared / denominator;
    double ibeta = RegularizedIncompleteBeta(0.5, df / 2.0, x);
    return t >= 0.0 ? 0.5 + 0.5 * ibeta : 0.5 - 0.5 * ibeta;
  }

  double x = df / denominator;
  double ibeta = RegularizedIncompleteBeta(df / 2.0, 0.5, x);
  return t >= 0.0 ? 1.0 - 0.5 * ibeta : 0.5 * ibeta;
}

// Positive Student's t-distribution quantile for an upper-tail probability.
// Using the lower tail avoids loss of precision for probabilities near 1.
static double StudentTUpperQuantile(double p, double df) {
  double lo = 0.0;
  double hi = 1.0;
  while (StudentTCdf(-hi, df) > p) hi *= 2.0;

  for (int i = 0; i < 100; i++) {
    double mid = (lo + hi) / 2.0;
    if (StudentTCdf(-mid, df) > p) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return (lo + hi) / 2.0;
}

// Binomial CDF: P(X <= k) for X ~ Binomial(n, p).
// Uses the identity P(X <= k) = I_{1-p}(n-k, k+1).
static double BinomialCdf(int64_t k, int64_t n, double p) {
  if (k < 0) return 0.0;
  if (k >= n) return 1.0;
  return RegularizedIncompleteBeta(
      static_cast<double>(n - k), static_cast<double>(k + 1), 1.0 - p);
}

struct QrdeResult {
  std::vector<double> quantiles;
  std::vector<double> densities;
  size_t corrections = 0;
};

enum class QrdeDequantization : uint32_t {
  kNone,
  kHdr,
  kAll,
};

static bool QrdeShouldDequantize(const Histogram::RecordedBucket& bucket,
                                 QrdeDequantization dequantization) {
  return bucket.count > 1 && (dequantization == QrdeDequantization::kAll ||
                              (dequantization == QrdeDequantization::kHdr &&
                               bucket.resolution > 1.0));
}

static std::pair<double, double> QrdeBucketRange(
    const Histogram::RecordedSnapshot& snapshot,
    size_t index,
    QrdeDequantization dequantization) {
  const auto& bucket = snapshot.buckets[index];
  if (!QrdeShouldDequantize(bucket, dequantization)) {
    return {bucket.value, bucket.value};
  }

  const double half_resolution = bucket.resolution / 2.0;
  if (snapshot.buckets.size() == 1) {
    return {bucket.value - half_resolution, bucket.value + half_resolution};
  }
  if (index == 0) {
    return {bucket.value, bucket.value + half_resolution};
  }
  if (index + 1 == snapshot.buckets.size()) {
    return {bucket.value - half_resolution, bucket.value};
  }
  return {bucket.value - half_resolution, bucket.value + half_resolution};
}

static bool QrdeExactSupport(const Histogram::RecordedSnapshot& snapshot,
                             const BetaParameters& params,
                             const BetaParameters& reflected,
                             double count,
                             size_t* begin_index,
                             size_t* end_index) {
  // Collapsing both tails changes a quantile by at most 2^-79 times the
  // histogram range, which is less than 2^-16 over the int64 value domain.
  constexpr double tail_mass = 0x1p-80;
  size_t lo = 0;
  size_t hi = snapshot.buckets.size();
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const double rank =
        static_cast<double>(snapshot.buckets[mid].cumulative_count) / count;
    const double cdf = RegularizedIncompleteBeta(params, rank);
    if (!std::isfinite(cdf)) return false;
    if (cdf < tail_mass) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo == snapshot.buckets.size()) return false;
  const size_t first = lo;

  lo = first;
  hi = snapshot.buckets.size();
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const double remaining_rank =
        static_cast<double>(snapshot.total_count -
                            snapshot.buckets[mid].cumulative_count) /
        count;
    const double survival =
        RegularizedIncompleteBeta(reflected, remaining_rank);
    if (!std::isfinite(survival)) return false;
    if (survival > tail_mass) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo == snapshot.buckets.size() || lo < first) return false;
  *begin_index = first;
  *end_index = lo + 1;
  return true;
}

static double QrdeQuantile(const Histogram::RecordedSnapshot& snapshot,
                           double p,
                           QrdeDequantization dequantization) {
  const double count = static_cast<double>(snapshot.total_count);
  const double a = (count + 1.0) * p;
  const double b = (count + 1.0) * (1.0 - p);
  const BetaParameters mass_params = MakeBetaParameters(a, b, true);
  const BetaParameters reflected_params = ReflectBetaParameters(mass_params);

  size_t begin_index = 0;
  size_t end_index = snapshot.buckets.size();
  if (mass_params.use_asymptotic_cdf) {
    // Outside this interval the normal-tail mass is below 2e-33.
    constexpr double support_deviations = 12.0;
    const double lower_rank = std::max(
        0.0,
        mass_params.mean - support_deviations * mass_params.standard_deviation);
    const double upper_rank = std::min(
        1.0,
        mass_params.mean + support_deviations * mass_params.standard_deviation);
    const auto first = std::lower_bound(
        snapshot.buckets.begin(),
        snapshot.buckets.end(),
        lower_rank * count,
        [](const Histogram::RecordedBucket& bucket, double cumulative) {
          return static_cast<double>(bucket.cumulative_count) < cumulative;
        });
    const auto last = std::lower_bound(
        first,
        snapshot.buckets.end(),
        upper_rank * count,
        [](const Histogram::RecordedBucket& bucket, double cumulative) {
          return static_cast<double>(bucket.cumulative_count) < cumulative;
        });
    begin_index = first - snapshot.buckets.begin();
    if (last != snapshot.buckets.end()) {
      end_index = last - snapshot.buckets.begin() + 1;
    }
  } else if (snapshot.buckets.size() >= 512) {
    QrdeExactSupport(snapshot,
                     mass_params,
                     reflected_params,
                     count,
                     &begin_index,
                     &end_index);
  }

  const bool dequantize = dequantization != QrdeDequantization::kNone;
  const auto first_range = QrdeBucketRange(snapshot, 0, dequantization);
  const auto last_range =
      QrdeBucketRange(snapshot, snapshot.buckets.size() - 1, dequantization);
  const double lower_endpoint = first_range.first;
  const double upper_endpoint = last_range.second;

  double previous_cdf = 0.0;
  double previous_survival = 0.0;
  double previous_front = 0.0;
  int64_t previous_count =
      begin_index == 0 ? 0 : snapshot.buckets[begin_index - 1].cumulative_count;
  if (previous_count != 0) {
    const double previous_rank = static_cast<double>(previous_count) / count;
    previous_cdf = RegularizedIncompleteBeta(
        mass_params, previous_rank, dequantize ? &previous_front : nullptr);
    if (!std::isfinite(previous_cdf) ||
        (dequantize && !std::isfinite(previous_front))) {
      begin_index = 0;
      end_index = snapshot.buckets.size();
      previous_count = 0;
      previous_cdf = 0.0;
      previous_front = 0.0;
    } else {
      previous_cdf = std::clamp(previous_cdf, 0.0, 1.0);
    }
  }

  double quantile = lower_endpoint * previous_cdf;
  bool using_survival = false;

  for (size_t i = begin_index; i < end_index; i++) {
    const auto& bucket = snapshot.buckets[i];
    const double u0 = static_cast<double>(previous_count) / count;
    const double u1 = static_cast<double>(bucket.cumulative_count) / count;
    double front = 0.0;
    double mass;
    // Use the reflected CDF above the symmetry point so small upper-tail
    // interval weights are not lost to subtraction from one.
    if (u1 <= mass_params.symmetry_point) {
      const double cdf =
          std::clamp(RegularizedIncompleteBeta(
                         mass_params, u1, dequantize ? &front : nullptr),
                     previous_cdf,
                     1.0);
      mass = cdf - previous_cdf;
      previous_cdf = cdf;
    } else {
      const double remaining_rank =
          static_cast<double>(snapshot.total_count - bucket.cumulative_count) /
          count;
      double survival = RegularizedIncompleteBeta(
          reflected_params, remaining_rank, dequantize ? &front : nullptr);
      if (using_survival) {
        survival = std::clamp(survival, 0.0, previous_survival);
        mass = previous_survival - survival;
      } else {
        survival = std::clamp(survival, 0.0, 1.0 - previous_cdf);
        mass = 1.0 - previous_cdf - survival;
        using_survival = true;
      }
      previous_survival = survival;
    }

    if (!QrdeShouldDequantize(bucket, dequantization)) {
      quantile += bucket.value * mass;
    } else {
      // I_x(a + 1, b) = I_x(a, b) - front / a, so the first moment
      // within this rank interval does not require a second beta CDF.
      const double sum = a + b;
      const double rank_width = static_cast<double>(bucket.count) / count;
      const double local_moment =
          std::clamp((a / sum - u0) * mass - (front - previous_front) / sum,
                     0.0,
                     rank_width * mass);
      const auto [lower, upper] = QrdeBucketRange(snapshot, i, dequantization);
      quantile += lower * mass + (upper - lower) * local_moment / rank_width;
    }

    previous_front = front;
    previous_count = bucket.cumulative_count;
  }

  const double remaining_mass =
      using_survival ? previous_survival : 1.0 - previous_cdf;
  quantile += upper_endpoint * std::clamp(remaining_mass, 0.0, 1.0);
  return quantile;
}

static QrdeResult CalculateQrde(const Histogram::RecordedSnapshot& snapshot,
                                const std::vector<double>& probabilities,
                                QrdeDequantization dequantization) {
  QrdeResult result;
  if (snapshot.buckets.empty()) return result;

  result.quantiles.resize(probabilities.size());
  result.densities.resize(probabilities.size() - 1);

  const auto first_range = QrdeBucketRange(snapshot, 0, dequantization);
  const auto last_range =
      QrdeBucketRange(snapshot, snapshot.buckets.size() - 1, dequantization);
  result.quantiles.front() = first_range.first;
  result.quantiles.back() = last_range.second;

  for (size_t i = 1; i + 1 < probabilities.size(); i++) {
    double value = QrdeQuantile(snapshot, probabilities[i], dequantization);
    value =
        std::clamp(value, result.quantiles.front(), result.quantiles.back());
    if (value < result.quantiles[i - 1]) {
      value = result.quantiles[i - 1];
      result.corrections++;
    }
    result.quantiles[i] = value;
  }

  for (size_t i = 0; i < result.densities.size(); i++) {
    const double width = result.quantiles[i + 1] - result.quantiles[i];
    const double probability_mass = probabilities[i + 1] - probabilities[i];
    result.densities[i] = width == 0.0 ? std::numeric_limits<double>::infinity()
                                       : probability_mass / width;
  }

  return result;
}

static Local<Float64Array> ToFloat64Array(Isolate* isolate,
                                          const std::vector<double>& values) {
  auto store =
      ArrayBuffer::NewBackingStore(isolate, values.size() * sizeof(double));
  if (!values.empty()) {
    memcpy(store->Data(), values.data(), values.size() * sizeof(double));
  }
  Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, std::move(store));
  return Float64Array::New(buffer, 0, values.size());
}

static const char* QrdeDequantizationName(QrdeDequantization dequantization) {
  switch (dequantization) {
    case QrdeDequantization::kNone:
      return "none";
    case QrdeDequantization::kHdr:
      return "hdr";
    case QrdeDequantization::kAll:
      return "all";
  }
  UNREACHABLE();
}

class QrdeJob final : public ThreadPoolWork {
 public:
  QrdeJob(Environment* env,
          Local<Promise::Resolver> resolver,
          std::shared_ptr<Histogram> histogram,
          Histogram::RecordedSnapshotSource snapshot_source,
          std::vector<double> probabilities,
          QrdeDequantization dequantization)
      : ThreadPoolWork(env, "histogram.qrde"),
        histogram_(std::move(histogram)),
        snapshot_source_(std::move(snapshot_source)),
        probabilities_(std::move(probabilities)),
        dequantization_(dequantization) {
    resolver_.Reset(env->isolate(), resolver);
  }

  void DoThreadPoolWork() override {
    if (snapshot_source_.snapshot) {
      snapshot_ = std::move(snapshot_source_.snapshot);
    } else {
      snapshot_ = std::make_shared<const Histogram::RecordedSnapshot>(
          Histogram::BuildRecordedSnapshot(snapshot_source_.histogram.get()));
      snapshot_source_.histogram.reset();
    }
    if (histogram_) {
      histogram_->CacheRecordedSnapshot(snapshot_source_.generation, snapshot_);
      histogram_.reset();
    }
    result_ = CalculateQrde(*snapshot_, probabilities_, dequantization_);
  }

  void AfterThreadPoolWork(int status) override {
    std::unique_ptr<QrdeJob> self(this);
    Environment* env = ThreadPoolWork::env();
    CHECK(status == 0 || status == UV_ECANCELED);
    if (!env->can_call_into_js()) {
      resolver_.Reset();
      return;
    }

    Isolate* isolate = env->isolate();
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(env->context());
    InternalCallbackScope callback_scope(
        env, Object::New(isolate), {0, 0}, InternalCallbackScope::kNoFlags);
    Local<Promise::Resolver> resolver =
        Local<Promise::Resolver>::New(isolate, resolver_);

    if (status == UV_ECANCELED) {
      USE(resolver->Reject(
          env->context(),
          Exception::Error(OneByteString(isolate, "QRDE job was canceled"))));
      resolver_.Reset();
      return;
    }

    Local<Name> names[] = {
        FIXED_ONE_BYTE_STRING(isolate, "probabilities"),
        FIXED_ONE_BYTE_STRING(isolate, "quantiles"),
        FIXED_ONE_BYTE_STRING(isolate, "densities"),
        FIXED_ONE_BYTE_STRING(isolate, "count"),
        FIXED_ONE_BYTE_STRING(isolate, "bucketCount"),
        FIXED_ONE_BYTE_STRING(isolate, "corrections"),
        FIXED_ONE_BYTE_STRING(isolate, "dequantize"),
    };
    Local<Value> values[] = {
        ToFloat64Array(isolate, probabilities_),
        ToFloat64Array(isolate, result_.quantiles),
        ToFloat64Array(isolate, result_.densities),
        BigInt::New(isolate, snapshot_->total_count),
        Number::New(isolate, static_cast<double>(snapshot_->buckets.size())),
        Number::New(isolate, static_cast<double>(result_.corrections)),
        OneByteString(isolate, QrdeDequantizationName(dequantization_)),
    };
    Local<Object> value =
        Object::New(isolate, Null(isolate), names, values, arraysize(names));
    USE(resolver->Resolve(env->context(), value));
    resolver_.Reset();
  }

 private:
  std::shared_ptr<Histogram> histogram_;
  Histogram::RecordedSnapshotSource snapshot_source_;
  std::shared_ptr<const Histogram::RecordedSnapshot> snapshot_;
  const std::vector<double> probabilities_;
  const QrdeDequantization dequantization_;
  QrdeResult result_;
  Global<Promise::Resolver> resolver_;
};

// -----------------------------------------------------------------------
// Minimal CBOR encoder/decoder (RFC 8949) -- just enough types for
// histogram export/import: unsigned int, float64, array, and map.
// -----------------------------------------------------------------------

// CBOR major types (upper 3 bits of the initial byte).
constexpr uint8_t kCborUint = 0 << 5;   // Major 0: unsigned integer
constexpr uint8_t kCborArray = 4 << 5;  // Major 4: array
constexpr uint8_t kCborMap = 5 << 5;    // Major 5: map
constexpr uint8_t kCborFloat64 = 0xfb;  // Major 7, additional 27

static void CborWriteUint(std::vector<uint8_t>& out,
                          uint8_t major,
                          uint64_t val) {
  if (val <= 23) {
    out.push_back(major | static_cast<uint8_t>(val));
  } else if (val <= 0xff) {
    out.push_back(major | 24);
    out.push_back(static_cast<uint8_t>(val));
  } else if (val <= 0xffff) {
    out.push_back(major | 25);
    out.push_back(static_cast<uint8_t>(val >> 8));
    out.push_back(static_cast<uint8_t>(val));
  } else if (val <= 0xffffffff) {
    out.push_back(major | 26);
    out.push_back(static_cast<uint8_t>(val >> 24));
    out.push_back(static_cast<uint8_t>(val >> 16));
    out.push_back(static_cast<uint8_t>(val >> 8));
    out.push_back(static_cast<uint8_t>(val));
  } else {
    out.push_back(major | 27);
    out.push_back(static_cast<uint8_t>(val >> 56));
    out.push_back(static_cast<uint8_t>(val >> 48));
    out.push_back(static_cast<uint8_t>(val >> 40));
    out.push_back(static_cast<uint8_t>(val >> 32));
    out.push_back(static_cast<uint8_t>(val >> 24));
    out.push_back(static_cast<uint8_t>(val >> 16));
    out.push_back(static_cast<uint8_t>(val >> 8));
    out.push_back(static_cast<uint8_t>(val));
  }
}

static void CborWriteFloat64(std::vector<uint8_t>& out, double val) {
  out.push_back(kCborFloat64);
  uint64_t bits;
  memcpy(&bits, &val, sizeof(bits));
  // Network byte order (big-endian).
  out.push_back(static_cast<uint8_t>(bits >> 56));
  out.push_back(static_cast<uint8_t>(bits >> 48));
  out.push_back(static_cast<uint8_t>(bits >> 40));
  out.push_back(static_cast<uint8_t>(bits >> 32));
  out.push_back(static_cast<uint8_t>(bits >> 24));
  out.push_back(static_cast<uint8_t>(bits >> 16));
  out.push_back(static_cast<uint8_t>(bits >> 8));
  out.push_back(static_cast<uint8_t>(bits));
}

static bool CborReadUint(const uint8_t*& p, const uint8_t* end, uint64_t* val) {
  if (p >= end) return false;
  uint8_t info = *p++ & 0x1f;
  if (info <= 23) {
    *val = info;
  } else if (info == 24) {
    if (p + 1 > end) return false;
    *val = p[0];
    p += 1;
  } else if (info == 25) {
    if (p + 2 > end) return false;
    *val = (static_cast<uint64_t>(p[0]) << 8) | p[1];
    p += 2;
  } else if (info == 26) {
    if (p + 4 > end) return false;
    *val = (static_cast<uint64_t>(p[0]) << 24) |
           (static_cast<uint64_t>(p[1]) << 16) |
           (static_cast<uint64_t>(p[2]) << 8) | p[3];
    p += 4;
  } else if (info == 27) {
    if (p + 8 > end) return false;
    *val = (static_cast<uint64_t>(p[0]) << 56) |
           (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) |
           (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) |
           (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) << 8) | p[7];
    p += 8;
  } else {
    return false;  // Indefinite length or reserved -- not supported.
  }
  return true;
}

static bool CborReadFloat64(const uint8_t*& p,
                            const uint8_t* end,
                            double* val) {
  if (p >= end || *p != kCborFloat64) return false;
  p++;
  if (p + 8 > end) return false;
  uint64_t bits = (static_cast<uint64_t>(p[0]) << 56) |
                  (static_cast<uint64_t>(p[1]) << 48) |
                  (static_cast<uint64_t>(p[2]) << 40) |
                  (static_cast<uint64_t>(p[3]) << 32) |
                  (static_cast<uint64_t>(p[4]) << 24) |
                  (static_cast<uint64_t>(p[5]) << 16) |
                  (static_cast<uint64_t>(p[6]) << 8) | p[7];
  p += 8;
  memcpy(val, &bits, sizeof(*val));
  return true;
}

// Read a value that may be either a uint or float64.
static bool CborReadNumber(const uint8_t*& p, const uint8_t* end, double* val) {
  if (p >= end) return false;
  if (*p == kCborFloat64) return CborReadFloat64(p, end, val);
  uint64_t u;
  if (!CborReadUint(p, end, &u)) return false;
  *val = static_cast<double>(u);
  return true;
}

// Histogram export format version.
constexpr uint64_t kExportVersion = 1;

// Integer keys for the top-level CBOR map.
constexpr uint64_t kKeyVersion = 0;
constexpr uint64_t kKeyLowest = 1;
constexpr uint64_t kKeyHighest = 2;
constexpr uint64_t kKeyFigures = 3;
constexpr uint64_t kKeyTotalCount = 4;
constexpr uint64_t kKeyMin = 5;
constexpr uint64_t kKeyMax = 6;
constexpr uint64_t kKeyNormOffset = 7;
constexpr uint64_t kKeyConvRatio = 8;
constexpr uint64_t kKeyCountsLen = 9;
constexpr uint64_t kKeyCounts = 10;
constexpr uint64_t kKeyEwma = 11;

// Integer keys for the EWMA sub-map.
constexpr uint64_t kEwmaAlpha = 0;
constexpr uint64_t kEwmaMean = 1;
constexpr uint64_t kEwmaVariance = 2;
constexpr uint64_t kEwmaErrorRate = 3;
constexpr uint64_t kEwmaThreshold = 4;
}  // namespace

Histogram::MeanCIResult Histogram::MeanCI(double confidence) const {
  RwLock::ScopedReadLock lock(mutex_);
  int64_t count = histogram_->total_count;
  double mean = hdr_mean(histogram_.get());
  if (count < 2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    return {mean, nan, nan};
  }

  double stddev = hdr_stddev(histogram_.get());
  if (stddev == 0.0) return {mean, mean, mean};

  // HdrHistogram computes population stddev (divides by N). The confidence
  // interval requires sample variance (divides by N-1).
  double variance = stddev * stddev * static_cast<double>(count) /
                    static_cast<double>(count - 1);
  double standard_error = std::sqrt(variance / static_cast<double>(count));
  double alpha = 1.0 - confidence;
  double t_crit =
      StudentTUpperQuantile(alpha / 2.0, static_cast<double>(count - 1));
  double margin = t_crit * standard_error;
  return {mean, mean - margin, mean + margin};
}

Histogram::WelchTestResult Histogram::WelchTest(const Histogram& other,
                                                double confidence) const {
  auto do_welch = [&]() -> WelchTestResult {
    int64_t n1 = histogram_->total_count;
    int64_t n2 = other.histogram_->total_count;
    if (n1 < 2 || n2 < 2) return {0, 0, 1, 0, 0};

    double mean1 = hdr_mean(histogram_.get());
    double mean2 = hdr_mean(other.histogram_.get());
    double sd1 = hdr_stddev(histogram_.get());
    double sd2 = hdr_stddev(other.histogram_.get());

    // HdrHistogram computes population stddev (divides by N).
    // Welch's t-test requires sample variance (divides by N-1).
    double var1 =
        sd1 * sd1 * static_cast<double>(n1) / static_cast<double>(n1 - 1);
    double var2 =
        sd2 * sd2 * static_cast<double>(n2) / static_cast<double>(n2 - 1);

    double se1 = var1 / static_cast<double>(n1);
    double se2 = var2 / static_cast<double>(n2);
    double se_sum = se1 + se2;
    if (se_sum == 0.0) return {0, 0, 1, 0, 0};

    double t = (mean1 - mean2) / std::sqrt(se_sum);

    // Welch-Satterthwaite degrees of freedom.
    double df = (se_sum * se_sum) / (se1 * se1 / static_cast<double>(n1 - 1) +
                                     se2 * se2 / static_cast<double>(n2 - 1));

    // Two-tailed p-value.
    double p = 2.0 * StudentTCdf(-std::fabs(t), df);

    // Confidence interval on the difference of means.
    double alpha = 1.0 - confidence;
    double t_crit = StudentTUpperQuantile(alpha / 2.0, df);
    double margin = t_crit * std::sqrt(se_sum);
    double diff = mean1 - mean2;

    return {t, df, p, diff - margin, diff + margin};
  };

  if (this == &other) return {0, 0, 1, 0, 0};

  if (this < &other) {
    RwLock::ScopedReadLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_welch();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedReadLock lock2(mutex_);
  return do_welch();
}

Histogram::MannWhitneyResult Histogram::MannWhitneyTest(
    const Histogram& other) const {
  auto do_mw = [&]() -> MannWhitneyResult {
    int64_t n1 = histogram_->total_count;
    int64_t n2 = other.histogram_->total_count;
    if (n1 == 0 || n2 == 0) return {0, 0, 1};

    // Walk the counts arrays to compute the U statistic.
    // At each bucket index, values from histogram 1 at index i "beat"
    // all values from histogram 2 at indices < i (concordant pairs).
    int32_t len =
        std::max(histogram_->counts_len, other.histogram_->counts_len);

    // Forward pass: count concordant pairs (h1 values > h2 values).
    int64_t cum2 = 0;
    double concordant = 0.0;
    double tied = 0.0;
    for (int32_t i = 0; i < len; i++) {
      int64_t c1 = (i < histogram_->counts_len) ? histogram_->counts[i] : 0;
      int64_t c2 =
          (i < other.histogram_->counts_len) ? other.histogram_->counts[i] : 0;
      concordant += static_cast<double>(c1) * static_cast<double>(cum2);
      tied += static_cast<double>(c1) * static_cast<double>(c2);
      cum2 += c2;
    }

    // U statistic for sample 1: concordant + half of ties.
    double u = concordant + 0.5 * tied;
    double dn1 = static_cast<double>(n1);
    double dn2 = static_cast<double>(n2);
    double mu = dn1 * dn2 / 2.0;

    // Tie correction for the variance.
    // sigma^2 = n1*n2/12 * (N+1 - sum(t_k^3 - t_k) / (N*(N-1)))
    // where t_k is the number of observations tied at rank k.
    double n_total = dn1 + dn2;
    double tie_correction = 0.0;
    for (int32_t i = 0; i < len; i++) {
      int64_t c1 = (i < histogram_->counts_len) ? histogram_->counts[i] : 0;
      int64_t c2 =
          (i < other.histogram_->counts_len) ? other.histogram_->counts[i] : 0;
      double tk = static_cast<double>(c1 + c2);
      if (tk > 1) {
        tie_correction += tk * tk * tk - tk;
      }
    }

    double sigma_sq =
        (dn1 * dn2 / 12.0) *
        (n_total + 1.0 - tie_correction / (n_total * (n_total - 1.0)));
    if (sigma_sq <= 0.0) return {u, 0, 1};

    // Continuity-corrected z-score.
    double z = (u - mu) / std::sqrt(sigma_sq);
    // Two-tailed p-value using normal approximation.
    double p = 2.0 * NormalCdf(-std::fabs(z));

    return {u, z, p};
  };

  if (this == &other) return {0, 0, 1};

  if (this < &other) {
    RwLock::ScopedReadLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_mw();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedReadLock lock2(mutex_);
  return do_mw();
}

double Histogram::CohensD(const Histogram& other) const {
  auto do_cohens = [&]() -> double {
    int64_t n1 = histogram_->total_count;
    int64_t n2 = other.histogram_->total_count;
    if (n1 < 2 || n2 < 2) return 0.0;

    double mean1 = hdr_mean(histogram_.get());
    double mean2 = hdr_mean(other.histogram_.get());
    double sd1 = hdr_stddev(histogram_.get());
    double sd2 = hdr_stddev(other.histogram_.get());

    // Convert population variance to sample variance (Bessel's correction).
    double var1 =
        sd1 * sd1 * static_cast<double>(n1) / static_cast<double>(n1 - 1);
    double var2 =
        sd2 * sd2 * static_cast<double>(n2) / static_cast<double>(n2 - 1);

    double pooled_sd = std::sqrt((static_cast<double>(n1 - 1) * var1 +
                                  static_cast<double>(n2 - 1) * var2) /
                                 static_cast<double>(n1 + n2 - 2));
    if (pooled_sd == 0.0) return 0.0;

    return (mean1 - mean2) / pooled_sd;
  };

  if (this == &other) return 0.0;

  if (this < &other) {
    RwLock::ScopedReadLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_cohens();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedReadLock lock2(mutex_);
  return do_cohens();
}

double Histogram::CliffsD(const Histogram& other) const {
  auto do_cliffs = [&]() -> double {
    int64_t n1 = histogram_->total_count;
    int64_t n2 = other.histogram_->total_count;
    if (n1 == 0 || n2 == 0) return 0.0;

    int32_t len =
        std::max(histogram_->counts_len, other.histogram_->counts_len);

    // Forward pass: count pairs where h1 value > h2 value (concordant).
    int64_t cum2 = 0;
    double concordant = 0.0;
    double tied = 0.0;
    for (int32_t i = 0; i < len; i++) {
      int64_t c1 = (i < histogram_->counts_len) ? histogram_->counts[i] : 0;
      int64_t c2 =
          (i < other.histogram_->counts_len) ? other.histogram_->counts[i] : 0;
      concordant += static_cast<double>(c1) * static_cast<double>(cum2);
      tied += static_cast<double>(c1) * static_cast<double>(c2);
      cum2 += c2;
    }

    double discordant =
        static_cast<double>(n1) * static_cast<double>(n2) - concordant - tied;

    return (concordant - discordant) /
           (static_cast<double>(n1) * static_cast<double>(n2));
  };

  if (this == &other) return 0.0;

  if (this < &other) {
    RwLock::ScopedReadLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_cliffs();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedReadLock lock2(mutex_);
  return do_cliffs();
}

Histogram::PercentileCIResult Histogram::PercentileCI(double percentile,
                                                      double confidence) const {
  RwLock::ScopedReadLock lock(mutex_);

  int64_t value = hdr_value_at_percentile(histogram_.get(), percentile);
  int64_t n = histogram_->total_count;

  if (n < 2) {
    return {value, value, value};
  }

  double p = percentile / 100.0;
  double alpha = 1.0 - confidence;

  // Lower rank: largest j such that BinomialCdf(j-1, n, p) <= alpha/2.
  // Binary search over [0, n].
  int64_t lo = 0;
  int64_t hi = n;
  while (lo < hi) {
    int64_t mid = lo + (hi - lo + 1) / 2;
    if (BinomialCdf(mid - 1, n, p) <= alpha / 2.0) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }
  double lower_pct = static_cast<double>(lo) / static_cast<double>(n) * 100.0;

  // Upper rank: smallest k such that BinomialCdf(k-1, n, p) >= 1 - alpha/2.
  lo = 0;
  hi = n;
  while (lo < hi) {
    int64_t mid = lo + (hi - lo) / 2;
    if (BinomialCdf(mid - 1, n, p) >= 1.0 - alpha / 2.0) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  double upper_pct = static_cast<double>(lo) / static_cast<double>(n) * 100.0;

  int64_t lower_val = hdr_value_at_percentile(histogram_.get(), lower_pct);
  int64_t upper_val = hdr_value_at_percentile(histogram_.get(), upper_pct);

  return {value, lower_val, upper_val};
}

// Serialize the histogram to a CBOR (RFC 8949) byte sequence. There is no
// standard for serializing histograms, so this format is specific to this
// implementation. But, it has been designed to be compact, extensible, and
// portable across languages and platforms. Should be easily parseable in
// other languages with or without a CBOR library.
//
// The delta-encoded sparse counts is a space-saving optimization in the
// common case.
//
// Layout: a CBOR map with integer keys:
//   0  -> uint    format version (currently 1)
//   1  -> uint    lowest discernible value
//   2  -> uint    highest trackable value
//   3  -> uint    significant figures
//   4  -> uint    total count
//   5  -> uint    min value
//   6  -> uint    max value
//   7  -> uint    normalizing index offset
//   8  -> float64 conversion ratio
//   9  -> uint    counts array length
//   10 -> array   delta-encoded sparse counts as flat [delta, count, ...]
//                 (first delta is the absolute index)
//   11 -> map     EWMA state (omitted when alpha = 0):
//        0 -> float64 alpha
//        1 -> float64 mean
//        2 -> float64 variance
//        3 -> float64 error rate
//        4 -> uint    threshold
std::vector<uint8_t> Histogram::Export() const {
  RwLock::ScopedReadLock lock(mutex_);

  // Count non-zero buckets for the sparse encoding.
  int32_t non_zero = 0;
  for (int32_t i = 0; i < histogram_->counts_len; i++) {
    if (histogram_->counts[i] != 0) non_zero++;
  }

  bool has_ewma = ewma_alpha_ > 0;
  uint64_t map_size = has_ewma ? 12 : 11;

  std::vector<uint8_t> out;
  out.reserve(64 + non_zero * 10);

  // Top-level map.
  CborWriteUint(out, kCborMap, map_size);

  // 0: version
  CborWriteUint(out, kCborUint, kKeyVersion);
  CborWriteUint(out, kCborUint, kExportVersion);
  // 1: lowest
  CborWriteUint(out, kCborUint, kKeyLowest);
  CborWriteUint(out,
                kCborUint,
                static_cast<uint64_t>(histogram_->lowest_discernible_value));
  // 2: highest
  CborWriteUint(out, kCborUint, kKeyHighest);
  CborWriteUint(out,
                kCborUint,
                static_cast<uint64_t>(histogram_->highest_trackable_value));
  // 3: figures
  CborWriteUint(out, kCborUint, kKeyFigures);
  CborWriteUint(
      out, kCborUint, static_cast<uint64_t>(histogram_->significant_figures));
  // 4: total_count
  CborWriteUint(out, kCborUint, kKeyTotalCount);
  CborWriteUint(out, kCborUint, static_cast<uint64_t>(histogram_->total_count));
  // 5: min
  CborWriteUint(out, kCborUint, kKeyMin);
  CborWriteUint(out, kCborUint, static_cast<uint64_t>(histogram_->min_value));
  // 6: max
  CborWriteUint(out, kCborUint, kKeyMax);
  CborWriteUint(out, kCborUint, static_cast<uint64_t>(histogram_->max_value));
  // 7: normalizing_index_offset
  CborWriteUint(out, kCborUint, kKeyNormOffset);
  CborWriteUint(out,
                kCborUint,
                static_cast<uint64_t>(histogram_->normalizing_index_offset));
  // 8: conversion_ratio
  CborWriteUint(out, kCborUint, kKeyConvRatio);
  CborWriteFloat64(out, histogram_->conversion_ratio);
  // 9: counts_len
  CborWriteUint(out, kCborUint, kKeyCountsLen);
  CborWriteUint(out, kCborUint, static_cast<uint64_t>(histogram_->counts_len));
  // 10: sparse counts -- array of [delta, count, ...] pairs.
  // Indices are delta-encoded: the first value is the absolute index,
  // each subsequent value is the difference from the previous index.
  CborWriteUint(out, kCborUint, kKeyCounts);
  CborWriteUint(out, kCborArray, static_cast<uint64_t>(non_zero) * 2);
  int32_t prev_idx = 0;
  for (int32_t i = 0; i < histogram_->counts_len; i++) {
    if (histogram_->counts[i] != 0) {
      CborWriteUint(out, kCborUint, static_cast<uint64_t>(i - prev_idx));
      CborWriteUint(
          out, kCborUint, static_cast<uint64_t>(histogram_->counts[i]));
      prev_idx = i;
    }
  }

  // 11: EWMA state (optional)
  if (has_ewma) {
    CborWriteUint(out, kCborUint, kKeyEwma);
    CborWriteUint(out, kCborMap, 5);
    CborWriteUint(out, kCborUint, kEwmaAlpha);
    CborWriteFloat64(out, ewma_alpha_);
    CborWriteUint(out, kCborUint, kEwmaMean);
    CborWriteFloat64(out, ewma_mean_);
    CborWriteUint(out, kCborUint, kEwmaVariance);
    CborWriteFloat64(out, ewma_variance_);
    CborWriteUint(out, kCborUint, kEwmaErrorRate);
    CborWriteFloat64(out, ewma_error_rate_);
    CborWriteUint(out, kCborUint, kEwmaThreshold);
    CborWriteUint(out, kCborUint, static_cast<uint64_t>(threshold_));
  }

  return out;
}

std::shared_ptr<Histogram> Histogram::Import(const uint8_t* data, size_t len) {
  const uint8_t* p = data;
  const uint8_t* end = data + len;

  // Read top-level map header.
  if (p >= end || (*p >> 5) != 5) return nullptr;  // Must be a map.
  uint64_t map_size;
  if (!CborReadUint(p, end, &map_size)) return nullptr;

  int64_t lowest = 1;
  int64_t highest = std::numeric_limits<int64_t>::max();
  int figures = 3;
  int64_t total_count = 0;
  int64_t min_value = std::numeric_limits<int64_t>::max();
  int64_t max_value = 0;
  int32_t norm_offset = 0;
  double conv_ratio = 1.0;
  int32_t counts_len = 0;
  uint64_t version = 0;

  // Sparse counts storage.
  std::vector<std::pair<int32_t, int64_t>> sparse_counts;

  // EWMA state.
  double ewma_alpha = 0;
  double ewma_mean = 0;
  double ewma_variance = 0;
  double ewma_error_rate = 0;
  int64_t threshold = 0;

  for (uint64_t i = 0; i < map_size; i++) {
    // Read key (unsigned int).
    uint64_t key;
    if (!CborReadUint(p, end, &key)) return nullptr;

    switch (key) {
      case kKeyVersion:
        if (!CborReadUint(p, end, &version)) return nullptr;
        if (version != kExportVersion) return nullptr;
        break;
      case kKeyLowest: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        lowest = static_cast<int64_t>(v);
        break;
      }
      case kKeyHighest: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        highest = static_cast<int64_t>(v);
        break;
      }
      case kKeyFigures: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        figures = static_cast<int>(v);
        break;
      }
      case kKeyTotalCount: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        total_count = static_cast<int64_t>(v);
        break;
      }
      case kKeyMin: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        min_value = static_cast<int64_t>(v);
        break;
      }
      case kKeyMax: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        max_value = static_cast<int64_t>(v);
        break;
      }
      case kKeyNormOffset: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        norm_offset = static_cast<int32_t>(v);
        break;
      }
      case kKeyConvRatio:
        if (!CborReadNumber(p, end, &conv_ratio)) return nullptr;
        break;
      case kKeyCountsLen: {
        uint64_t v;
        if (!CborReadUint(p, end, &v)) return nullptr;
        counts_len = static_cast<int32_t>(v);
        break;
      }
      case kKeyCounts: {
        // Array of flat [delta, count, ...] pairs. Indices are
        // delta-encoded: accumulate to recover absolute indices.
        if (p >= end || (*p >> 5) != 4) return nullptr;
        uint64_t arr_len;
        if (!CborReadUint(p, end, &arr_len)) return nullptr;
        if (arr_len % 2 != 0) return nullptr;
        // Each element needs at least 1 byte of CBOR encoding, so
        // arr_len can't exceed the remaining buffer. Without this
        // check, a crafted buffer claiming arr_len=2^60 would cause
        // reserve() to OOM-crash before the loop catches the error.
        if (arr_len > static_cast<uint64_t>(end - p)) return nullptr;
        sparse_counts.reserve(static_cast<size_t>(arr_len / 2));
        int32_t acc_idx = 0;
        for (uint64_t j = 0; j < arr_len; j += 2) {
          uint64_t delta, cnt;
          if (!CborReadUint(p, end, &delta)) return nullptr;
          if (!CborReadUint(p, end, &cnt)) return nullptr;
          acc_idx += static_cast<int32_t>(delta);
          sparse_counts.emplace_back(acc_idx, static_cast<int64_t>(cnt));
        }
        break;
      }
      case kKeyEwma: {
        // Sub-map for EWMA state.
        if (p >= end || (*p >> 5) != 5) return nullptr;
        uint64_t sub_size;
        if (!CborReadUint(p, end, &sub_size)) return nullptr;
        for (uint64_t j = 0; j < sub_size; j++) {
          uint64_t sub_key;
          if (!CborReadUint(p, end, &sub_key)) return nullptr;
          switch (sub_key) {
            case kEwmaAlpha:
              if (!CborReadNumber(p, end, &ewma_alpha)) return nullptr;
              break;
            case kEwmaMean:
              if (!CborReadNumber(p, end, &ewma_mean)) return nullptr;
              break;
            case kEwmaVariance:
              if (!CborReadNumber(p, end, &ewma_variance)) return nullptr;
              break;
            case kEwmaErrorRate:
              if (!CborReadNumber(p, end, &ewma_error_rate)) return nullptr;
              break;
            case kEwmaThreshold: {
              uint64_t v;
              if (!CborReadUint(p, end, &v)) return nullptr;
              threshold = static_cast<int64_t>(v);
              break;
            }
            default:
              return nullptr;  // Unknown EWMA key.
          }
        }
        break;
      }
      default:
        return nullptr;  // Unknown key.
    }
  }

  // Reconstruct the histogram.
  Options opts;
  opts.lowest = lowest;
  opts.highest = highest;
  opts.figures = figures;
  // Compute half_life from alpha: alpha = 1 - 2^(-1/halfLife)
  // => halfLife = -1 / log2(1 - alpha)
  if (ewma_alpha > 0 && ewma_alpha < 1) {
    opts.half_life = -1.0 / std::log2(1.0 - ewma_alpha);
  }
  opts.threshold = threshold;

  auto histogram = Histogram::Create(opts);
  if (!histogram) return nullptr;

  // Validate counts_len matches what the options produce.
  if (histogram->histogram_->counts_len != counts_len) return nullptr;

  // Restore counts directly.
  for (const auto& [idx, cnt] : sparse_counts) {
    if (idx < 0 || idx >= counts_len) return nullptr;
    histogram->histogram_->counts[idx] = cnt;
  }
  histogram->histogram_->total_count = total_count;
  histogram->histogram_->min_value = min_value;
  histogram->histogram_->max_value = max_value;
  histogram->histogram_->normalizing_index_offset = norm_offset;
  histogram->histogram_->conversion_ratio = conv_ratio;

  // Restore EWMA state.
  if (ewma_alpha > 0) {
    histogram->ewma_mean_ = ewma_mean;
    histogram->ewma_variance_ = ewma_variance;
    histogram->ewma_error_rate_ = ewma_error_rate;
    histogram->ewma_initialized_ = true;
  }

  return histogram;
}

HistogramImpl::HistogramImpl(const Histogram::Options& options)
    : histogram_(Histogram::Create(options)) {
  CHECK(histogram_);
}

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
CFunction HistogramImpl::fast_get_ewma_mean_(
    CFunction::Make(&HistogramImpl::FastGetEwmaMean));
CFunction HistogramImpl::fast_get_ewma_stddev_(
    CFunction::Make(&HistogramImpl::FastGetEwmaStddev));
CFunction HistogramImpl::fast_get_ewma_error_rate_(
    CFunction::Make(&HistogramImpl::FastGetEwmaErrorRate));
CFunction HistogramBase::fast_record_(
    CFunction::Make(&HistogramBase::FastRecord));
CFunction HistogramBase::fast_record_delta_(
    CFunction::Make(&HistogramBase::FastRecordDelta));
CFunction SlidingWindowHistogram::fast_record_(
    CFunction::Make(&SlidingWindowHistogram::FastRecord));
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
  SetProtoMethodNoSideEffect(isolate, tmpl, "meanCI", GetMeanCI);
  SetProtoMethodNoSideEffect(isolate, tmpl, "welchTest", GetWelchTest);
  SetProtoMethodNoSideEffect(
      isolate, tmpl, "mannWhitneyTest", GetMannWhitneyTest);
  SetProtoMethodNoSideEffect(isolate, tmpl, "cohensD", GetCohensD);
  SetProtoMethodNoSideEffect(isolate, tmpl, "cliffsD", GetCliffsD);
  SetProtoMethodNoSideEffect(isolate, tmpl, "percentileCI", GetPercentileCI);
  SetProtoMethod(isolate, tmpl, "qrde", GetQrde);
  SetFastMethodNoSideEffect(
      isolate, instance, "ewmaMean", GetEwmaMean, &fast_get_ewma_mean_);
  SetFastMethodNoSideEffect(
      isolate, instance, "ewmaStddev", GetEwmaStddev, &fast_get_ewma_stddev_);
  SetFastMethodNoSideEffect(isolate,
                            instance,
                            "ewmaErrorRate",
                            GetEwmaErrorRate,
                            &fast_get_ewma_error_rate_);
  SetProtoMethodNoSideEffect(isolate, tmpl, "export", DoExport);
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
  registry->Register(GetMeanCI);
  registry->Register(GetWelchTest);
  registry->Register(GetMannWhitneyTest);
  registry->Register(GetCohensD);
  registry->Register(GetCliffsD);
  registry->Register(GetPercentileCI);
  registry->Register(GetQrde);
  registry->Register(GetEwmaMean);
  registry->Register(GetEwmaStddev);
  registry->Register(GetEwmaErrorRate);
  registry->Register(DoExport);
  registry->Register(fast_get_ewma_mean_);
  registry->Register(fast_get_ewma_stddev_);
  registry->Register(fast_get_ewma_error_rate_);
  registry->Register(fast_get_skewness_);
  registry->Register(fast_get_kurtosis_);
  registry->Register(fast_get_cdf_);
  registry->Register(fast_get_count_at_);
  is_registered = true;
}

HistogramBase::HistogramBase(Environment* env,
                             Local<Object> wrap,
                             const Histogram::Options& options)
    : BaseObject(env, wrap), HistogramImpl(options) {
  MakeWeak();
  wrap->SetAlignedPointerInInternalField(
      HistogramImpl::InternalFields::kImplField,
      static_cast<HistogramImpl*>(this),
      EmbedderDataTag::kDefault);
}

HistogramBase::HistogramBase(Environment* env,
                             Local<Object> wrap,
                             std::shared_ptr<Histogram> histogram)
    : BaseObject(env, wrap), HistogramImpl(std::move(histogram)) {
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
    Environment* env, const Histogram::Options& options) {
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
    Environment* env, std::shared_ptr<Histogram> histogram) {
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
  double half_life = 0;
  if (args.Length() > 3 && args[3]->IsNumber()) {
    half_life = args[3].As<Number>()->Value();
  }
  int64_t threshold = 0;
  if (args.Length() > 4 && args[4]->IsNumber()) {
    threshold = static_cast<int64_t>(args[4].As<Number>()->Value());
  }
  auto histogram = Histogram::Create(
      Histogram::Options{lowest, highest, figures, half_life, threshold});
  if (!histogram) {
    return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid histogram options");
  }
  new HistogramBase(env, args.This(), std::move(histogram));
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
  registry->Register(HistogramImpl::DoImport);
  HistogramImpl::RegisterExternalReferences(registry);
}

void HistogramBase::Initialize(IsolateData* isolate_data,
                               Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(isolate_data);
  SetMethodNoSideEffect(isolate, tmpl, "import", HistogramImpl::DoImport);
  SetConstructorFunction(
      isolate, target, "Histogram", tmpl, SetConstructorFunctionFlag::NONE);
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

SlidingWindowHistogram::SlidingWindowHistogram(
    Environment* env,
    Local<Object> wrap,
    const Histogram::Options& options,
    size_t chunk_count,
    bool time_based,
    uint64_t rotate_at,
    std::shared_ptr<Histogram> spare)
    : BaseObject(env, wrap),
      options_(options),
      chunks_(chunk_count),
      generations_(chunk_count, kNoGeneration),
      spare_(std::move(spare)),
      time_based_(time_based),
      rotate_at_(rotate_at),
      origin_(uv_hrtime()) {
  MakeWeak();
  external_memory_ = spare_->GetMemorySize();
  env->external_memory_accounter()->Increase(env->isolate(), external_memory_);
}

SlidingWindowHistogram::~SlidingWindowHistogram() {
  env()->external_memory_accounter()->Decrease(env()->isolate(),
                                               external_memory_);
}

void SlidingWindowHistogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("chunks", chunks_);
  tracker->TrackField("generations", generations_);
  tracker->TrackField("spare", spare_);
}

uint64_t SlidingWindowHistogram::CurrentTimeGeneration() const {
  const uint64_t now = uv_hrtime();
  CHECK_GE(now, origin_);
  return (now - origin_) / rotate_at_;
}

Histogram* SlidingWindowHistogram::GetChunk(uint64_t generation) {
  const size_t index = generation % chunks_.size();
  if (generations_[index] == generation) {
    CHECK(chunks_[index]);
    return chunks_[index].get();
  }

  if (chunks_[index]) {
    chunks_[index]->Reset();
  } else if (spare_) {
    chunks_[index] = std::move(spare_);
  } else {
    chunks_[index] = Histogram::Create(options_);
    if (!chunks_[index]) return nullptr;
    const size_t size = chunks_[index]->GetMemorySize();
    external_memory_ += size;
    env()->external_memory_accounter()->Increase(env()->isolate(), size);
  }

  generations_[index] = generation;
  return chunks_[index].get();
}

bool SlidingWindowHistogram::RecordValue(int64_t value) {
  uint64_t generation;
  if (time_based_) {
    generation = CurrentTimeGeneration();
  } else if (records_in_current_chunk_ == rotate_at_) {
    CHECK_LT(current_generation_, kNoGeneration - 1);
    generation = current_generation_ + 1;
  } else {
    generation = current_generation_;
  }

  Histogram* chunk = GetChunk(generation);
  if (chunk == nullptr) return false;

  chunk->Record(value);
  if (!time_based_) {
    if (generation != current_generation_) {
      current_generation_ = generation;
      records_in_current_chunk_ = 0;
    }
    records_in_current_chunk_++;
    has_count_records_ = true;
  }
  return true;
}

std::shared_ptr<Histogram> SlidingWindowHistogram::CreateSnapshot() const {
  std::shared_ptr<Histogram> snapshot = Histogram::Create(options_);
  if (!snapshot) return {};

  uint64_t current_generation;
  if (time_based_) {
    current_generation = CurrentTimeGeneration();
  } else {
    if (!has_count_records_) return snapshot;
    current_generation = current_generation_;
  }

  for (size_t i = 0; i < chunks_.size(); i++) {
    const uint64_t generation = generations_[i];
    if (generation == kNoGeneration || generation > current_generation ||
        current_generation - generation >= chunks_.size()) {
      continue;
    }
    CHECK(chunks_[i]);
    CHECK_EQ(snapshot->Add(*chunks_[i]), 0);
  }
  return snapshot;
}

void SlidingWindowHistogram::ResetWindow() {
  std::fill(generations_.begin(), generations_.end(), kNoGeneration);
  origin_ = uv_hrtime();
  current_generation_ = 0;
  records_in_current_chunk_ = 0;
  has_count_records_ = false;
}

void SlidingWindowHistogram::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  CHECK_IMPLIES(!args[1]->IsNumber(), args[1]->IsBigInt());
  CHECK(args[2]->IsUint32());
  CHECK(args[3]->IsUint32());
  CHECK(args[4]->IsBoolean());
  CHECK(args[5]->IsBigInt());

  Environment* env = Environment::GetCurrent(args);
  bool lossless = true;
  int64_t lowest = 1;
  int64_t highest = std::numeric_limits<int64_t>::max();

  if (args[0]->IsNumber()) {
    lowest = args[0].As<Integer>()->Value();
  } else {
    lowest = args[0].As<BigInt>()->Int64Value(&lossless);
    if (!lossless)
      return THROW_ERR_OUT_OF_RANGE(env, "options.lowest is out of range");
  }

  if (args[1]->IsNumber()) {
    highest = args[1].As<Integer>()->Value();
  } else {
    highest = args[1].As<BigInt>()->Int64Value(&lossless);
    if (!lossless)
      return THROW_ERR_OUT_OF_RANGE(env, "options.highest is out of range");
  }

  const int figures = args[2].As<Uint32>()->Value();
  const uint32_t chunk_count = args[3].As<Uint32>()->Value();
  if (chunk_count == 0)
    return THROW_ERR_OUT_OF_RANGE(env, "options.chunks is out of range");

  lossless = true;
  const uint64_t rotate_at = args[5].As<BigInt>()->Uint64Value(&lossless);
  if (!lossless || rotate_at == 0) {
    return THROW_ERR_OUT_OF_RANGE(env, "rotation interval is out of range");
  }

  Histogram::Options options{lowest, highest, figures};
  std::shared_ptr<Histogram> spare = Histogram::Create(options);
  if (!spare)
    return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid histogram options");

  new SlidingWindowHistogram(env,
                             args.This(),
                             options,
                             chunk_count,
                             args[4]->IsTrue(),
                             rotate_at,
                             std::move(spare));
}

void SlidingWindowHistogram::Record(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  bool lossless = true;
  const int64_t value =
      args[0]->IsBigInt() ? args[0].As<BigInt>()->Int64Value(&lossless)
                          : static_cast<int64_t>(args[0].As<Number>()->Value());
  if (!lossless || value < 1)
    return THROW_ERR_OUT_OF_RANGE(env, "value is out of range");

  SlidingWindowHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());
  if (!histogram->RecordValue(value)) THROW_ERR_MEMORY_ALLOCATION_FAILED(env);
}

void SlidingWindowHistogram::FastRecord(Local<Value> receiver,
                                        int64_t value,
                                        // NOLINTNEXTLINE(runtime/references)
                                        FastApiCallbackOptions& options) {
  CHECK_GE(value, 1);
  TRACK_V8_FAST_API_CALL("histogram.slidingWindow.record");
  SlidingWindowHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, receiver);
  if (!histogram->RecordValue(value)) {
    HandleScope scope(options.isolate);
    THROW_ERR_MEMORY_ALLOCATION_FAILED(histogram->env());
  }
}

void SlidingWindowHistogram::Snapshot(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SlidingWindowHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());

  std::shared_ptr<Histogram> snapshot = histogram->CreateSnapshot();
  if (!snapshot) return THROW_ERR_MEMORY_ALLOCATION_FAILED(env);

  BaseObjectPtr<HistogramBase> result =
      HistogramBase::Create(env, std::move(snapshot));
  if (result) args.GetReturnValue().Set(result->object());
}

void SlidingWindowHistogram::Reset(const FunctionCallbackInfo<Value>& args) {
  SlidingWindowHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.This());
  histogram->ResetWindow();
}

void SlidingWindowHistogram::Initialize(IsolateData* isolate_data,
                                        Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, New);
  tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "SlidingWindowHistogram"));
  auto instance = tmpl->InstanceTemplate();
  instance->SetInternalFieldCount(BaseObject::kInternalFieldCount);
  SetFastMethod(isolate, instance, "record", Record, &fast_record_);
  SetProtoMethod(isolate, tmpl, "snapshot", Snapshot);
  SetProtoMethod(isolate, tmpl, "reset", Reset);
  SetConstructorFunction(isolate,
                         target,
                         "SlidingWindowHistogram",
                         tmpl,
                         SetConstructorFunctionFlag::NONE);
}

void SlidingWindowHistogram::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Record);
  registry->Register(fast_record_);
  registry->Register(Snapshot);
  registry->Register(Reset);
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
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
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
  if (flags == StartFlags::RESET) histogram()->Reset();
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

void HistogramImpl::GetMeanCI(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  double confidence = args[0].As<Number>()->Value();

  auto result = (*histogram)->MeanCI(confidence);

  Local<Value> values[] = {Number::New(isolate, result.mean),
                           Number::New(isolate, result.lower),
                           Number::New(isolate, result.upper)};
  Local<Array> arr = Array::New(isolate, &values[0], arraysize(values));
  args.GetReturnValue().Set(arr);
}

void HistogramImpl::GetWelchTest(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  HistogramImpl* other = HistogramImpl::FromJSObject(args[0]);
  CHECK(args[1]->IsNumber());
  double confidence = args[1].As<Number>()->Value();

  auto result = (*histogram)->WelchTest(*(other->histogram()), confidence);

  Local<Value> values[] = {Number::New(isolate, result.t_statistic),
                           Number::New(isolate, result.degrees_of_freedom),
                           Number::New(isolate, result.p_value),
                           Number::New(isolate, result.ci_lower),
                           Number::New(isolate, result.ci_upper)};
  Local<Array> arr = Array::New(isolate, &values[0], arraysize(values));
  args.GetReturnValue().Set(arr);
}

void HistogramImpl::GetMannWhitneyTest(
    const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  HistogramImpl* other = HistogramImpl::FromJSObject(args[0]);

  auto result = (*histogram)->MannWhitneyTest(*(other->histogram()));

  Local<Value> values[] = {Number::New(isolate, result.u_statistic),
                           Number::New(isolate, result.z_score),
                           Number::New(isolate, result.p_value)};
  Local<Array> arr = Array::New(isolate, &values[0], arraysize(values));
  args.GetReturnValue().Set(arr);
}

void HistogramImpl::GetCohensD(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  HistogramImpl* other = HistogramImpl::FromJSObject(args[0]);
  args.GetReturnValue().Set((*histogram)->CohensD(*(other->histogram())));
}

void HistogramImpl::GetCliffsD(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  HistogramImpl* other = HistogramImpl::FromJSObject(args[0]);
  args.GetReturnValue().Set((*histogram)->CliffsD(*(other->histogram())));
}

void HistogramImpl::GetPercentileCI(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  double confidence = args[1].As<Number>()->Value();

  auto result = (*histogram)->PercentileCI(percentile, confidence);

  Local<Value> values[] = {
      Number::New(isolate, static_cast<double>(result.value)),
      Number::New(isolate, static_cast<double>(result.lower)),
      Number::New(isolate, static_cast<double>(result.upper))};
  Local<Array> arr = Array::New(isolate, &values[0], arraysize(values));
  args.GetReturnValue().Set(arr);
}

void HistogramImpl::GetQrde(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsFloat64Array());
  CHECK(args[1]->IsUint32());
  CHECK(args[2]->IsBoolean());
  Local<Float64Array> input = args[0].As<Float64Array>();
  CHECK_GE(input->Length(), 2);
  CHECK_LE(input->Length(), 1001);
  auto backing = input->Buffer()->GetBackingStore();
  const double* data = reinterpret_cast<const double*>(
      static_cast<const char*>(backing->Data()) + input->ByteOffset());
  std::vector<double> probabilities(data, data + input->Length());
  const uint32_t dequantization = args[1].As<Uint32>()->Value();
  CHECK_LE(dequantization, static_cast<uint32_t>(QrdeDequantization::kAll));
  const bool cache_snapshot = args[2]->IsTrue();

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) return;

  auto histogram_ptr = histogram->histogram();
  auto snapshot_source =
      histogram_ptr->GetRecordedSnapshotSource(cache_snapshot);
  if (!snapshot_source.histogram && !snapshot_source.snapshot) {
    THROW_ERR_MEMORY_ALLOCATION_FAILED(env);
    return;
  }
  std::shared_ptr<Histogram> cache_target;
  if (cache_snapshot && !snapshot_source.cache_hit) {
    cache_target = histogram_ptr;
  }
  auto* job = new QrdeJob(env,
                          resolver,
                          std::move(cache_target),
                          std::move(snapshot_source),
                          std::move(probabilities),
                          static_cast<QrdeDequantization>(dequantization));
  args.GetReturnValue().Set(resolver->GetPromise());
  job->ScheduleWork();
}

void HistogramImpl::GetEwmaMean(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->EwmaMean());
}

double HistogramImpl::FastGetEwmaMean(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.ewmaMean");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->EwmaMean();
}

void HistogramImpl::GetEwmaStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->EwmaStddev());
}

double HistogramImpl::FastGetEwmaStddev(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.ewmaStddev");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->EwmaStddev();
}

void HistogramImpl::GetEwmaErrorRate(const FunctionCallbackInfo<Value>& args) {
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  args.GetReturnValue().Set((*histogram)->EwmaErrorRate());
}

double HistogramImpl::FastGetEwmaErrorRate(Local<Value> receiver) {
  TRACK_V8_FAST_API_CALL("histogram.ewmaErrorRate");
  HistogramImpl* histogram = HistogramImpl::FromJSObject(receiver);
  return (*histogram)->EwmaErrorRate();
}

void HistogramImpl::DoExport(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  std::vector<uint8_t> data = (*histogram)->Export();

  auto store = ArrayBuffer::NewBackingStore(env->isolate(), data.size());
  memcpy(store->Data(), data.data(), data.size());
  auto buf = ArrayBuffer::New(env->isolate(), std::move(store));
  auto arr = Uint8Array::New(buf, 0, data.size());
  args.GetReturnValue().Set(arr);
}

void HistogramImpl::DoImport(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args[0]->IsUint8Array()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "data must be a Uint8Array");
    return;
  }
  Local<Uint8Array> input = args[0].As<Uint8Array>();
  auto backing = input->Buffer()->GetBackingStore();
  const uint8_t* data =
      static_cast<const uint8_t*>(backing->Data()) + input->ByteOffset();
  size_t len = input->ByteLength();

  auto histogram = Histogram::Import(data, len);
  if (!histogram) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Invalid histogram export data");
    return;
  }

  // Create a new HistogramBase wrapping the imported histogram.
  Local<FunctionTemplate> tmpl =
      HistogramBase::GetConstructorTemplate(env->isolate_data());
  Local<Object> obj;
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj))
    return;
  new HistogramBase(env, obj, std::move(histogram));
  args.GetReturnValue().Set(obj);
}

void HistogramImpl::GetPercentilesAt(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramImpl* histogram = HistogramImpl::FromJSObject(args.This());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  CHECK(args[1]->IsFloat64Array());
  Local<Float64Array> input = args[1].As<Float64Array>();
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

std::unique_ptr<worker::TransferData> IntervalHistogram::CloneForMessaging()
    const {
  return std::make_unique<HistogramBase::HistogramTransferData>(histogram());
}

}  // namespace node
