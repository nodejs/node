// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <cmath>
#include <memory>
#include <string>

#include "absl/time/clock.h"
#include "src/bigint/bigint-inl.h"
#include "src/bigint/bigint-internal.h"
#include "src/bigint/div-helpers-inl.h"

namespace v8 {
namespace bigint {
namespace test {

int PrintHelp(char** argv) {
  std::cerr << "Usage:\n"
            << argv[0] << " --help\n"
            << "    Print this help and exit.\n"
            << argv[0] << " --list\n"
            << "    List supported tests.\n"
            << argv[0] << " <testname>\n"
            << "    Run the specified test (see --list for a list).\n"
            << "\nOptions when running tests:\n"
            << "--random-seed R\n"
            << "    Initialize the random number generator with this seed.\n"
            << "--runs N\n"
            << "    Repeat the test N times.\n";
  return 1;
}

#define TESTS(V)                    \
  V(Barrett, "barrett")             \
  V(Burnikel, "burnikel")           \
  V(CachedMod, "cachedmod")         \
  V(FFT, "fft")                     \
  V(FromString, "fromstring")       \
  V(FromStringBase2, "fromstring2") \
  V(Karatsuba, "karatsuba")         \
  V(Toom, "toom")                   \
  V(ToString, "tostring")

enum Operation { kNoOp, kList, kTest, kThresholds };

enum Test {
#define TEST(Name, name) k##Name,
  TESTS(TEST)
#undef TEST
};

class RNG {
 public:
  RNG() = default;

  void Initialize(int64_t seed) {
    state0_ = MurmurHash3(static_cast<uint64_t>(seed));
    state1_ = MurmurHash3(~state0_);
    CHECK(state0_ != 0 || state1_ != 0);
  }

  uint64_t NextUint64() {
    XorShift128(&state0_, &state1_);
    return static_cast<uint64_t>(state0_ + state1_);
  }

  static inline void XorShift128(uint64_t* state0, uint64_t* state1) {
    uint64_t s1 = *state0;
    uint64_t s0 = *state1;
    *state0 = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    *state1 = s1;
  }

  static uint64_t MurmurHash3(uint64_t h) {
    h ^= h >> 33;
    h *= uint64_t{0xFF51AFD7ED558CCD};
    h ^= h >> 33;
    h *= uint64_t{0xC4CEB9FE1A85EC53};
    h ^= h >> 33;
    return h;
  }

