#ifndef SRC_QUIC_STATS_H_
#define SRC_QUIC_STATS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker.h"
#include "util.h"
#include <v8.h>
#include <uv.h>

#include <limits>
#include <string>

namespace node {
namespace quic {

static constexpr uint64_t kMaxUint64 = std::numeric_limits<int64_t>::max();

template <typename T> class StatsBase;

template <typename T, typename Q>
struct StatsTraits {
  using Stats = T;
  using Base = Q;

  template <typename Fn>
  static void ToString(const Q& ptr, Fn&& add_field) {
  }
};

using AddStatsField = std::function<void(const char*, uint64_t)>;

// StatsBase is a utility help for classes (like Session)
// that record performance statistics. The template takes a
// single Traits argument (see StreamStatsTraits in
// stream.h as an example). When the StatsBase
// is deconstructed, collected statistics are output to
// Debug automatically.
template <typename T>
class StatsBase {
 public:
  typedef typename T::Stats Stats;

  inline explicit StatsBase(Environment* env)
      : stats_store_(
            v8::ArrayBuffer::NewBackingStore(
                env->isolate(),
                sizeof(Stats))),
        stats_(new (stats_store_->Data()) Stats) {
    DCHECK_NOT_NULL(stats_);
    stats_->created_at = uv_hrtime();
  }

  inline v8::Local<v8::BigUint64Array> ToBigUint64Array(Environment* env) {
    size_t size = sizeof(Stats);
    size_t count = size / sizeof(uint64_t);
    v8::Local<v8::ArrayBuffer> stats_buffer =
        v8::ArrayBuffer::New(env->isolate(), stats_store_);
    return v8::BigUint64Array::New(stats_buffer, 0, count);
  }

  virtual ~StatsBase() {
    if (stats_ != nullptr) stats_->~Stats();
  }

  // The StatsDebug utility is used when StatsBase is destroyed
  // to output statistical information to Debug. It is designed
  // to only incur a performance cost constructing the debug
  // output when Debug output is enabled.
  struct StatsDebug {
    typename T::Base* ptr;
    inline explicit StatsDebug(typename T::Base* ptr_) : ptr(ptr_) {}
    inline std::string ToString() const {
      std::string out = "Statistics:\n";
      auto add_field = [&out](const char* name, uint64_t val) {
        out += "  ";
        out += std::string(name);
        out += ": ";
        out += std::to_string(val);
        out += "\n";
      };
      add_field("Duration", uv_hrtime() - ptr->GetStat(&Stats::created_at));
      T::ToString(*ptr, add_field);
      return out;
    }
  };

  // Increments the given stat field by the given amount or 1 if
  // no amount is specified.
  inline void IncrementStat(uint64_t Stats::*member, uint64_t amount = 1) {
    Mutex::ScopedLock lock(mutex_);
    stats_->*member += std::min(amount, kMaxUint64 - stats_->*member);
  }

  // Sets an entirely new value for the given stat field
  inline void SetStat(uint64_t Stats::*member, uint64_t value) {
    Mutex::ScopedLock lock(mutex_);
    stats_->*member = value;
  }

  // Sets the given stat field to the current uv_hrtime()
  inline void RecordTimestamp(uint64_t Stats::*member) {
    Mutex::ScopedLock lock(mutex_);
    stats_->*member = uv_hrtime();
  }

  // Gets the current value of the given stat field
  inline uint64_t GetStat(uint64_t Stats::*member) const {
    return stats_->*member;
  }

  inline void StatsMemoryInfo(MemoryTracker* tracker) const {
    tracker->TrackField("stats_store", stats_store_);
  }

  inline void DebugStats(AsyncWrap* wrap) {
    StatsDebug stats_debug(static_cast<typename T::Base*>(this));
    Debug(wrap, "Destroyed. %s", stats_debug);
  }

 private:
  std::shared_ptr<v8::BackingStore> stats_store_;
  Stats* stats_ = nullptr;

  Mutex mutex_;
};

template <typename S, typename B>
struct StatsTraitsImpl final {
  using Stats = S;
  using Base = B;

  static void ToString(const B& ptr, AddStatsField add_field);
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_STATS_H_