 private:
  uint64_t state0_;
  uint64_t state1_;
};

// Fills {chars} with {char_count} random decimal digits.
void GenerateRandomDecimalDigits(RNG& rng, char* chars, uint32_t char_count) {
  uint64_t random = 0;
  for (uint32_t i = 0; i < char_count; i++) {
    if (random < 10) random = rng.NextUint64();
    chars[i] = static_cast<char>('0' + random % 10);
    random /= 10;
  }
}

// Fills {Z} with random bits, each bit independent from the others.
// Useful for benchmarking.
void GenerateRandomBits(RNG& rng, RWDigits Z) {
  if (sizeof(digit_t) == 8) {
    for (uint32_t i = 0; i < Z.len(); i++) {
      Z[i] = static_cast<digit_t>(rng.NextUint64());
    }
  } else {
    for (uint32_t i = 0; i < Z.len(); i += 2) {
      uint64_t random = rng.NextUint64();
      Z[i] = static_cast<digit_t>(random);
      if (i + 1 < Z.len()) Z[i + 1] = static_cast<digit_t>(random >> 32);
    }
  }
  // Special case: we don't want the MSD to be zero.
  while (Z.msd() == 0) {
    Z[Z.len() - 1] = static_cast<digit_t>(rng.NextUint64());
  }
}

// Fills {Z} with random bits, prioritizing patterns that tend to flush out
// bugs, i.e. large-ish sequences of all-zero or all-one bits.
void GenerateRandom(RNG& rng, RWDigits Z) {
  if (Z.len() == 0) return;
  int mode = static_cast<int>(rng.NextUint64() & 3);
  if (mode == 0) {
    return GenerateRandomBits(rng, Z);
  }
  if (mode == 1) {
    // Generate a power of 2, with the lone 1-bit somewhere in the MSD.
    int bit_in_msd = static_cast<int>(rng.NextUint64() % kDigitBits);
    Z[Z.len() - 1] = digit_t{1} << bit_in_msd;
    for (uint32_t i = 0; i < Z.len() - 1; i++) Z[i] = 0;
    return;
  }
  // For mode == 2 and mode == 3, generate a random number of 1-bits in the
  // MSD, aligned to the least-significant end.
  int bits_in_msd = static_cast<int>(rng.NextUint64() % kDigitBits);
  digit_t msd = (digit_t{1} << bits_in_msd) - 1;
  if (msd == 0) msd = ~digit_t{0};
  Z[Z.len() - 1] = msd;
  if (mode == 2) {
    // The non-MSD digits are all 1-bits.
    for (uint32_t i = 0; i < Z.len() - 1; i++) Z[i] = ~digit_t{0};
  } else {
    // mode == 3
    // Each non-MSD digit is either all ones or all zeros.
    uint64_t random;
    int random_bits = 0;
    for (uint32_t i = 0; i < Z.len() - 1; i++) {
      if (random_bits == 0) {
        random = rng.NextUint64();
        random_bits = 64;
      }
      Z[i] = random & 1 ? ~digit_t{0} : digit_t{0};
      random >>= 1;
      random_bits--;
    }
  }
}

static constexpr int kCharsPerDigit = kDigitBits / 4;

static const char kConversionChars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

std::string FormatHex(Digits X) {
  X.Normalize();
  if (X.len() == 0) return "0";
  digit_t msd = X.msd();
  const int msd_leading_zeros = CountLeadingZeros(msd);
  const size_t bit_length = X.len() * kDigitBits - msd_leading_zeros;
  const size_t chars = DIV_CEIL(bit_length, 4);

  if (chars > 100000) {
    return std::string("<BigInt with ") + std::to_string(bit_length) +
           std::string(" bits>");
  }

  std::unique_ptr<char[]> result(new char[chars]);
  for (size_t i = 0; i < chars; i++) result[i] = '?';
  // Print the number into the string, starting from the last position.
  int pos = static_cast<int>(chars - 1);
  for (uint32_t i = 0; i < X.len() - 1; i++) {
    digit_t d = X[i];
    for (int j = 0; j < kCharsPerDigit; j++) {
      result[pos--] = kConversionChars[d & 15];
      d = static_cast<digit_t>(d >> 4u);
    }
  }
  while (msd != 0) {
    result[pos--] = kConversionChars[msd & 15];
    msd = static_cast<digit_t>(msd >> 4u);
  }
  CHECK(pos == -1);
  return std::string(result.get(), chars);
}

class ThresholdRunner {
 public:
  enum Which {
    kKaratsuba,
    kBurnikel,
    kToString,
    kFromString,
#if V8_ADVANCED_BIGINT_ALGORITHMS
    kToom,
    kFFT,
    kBarrett,
    kNewton,
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
  };
  using Calculator = void (*)(ProcessorImpl*, RWDigits, Digits, Digits);

  explicit ThresholdRunner(ProcessorImpl* processor, RNG& rng)
      : processor_(processor), rng_(rng) {}

  void Measure(Which which) {
    uint32_t estimate;
    std::string base_name;
    std::string other_name;
    Calculator base;
    Calculator other;
    uint32_t a_len_factor = 1;  // 2 for divisions.
    switch (which) {
      case kKaratsuba:
        estimate = config::kKaratsubaThreshold;
        base = &Schoolbook;
        other = &Karatsuba;
        base_name = "Schoolbook";
        other_name = "Karatsuba";
        break;
      case kBurnikel:
        a_len_factor = 2;
        estimate = config::kBurnikelThreshold;
        base = &DivideSchoolbook;
        other = &Burnikel;
        base_name = "Schoolbook";
        other_name = "Burnikel-Ziegler";
        break;
      case kToString:
        estimate = config::kToStringFastThreshold;
        base_name = "Classic";
        other_name = "Fast ToString";
        break;
      case kFromString:
        estimate = config::kFromStringLargeThreshold;
        base_name = "Classic";
        other_name = "Large FromString";
        break;
#if V8_ADVANCED_BIGINT_ALGORITHMS
      case kToom:
        estimate = config::kToomThreshold;
        base = &Karatsuba;
        other = &Toom;
        base_name = "Karatsuba";
        other_name = "Toom-Cook";
        break;
      case kFFT:
        estimate = config::kFftThreshold;
        base = &Toom;
        other = &FFT;
        base_name = "Toom-Cook";
        other_name = "FFT";
        break;
      case kBarrett:
        a_len_factor = 2;
        estimate = config::kBarrettThreshold;
        base = &Burnikel;
        other = &Barrett;
        base_name = "Burnikel-Ziegler";
        other_name = "Barrett";
        break;
      case kNewton:
        estimate = config::kNewtonInversionThreshold;
        base = &Invert;
        other = &Newton;
        base_name = "Invert";
        other_name = "Newton";
        break;
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
    }

    std::cout << "#########################################################\n";
    std::cout << "##  " << base_name << " vs. " << other_name << "\n";
    std::cout << "#########################################################\n";
    uint32_t min = estimate * 0.5;
    uint32_t max = estimate * 2;
    uint32_t delta = (max - min) / 100;
    if (delta == 0) delta = 1;
    bool finding_lower = true;
    bool finding_upper = true;
    uint32_t lower = 0;
    uint32_t upper = 0;

    // Over-allocate all storage to support bumping {max} by up to 2x if
    // needed.
    Storage a_mem(2 * max * a_len_factor, processor_->platform());
    uint32_t b_mem_size =
#if V8_ADVANCED_BIGINT_ALGORITHMS
        which == kNewton ? InvertNewtonScratchSpace(2 * max) :
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
                         2 * max;
    Storage b_mem(b_mem_size, processor_->platform());
    // Multiplication needs 2 * size (where size <= 2 * max).
    // Division needs size + 1 for the quotient plus size for the remainder.
    Storage z_mem(4 * max + 1, processor_->platform());
    // We can't simply use {ToStringResultLength} because that wants to look
    // at specific Digits, whereas we preallocate for a maximum size. The
    // production code approximates lengths, so we add some slack.
    // {log2} will be constexpr in C++26.
    static const double kBitsPerDecimalDigit = log2(10);
    static constexpr uint32_t kSlack = 20;
    uint32_t string_len =
        ceil(2 * max * kDigitBits / kBitsPerDecimalDigit) + kSlack;
    const uint32_t original_string_len = string_len;
    char* string = new char[string_len];
    for (uint32_t size = min; size <= max; size += delta) {
      RWDigits A(a_mem.get(), size * a_len_factor);
      uint32_t b_len =
#if V8_ADVANCED_BIGINT_ALGORITHMS
          which == kNewton ? InvertNewtonScratchSpace(size) :
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
                           size;
      RWDigits B(b_mem.get(), b_len);
      RWDigits Z(z_mem.get(), size * 2 + 1);

      int64_t base_total = 0;
      int64_t other_total = 0;
      int count = 0;

      static constexpr int64_t kMinNs = 100'000'000;
      while (base_total < kMinNs && other_total < kMinNs && count < 1000) {
        if (which == kFromString) {
          uint32_t char_count = floor(size * kDigitBits / kBitsPerDecimalDigit);
          GenerateRandomDecimalDigits(rng_, string, char_count);
          {
            int64_t start = absl::GetCurrentTimeNanos();
            FromStringAccumulator acc(2 * max, processor_->platform());
            acc.Parse(string, string + char_count, 10);
            processor_->FromStringClassic(Z, &acc);
            base_total += absl::GetCurrentTimeNanos() - start;
          }
          {
            int64_t start = absl::GetCurrentTimeNanos();
            FromStringAccumulator acc(2 * max, processor_->platform());
            acc.Parse(string, string + char_count, 10);
            processor_->FromStringLarge(Z, &acc);
            other_total += absl::GetCurrentTimeNanos() - start;
          }
          count++;
          continue;
        }
        GenerateRandomBits(rng_, A);
        if (which == kToString) {
          string_len = original_string_len;
          int64_t start = absl::GetCurrentTimeNanos();
          processor_->ToStringImpl(string, &string_len, A, 10, false, false);
          base_total += absl::GetCurrentTimeNanos() - start;

          string_len = original_string_len;
          start = absl::GetCurrentTimeNanos();
          processor_->ToStringImpl(string, &string_len, A, 10, false, true);
          other_total += absl::GetCurrentTimeNanos() - start;
          count++;
          continue;
        }

#if V8_ADVANCED_BIGINT_ALGORITHMS
        if (which == kNewton) {
          int shift = CountLeadingZeros(A.msd());
          if (shift != 0) LeftShift(A, A, shift);
        } else {
          GenerateRandomBits(rng_, B);
        }
#else
        GenerateRandomBits(rng_, B);
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
        int64_t start = absl::GetCurrentTimeNanos();
        base(processor_, Z, A, B);
        base_total += absl::GetCurrentTimeNanos() - start;

        start = absl::GetCurrentTimeNanos();
        other(processor_, Z, A, B);
        other_total += absl::GetCurrentTimeNanos() - start;
        count++;
      }
      if (base_total < other_total) {
        if (finding_lower) lower = size + 1;
        finding_upper = true;
        // Make sure we keep trying until we have an upper bound.
        max = std::max(max, size * 11 / 10);
        // But not beyond what we have allocated memory for.
        max = std::min(estimate * 4, max);
      } else {
        finding_lower = false;
        if (finding_upper) {
          upper = size;
          finding_upper = false;
          // Make sure we get enough data points above the first success case.
          max = std::max(max, size * 11 / 10);
          // But not beyond what we have allocated memory for.
          max = std::min(estimate * 4, max);
        }
      }
      Plot(base_total, other_total);
      int64_t base_time = base_total / count / 1000;
      int64_t other_time = other_total / count / 1000;
      std::cout << line_ << " size: " << size << " " << base_name << ": "
                << base_time << " μs, " << other_name << ": " << other_time
                << " μs (" << count << " data points)\n";
    }
    std::cout << "A good threshold would be between " << lower << " and "
              << upper << ".\n\n";
  }

 private:
  static void Schoolbook(ProcessorImpl*, RWDigits Z, Digits A, Digits B) {
    MultiplySchoolbook(Z, A, B);
  }
  static void Karatsuba(ProcessorImpl* processor, RWDigits Z, Digits A,
                        Digits B) {
    processor->MultiplyKaratsuba(Z, A, B);
  }
  static void DivideSchoolbook(ProcessorImpl* processor, RWDigits Z, Digits A,
                               Digits B) {
    RWDigits ignored_remainder(nullptr, 0);
    processor->DivideSchoolbook(Z, ignored_remainder, A, B);
  }
  static void Burnikel(ProcessorImpl* processor, RWDigits Z, Digits A,
                       Digits B) {
    RWDigits ignored_remainder(nullptr, 0);
    processor->DivideBurnikelZiegler(Z, ignored_remainder, A, B);
  }
#if V8_ADVANCED_BIGINT_ALGORITHMS
  static void Toom(ProcessorImpl* processor, RWDigits Z, Digits A, Digits B) {
    processor->MultiplyToomCook(Z, A, B);
  }
  static void FFT(ProcessorImpl* processor, RWDigits Z, Digits A, Digits B) {
    processor->MultiplyFFT(Z, A, B);
  }
  static void Barrett(ProcessorImpl* processor, RWDigits Z, Digits A,
                      Digits B) {
    // The implementation requires a non-empty remainder. Z is large enough
    // for both that and the quotient.
    RWDigits R(Z, 0, B.len());
    RWDigits Q = Z + B.len();
    processor->DivideBarrett(Q, R, A, B);
  }
  // Hack: we don't need a second argument, but we need all of these functions
  // to have the same signature, so we pass the scratch space there.
  static void Invert(ProcessorImpl* processor, RWDigits Z, Digits A, Digits B) {
    RWDigits scratch(const_cast<digit_t*>(B.digits()), B.len());
    processor->InvertBasecase(Z, A, scratch);
  }
  static void Newton(ProcessorImpl* processor, RWDigits Z, Digits A, Digits B) {
    RWDigits scratch(const_cast<digit_t*>(B.digits()), B.len());
    processor->InvertNewton(Z, A, scratch);
  }
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS

  void Plot(int64_t base, int64_t other) {
    int percent = static_cast<int>(
        (100.0 * static_cast<double>(other) / static_cast<double>(base)));
    const int pos_bar = 50;
    int pos_star = std::max(0, percent - 50);
    pos_star = std::min(pos_star, 100);
    line_[prev_pos_star_] = ' ';
    line_[pos_bar] = '|';
    line_[pos_star] = '*';
    prev_pos_star_ = pos_star;
  }

  std::string line_{std::string(101, ' ')};
  int prev_pos_star_{0};
  ProcessorImpl* processor_;
  RNG& rng_;
};

class Runner {
 public:
  Runner() = default;

  void Initialize() {
    rng_.Initialize(random_seed_);
    processor_.reset(Processor::New(new DefaultPlatform()));
    platform_ = processor_->platform();
  }

  ProcessorImpl* processor() {
    return static_cast<ProcessorImpl*>(processor_.get());
  }

  int Run() {
    if (op_ == kList) {
      ListTests();
    } else if (op_ == kTest) {
      RunTest();
    } else if (op_ == kNoOp) {
      // Probably just --help, do nothing.
    } else if (op_ == kThresholds) {
      RunThresholds();
    } else {
      DCHECK(false);  // Unreachable.
    }
    return 0;
  }

  void ListTests() {
#define PRINT(kName, name) std::cout << name << "\n";
    TESTS(PRINT)
#undef PRINT
  }

  void AssertEquals(Digits input1, Digits input2, Digits expected,
                    Digits actual) {
    if (Compare(expected, actual) == 0) return;
    std::cerr << "Input 1:  " << FormatHex(input1) << "\n";
    std::cerr << "Input 2:  " << FormatHex(input2) << "\n";
    std::cerr << "Expected: " << FormatHex(expected) << "\n";
    std::cerr << "Actual:   " << FormatHex(actual) << "\n";
    error_ = true;
  }

  void AssertEquals(Digits X, int radix, char* expected, int expected_length,
                    char* actual, int actual_length) {
    if (expected_length == actual_length &&
        std::memcmp(expected, actual, actual_length) == 0) {
      return;
    }
    std::cerr << "Input:    " << FormatHex(X) << "\n";
    std::cerr << "Radix:    " << radix << "\n";
    std::cerr << "Expected: " << std::string(expected, expected_length) << "\n";
    std::cerr << "Actual:   " << std::string(actual, actual_length) << "\n";
    error_ = true;
  }

  void AssertEquals(const char* input, int input_length, int radix,
                    Digits expected, Digits actual) {
    if (Compare(expected, actual) == 0) return;
    std::cerr << "Input:    " << std::string(input, input_length) << "\n";
    std::cerr << "Radix:    " << radix << "\n";
    std::cerr << "Expected: " << FormatHex(expected) << "\n";
    std::cerr << "Actual:   " << FormatHex(actual) << "\n";
    error_ = true;
  }

  int RunTest() {
    int count = 0;
#define CASE(Name, _)                                   \
  if (test_ == k##Name) {                               \
    for (int i = 0; i < runs_; i++) Test##Name(&count); \
  } else
    TESTS(CASE)
#undef CASE
    {
      DCHECK(false);  // Unreachable.
    }
    if (error_) return 1;
    std::cout << count << " tests run, no error reported.\n";
    return 0;
  }

  void TestKaratsuba(int* count) {
    // Calling {MultiplyKaratsuba} directly is only valid if
    // left_size >= right_size and right_size >= kKaratsubaThreshold.
    constexpr uint32_t kMin = config::kKaratsubaThreshold;
    constexpr uint32_t kMax = 3 * config::kKaratsubaThreshold;
    for (uint32_t right_size = kMin; right_size <= kMax; right_size++) {
      for (uint32_t left_size = right_size; left_size <= kMax; left_size++) {
        ScratchDigits A(left_size, platform_);
        ScratchDigits B(right_size, platform_);
        uint32_t result_len = MultiplyResultLength(A, B);
        ScratchDigits result(result_len, platform_);
        ScratchDigits result_schoolbook(result_len, platform_);
        GenerateRandom(A);
        GenerateRandom(B);
        processor()->MultiplyKaratsuba(result, A, B);
        MultiplySchoolbook(result_schoolbook, A, B);
        AssertEquals(A, B, result_schoolbook, result);
        if (error_) return;
        (*count)++;
      }
    }
  }

  // Running Toom3 with less than 3 digits makes no sense.
  static constexpr uint32_t kMinToomDigits = 3;

  void TestToom(int* count) {
#if V8_ADVANCED_BIGINT_ALGORITHMS
    // {MultiplyToomCook} works fine even below the threshold, so we can
    // save some time by starting small.
    static_assert(config::kToomThreshold > 60 + kMinToomDigits);
    constexpr uint32_t kMin = config::kToomThreshold - 60;
    constexpr uint32_t kMax = config::kToomThreshold + 10;
    for (uint32_t right_size = kMin; right_size <= kMax; right_size++) {
      for (uint32_t left_size = right_size; left_size <= kMax; left_size++) {
        ScratchDigits A(left_size, platform_);
        ScratchDigits B(right_size, platform_);
        uint32_t result_len = MultiplyResultLength(A, B);
        ScratchDigits result(result_len, platform_);
        ScratchDigits result_karatsuba(result_len, platform_);
        GenerateRandom(A);
        GenerateRandom(B);
        processor()->MultiplyToomCook(result, A, B);
        // Using Karatsuba as reference.
        processor()->MultiplyKaratsuba(result_karatsuba, A, B);
        AssertEquals(A, B, result_karatsuba, result);
        if (error_) return;
        (*count)++;
      }
    }
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
  }

  void TestFFT(int* count) {
#if V8_ADVANCED_BIGINT_ALGORITHMS
    // Larger multiplications are slower, so to keep individual runs fast,
    // we test a few random samples. With build bots running 24/7, we'll
    // get decent coverage over time.
    uint64_t random_bits = rng_.NextUint64();
    static_assert(config::kFftThreshold > 511 + kMinToomDigits);
    uint32_t min =
        config::kFftThreshold - static_cast<uint32_t>(random_bits & 511);
    random_bits >>= 10;
    uint32_t max =
        config::kFftThreshold + static_cast<uint32_t>(random_bits & 1023);
    random_bits >>= 10;
    // If delta is too small, then this run gets too slow. If it happened
    // to be zero, we'd even loop forever!
    uint32_t delta = 10 + (random_bits & 127);
    std::cout << "min " << min << " max " << max << " delta " << delta << "\n";
    for (uint32_t right_size = min; right_size <= max; right_size += delta) {
      for (uint32_t left_size = right_size; left_size <= max;
           left_size += delta) {
        ScratchDigits A(left_size, platform_);
        ScratchDigits B(right_size, platform_);
        uint32_t result_len = MultiplyResultLength(A, B);
        ScratchDigits result(result_len, platform_);
        ScratchDigits result_toom(result_len, platform_);
        GenerateRandom(A);
        GenerateRandom(B);
        processor()->MultiplyFFT(result, A, B);
        // Using Toom-Cook as reference.
        processor()->MultiplyToomCook(result_toom, A, B);
        AssertEquals(A, B, result_toom, result);
        if (error_) return;
        (*count)++;
      }
    }
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
  }

  void TestBurnikel(int* count) {
    // Start small to save test execution time.
    constexpr uint32_t kMin = config::kBurnikelThreshold / 2;
    constexpr uint32_t kMax = 2 * config::kBurnikelThreshold;
    for (uint32_t right_size = kMin; right_size <= kMax; right_size++) {
      for (uint32_t left_size = right_size; left_size <= kMax; left_size++) {
        ScratchDigits A(left_size, platform_);
        ScratchDigits B(right_size, platform_);
        GenerateRandom(A);
        GenerateRandom(B);
        uint32_t quotient_len = DivideResultLength(A, B);
        uint32_t remainder_len = right_size;
        ScratchDigits quotient(quotient_len, platform_);
        ScratchDigits quotient_schoolbook(quotient_len, platform_);
        ScratchDigits remainder(remainder_len, platform_);
        ScratchDigits remainder_schoolbook(remainder_len, platform_);
        processor()->DivideBurnikelZiegler(quotient, remainder, A, B);
        processor()->DivideSchoolbook(quotient_schoolbook, remainder_schoolbook,
                                      A, B);
        AssertEquals(A, B, quotient_schoolbook, quotient);
        AssertEquals(A, B, remainder_schoolbook, remainder);
        if (error_) return;
        (*count)++;
      }
    }
  }

  void TestCachedMod(int* count) {
    // We could support b_len == 1, but it's not relevant in practice and
    // the implementation would have to special-case it.
    for (uint32_t b_len = 2; b_len <= 20; b_len++) {
      ScratchDigits B(b_len, platform_);
      ScratchDigits R(b_len, platform_);
      ScratchDigits R_exp(b_len, platform_);
      GenerateRandom(B);
      processor()->CachedMod_MakeInverse(B);
      // The length of the cached inverse determines the maximum {a_len} we
      // can handle.
      for (uint32_t a_len = b_len; a_len <= 2 * b_len; a_len++) {
        ScratchDigits A(a_len, platform_);
        for (uint32_t j = 0; j < 200; j++) {
          GenerateRandom(A);
          processor()->CachedMod(R, A);

          auto [done, top] = ModuloSmall(R_exp, A, B);
          if (!done) {
            processor()->ModuloLarge(R_exp, A, B);
          }

          AssertEquals(A, B, R_exp, R);
          if (error_) return;
          (*count)++;
        }
      }
    }
  }

#if V8_ADVANCED_BIGINT_ALGORITHMS
  void TestBarrett_Internal(uint32_t left_size, uint32_t right_size) {
    ScratchDigits A(left_size, platform_);
    ScratchDigits B(right_size, platform_);
    GenerateRandom(A);
    GenerateRandom(B);
    uint32_t quotient_len = DivideResultLength(A, B);
    // {DivideResultLength} doesn't expect to be called for sizes below
    // {kBarrettThreshold} (which we do here to save time), so we have to
    // manually adjust the allocated result length.
    if (B.len() < config::kBarrettThreshold) quotient_len++;
    uint32_t remainder_len = right_size;
    ScratchDigits quotient(quotient_len, platform_);
    ScratchDigits quotient_burnikel(quotient_len, platform_);
    ScratchDigits remainder(remainder_len, platform_);
    ScratchDigits remainder_burnikel(remainder_len, platform_);
    processor()->DivideBarrett(quotient, remainder, A, B);
    processor()->DivideBurnikelZiegler(quotient_burnikel, remainder_burnikel, A,
                                       B);
    AssertEquals(A, B, quotient_burnikel, quotient);
    AssertEquals(A, B, remainder_burnikel, remainder);
  }

  void TestBarrett(int* count) {
    // We pick a range around kBurnikelThreshold (instead of kBarrettThreshold)
    // to save test execution time.
    constexpr uint32_t kMin = config::kBurnikelThreshold / 2;
    constexpr uint32_t kMax = 2 * config::kBurnikelThreshold;
    // {DivideBarrett(A, B)} requires that A.len > B.len!
    for (uint32_t right_size = kMin; right_size <= kMax; right_size++) {
      for (uint32_t left_size = right_size + 1; left_size <= kMax;
           left_size++) {
        TestBarrett_Internal(left_size, right_size);
        if (error_) return;
        (*count)++;
      }
    }
    // We also test one random large case.
    uint64_t random_bits = rng_.NextUint64();
    uint32_t right_size =
        config::kBarrettThreshold + static_cast<uint32_t>(random_bits & 0x3FF);
    random_bits >>= 10;
    uint32_t left_size =
        right_size + 1 + static_cast<uint32_t>(random_bits & 0x3FFF);
    random_bits >>= 14;
    TestBarrett_Internal(left_size, right_size);
    if (error_) return;
    (*count)++;
  }
#else
  void TestBarrett(int* count) {}
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS

  void TestToString(int* count) {
    constexpr uint32_t kMin = config::kToStringFastThreshold / 2;
    constexpr uint32_t kMax = config::kToStringFastThreshold * 2;
    for (uint32_t size = kMin; size < kMax; size++) {
      ScratchDigits X(size, platform_);
      GenerateRandom(X);
      for (int radix = 2; radix <= 36; radix++) {
        uint32_t chars_required = ToStringResultLength(X, radix, false);
        uint32_t result_len = chars_required;
        uint32_t reference_len = chars_required;
        std::unique_ptr<char[]> result(new char[result_len]);
        std::unique_ptr<char[]> reference(new char[reference_len]);
        processor()->ToStringImpl(result.get(), &result_len, X, radix, false,
                                  true);
        processor()->ToStringImpl(reference.get(), &reference_len, X, radix,
                                  false, false);
        AssertEquals(X, radix, reference.get(), reference_len, result.get(),
                     result_len);
        if (error_) return;
        (*count)++;
      }
    }
  }

  void TestFromString(int* count) {
    constexpr uint32_t kMaxDigits = 1 << 20;  // Any large-enough value will do.
    constexpr uint32_t kMin = config::kFromStringLargeThreshold / 2;
    constexpr uint32_t kMax = config::kFromStringLargeThreshold * 2;
    for (uint32_t size = kMin; size < kMax; size++) {
      // To keep test execution times low, test one random radix every time.
      // Generally, radixes 2 through 36 (inclusive) are supported; however
      // the functions {FromStringLarge} and {FromStringClassic} can't deal
      // with the data format that {Parse} creates for power-of-two radixes,
      // so we skip power-of-two radixes here (and test them separately below).
      // We round up the number of radixes in the list to 32 by padding with
      // 10, giving decimal numbers extra test coverage, and making it easy
      // to evenly map a random number into the index space.
      constexpr uint8_t radixes[] = {3,  5,  6,  7,  9,  10, 11, 12, 13, 14, 15,
                                     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
                                     28, 29, 30, 31, 33, 34, 35, 36, 10, 10};
      int radix_index = (rng_.NextUint64() & 31);
      int radix = radixes[radix_index];
      int num_chars = std::round(size * kDigitBits / std::log2(radix));
      std::unique_ptr<char[]> chars(new char[num_chars]);
      GenerateRandomString(chars.get(), num_chars, radix);
      FromStringAccumulator accumulator(kMaxDigits, platform_);
      FromStringAccumulator ref_accumulator(kMaxDigits, platform_);
      const char* start = chars.get();
      const char* end = chars.get() + num_chars;
      accumulator.Parse(start, end, radix);
      ref_accumulator.Parse(start, end, radix);
      ScratchDigits result(accumulator.ResultLength(), platform_);
      ScratchDigits reference(ref_accumulator.ResultLength(), platform_);
      processor()->FromStringLarge(result, &accumulator);
      processor()->FromStringClassic(reference, &ref_accumulator);
      AssertEquals(start, num_chars, radix, result, reference);
      if (error_) return;
      (*count)++;
    }
  }

  void TestFromStringBase2(int* count) {
    constexpr uint32_t kMaxDigits = 1 << 20;  // Any large-enough value will do.
    constexpr uint32_t kMin = 1;
    constexpr uint32_t kMax = 100;
    for (uint32_t size = kMin; size < kMax; size++) {
      ScratchDigits X(size, platform_);
      GenerateRandom(X);
      for (int bits = 1; bits <= 5; bits++) {
        uint32_t radix = 1 << bits;
        uint32_t chars_required = ToStringResultLength(X, radix, false);
        uint32_t string_len = chars_required;
        std::unique_ptr<char[]> chars(new char[string_len]);
        processor()->ToStringImpl(chars.get(), &string_len, X, radix, false,
                                  true);
        // Fill any remaining allocated characters with garbage to test that
        // too.
        for (uint32_t i = string_len; i < chars_required; i++) {
          chars[i] = '?';
        }
        const char* start = chars.get();
        const char* end = start + chars_required;
        FromStringAccumulator accumulator(kMaxDigits, platform_);
        accumulator.Parse(start, end, radix);
        ScratchDigits result(accumulator.ResultLength(), platform_);
        processor()->FromString(result, &accumulator);
        AssertEquals(start, chars_required, radix, X, result);
        if (error_) return;
        (*count)++;
      }
    }
  }

  void RunThresholds() {
    std::cout << "Thresholds testing mode selected.\n"
                 "The plotted vertical line represents the performance of the "
                 "respective\n"
                 "\"smaller\" algorithm; the stars represent the relative "
                 "performance of the\n"
                 "\"larger\" algorithm. Less is better. So as threshold you "
                 "want to pick the\n"
                 "size where the starry line crosses the vertical line towards "
                 "the left.\n\n";

    ThresholdRunner runner(processor(), rng_);
    runner.Measure(ThresholdRunner::kKaratsuba);
    runner.Measure(ThresholdRunner::kBurnikel);
    runner.Measure(ThresholdRunner::kToString);
    runner.Measure(ThresholdRunner::kFromString);
#if V8_ADVANCED_BIGINT_ALGORITHMS
    runner.Measure(ThresholdRunner::kToom);
    runner.Measure(ThresholdRunner::kFFT);
    runner.Measure(ThresholdRunner::kNewton);
    runner.Measure(ThresholdRunner::kBarrett);
#endif  // V8_ADVANCED_BIGINT_ALGORITHMS
  }

  template <typename I>
  bool ParseInt(char* s, I* out) {
    char* end;
    if (s[0] == '\0') return false;
    errno = 0;
    long l = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || l > std::numeric_limits<I>::max() ||
        l < std::numeric_limits<I>::min()) {
      return false;
    }
    *out = static_cast<I>(l);
    return true;
  }

  int ParseOptions(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--list") == 0) {
        op_ = kList;
      } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        PrintHelp(argv);
        return 0;
      } else if (strcmp(argv[i], "--random-seed") == 0 ||
                 strcmp(argv[i], "--random_seed") == 0) {
        if (++i == argc || !ParseInt(argv[i], &random_seed_)) {
          return PrintHelp(argv);
        }
      } else if (strncmp(argv[i], "--random-seed=", 14) == 0 ||
                 strncmp(argv[i], "--random_seed=", 14) == 0) {
        if (!ParseInt(argv[i] + 14, &random_seed_)) return PrintHelp(argv);
      } else if (strcmp(argv[i], "--runs") == 0) {
        if (++i == argc || !ParseInt(argv[i], &runs_)) return PrintHelp(argv);
      } else if (strncmp(argv[i], "--runs=", 7) == 0) {
        if (!ParseInt(argv[i] + 7, &runs_)) return PrintHelp(argv);
      } else if (strncmp(argv[i], "--thresholds", 12) == 0) {
        op_ = kThresholds;
      }
#define TEST(Name, name)                 \
  else if (strcmp(argv[i], name) == 0) { \
    op_ = kTest;                         \
    test_ = k##Name;                     \
  }
      TESTS(TEST)
#undef TEST
      else {
        std::cerr << "Warning: ignored argument: " << argv[i] << "\n";
      }
    }
    if (op_ == kNoOp) return PrintHelp(argv);  // op is mandatory.
    return 0;
  }

 private:
  void GenerateRandom(RWDigits X) { test::GenerateRandom(rng_, X); }

  void GenerateRandomString(char* str, int len, int radix) {
    DCHECK(2 <= radix && radix <= 36);
    if (len == 0) return;
    uint64_t random;
    int available_bits = 0;
    const int char_bits = BitLength(radix - 1);
    const uint64_t char_mask = (1u << char_bits) - 1u;
    for (int i = 0; i < len; i++) {
      while (true) {
        if (available_bits < char_bits) {
          random = rng_.NextUint64();
          available_bits = 64;
        }
        int next_char = static_cast<int>(random & char_mask);
        random = random >> char_bits;
        available_bits -= char_bits;
        if (next_char >= radix) continue;
        *str = kConversionChars[next_char];
        str++;
        break;
      };
    }
  }

  Operation op_{kNoOp};
  Test test_;
  bool error_{false};
  int runs_ = 1;
  int64_t random_seed_{314159265359};
  RNG rng_;
  std::unique_ptr<Processor, Processor::Destroyer> processor_;
  Platform* platform_;
};

}  // namespace test
}  // namespace bigint
}  // namespace v8

int main(int argc, char** argv) {
  v8::bigint::test::Runner runner;
  int ret = runner.ParseOptions(argc, argv);
  if (ret != 0) return ret;
  runner.Initialize();
  return runner.Run();
}
