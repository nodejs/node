#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node.h>
#include <node_blob.h>
#include <node_bob.h>
#include <node_errors.h>
#include <node_mem.h>
#include <node_mutex.h>
#include <node_sockaddr.h>
#include <stream_base.h>
#include <string_bytes.h>
#include <v8.h>
#include <limits>
#include <string>
#include <unordered_map>
#include "defs.h"

namespace node {
namespace quic {

class BindingState;
class CryptoContext;
class Endpoint;
class Session;
class Stream;

using QuicMemoryManager = mem::NgLibMemoryManager<BindingState, ngtcp2_mem>;

// =============================================================================
// The BindingState object holds state for the internalBinding('quic') binding
// instance. It is mostly used to hold the persistent constructors, strings, and
// callback references used for the rest of the implementation.
class BindingState final : public BaseObject, public QuicMemoryManager {
 public:
  static constexpr FastStringKey type_name{"quic"};

  static bool Initialize(Environment* env, v8::Local<v8::Object> target);
  static BindingState& Get(Environment* env);

  operator ngtcp2_mem() const;
  operator nghttp3_mem() const;

  BindingState(Environment* env, v8::Local<v8::Object> object);
  QUIC_NO_COPY_OR_MOVE(BindingState)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BindingState);
  SET_SELF_SIZE(BindingState);

  // NgLibMemoryManager (QuicMemoryManager)
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

  // A set of listening Endpoints. We maintain this to ensure that the Endpoint
  // cannot be gc'd while it is still listening and there are active
  // connections.
  std::unordered_map<Endpoint*, BaseObjectPtr<Endpoint>> listening_endpoints_;

  bool warn_trace_tls = true;
  bool initialized = false;

#define V(name)                                                                \
  void set_##name##_constructor_template(                                      \
      v8::Local<v8::FunctionTemplate> tmpl);                                   \
  v8::Local<v8::FunctionTemplate> name##_constructor_template() const;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _)                                                             \
  void set_##name##_callback(v8::Local<v8::Function> fn);                      \
  v8::Local<v8::Function> name##_callback() const;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) v8::Local<v8::String> name##_string() const;
  QUIC_STRINGS(V)
#undef V

 private:
  size_t current_ngtcp2_memory_ = 0;

#define V(name) v8::Global<v8::FunctionTemplate> name##_constructor_template_;
  QUIC_CONSTRUCTORS(V)
#undef V

#define V(name, _) v8::Global<v8::Function> name##_callback_;
  QUIC_JS_CALLBACKS(V)
#undef V

#define V(name, _) mutable v8::Eternal<v8::String> name##_string_;
  QUIC_STRINGS(V)
#undef V
};

// =============================================================================
// CIDs are used to identify endpoints participating in a QUIC session

class CIDFactory;

class CID final : public MemoryRetainer {
 public:
  inline CID() : ptr_(&cid_) {}

  // Copies the given cid.
  CID(const uint8_t* cid, size_t len);

  CID(const CID& other);
  CID& operator=(const CID& other);

  // Wraps the given ngtcp2_cid
  inline explicit CID(const ngtcp2_cid* cid) : ptr_(cid) {}

  // Copies the given cid.
  inline explicit CID(const ngtcp2_cid& cid) : CID(cid.data, cid.datalen) {}

  struct Hash final {
    inline size_t operator()(const CID& cid) const {
      size_t hash = 0;
      for (size_t n = 0; n < cid->datalen; n++) {
        hash ^= std::hash<uint8_t>{}(cid->data[n]) + 0x9e3779b9 + (hash << 6) +
                (hash >> 2);
      }
      return hash;
    }
  };

  inline bool operator==(const CID& other) const noexcept {
    return memcmp(ptr_->data, other.ptr_->data, ptr_->datalen) == 0;
  }

  inline bool operator!=(const CID& other) const noexcept {
    return !(*this == other);
  }

  inline const ngtcp2_cid& operator*() const { return *ptr_; }
  inline const ngtcp2_cid* operator->() const { return ptr_; }

  inline operator ngtcp2_cid*() { return &cid_; }
  inline operator const ngtcp2_cid*() const { return ptr_; }
  inline operator const uint8_t*() const { return ptr_->data; }

  std::string ToString() const;

  inline operator bool() const { return ptr_->datalen > 0; }
  inline size_t length() const { return ptr_->datalen; }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(CID)
  SET_SELF_SIZE(CID)

  template <typename T>
  using Map = std::unordered_map<CID, T, CID::Hash>;

 private:
  inline void set_length(size_t length) {
    CHECK_EQ(ptr_, &cid_);
    cid_.datalen = length;
  }

  ngtcp2_cid cid_;
  const ngtcp2_cid* ptr_ = nullptr;

  friend class CIDFactory;
};

// A CIDFactory, as the name suggests, is used to create new CIDs.
// Per https://datatracker.ietf.org/doc/draft-ietf-quic-load-balancers/, QUIC
// implementations may use the Connection IDs associated with a QUIC session as
// a routing mechanism, with each CID instance securely encoding the routing
// information. By default, our implementation creates CIDs randomly but allows
// user code to provide their own CIDFactory implementation.
class CIDFactory {
 public:
  virtual ~CIDFactory() = default;

  virtual void Generate(ngtcp2_cid* cid,
                        size_t length_hint = NGTCP2_MAX_CIDLEN) = 0;

  void Generate(CID* cid, size_t length_hint = NGTCP2_MAX_CIDLEN);

  CID Generate(size_t length_hint = NGTCP2_MAX_CIDLEN);

  virtual BaseObjectPtr<BaseObject> StrongRef() { return {}; }

  virtual void UpdateCIDState(const CID& cid) {
    // By default, do nothing.
  }

  class RandomCIDFactory;
  class Base;

  static CIDFactory& random();
};

// The default random CIDFactory implementation.
class CIDFactory::RandomCIDFactory final : public CIDFactory {
 public:
  RandomCIDFactory() = default;
  QUIC_NO_COPY_OR_MOVE(RandomCIDFactory)
  void Generate(ngtcp2_cid* cid,
                size_t length_hint = NGTCP2_MAX_CIDLEN) override;
};

// A virtual base class for CIDFactory implementations implemented as Base
// objects.
class CIDFactory::Base : public CIDFactory, public BaseObject {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  Base(Environment* env, v8::Local<v8::Object> object);
  BaseObjectPtr<BaseObject> StrongRef() override;
};

// =============================================================================

using PacketReq = ReqWrap<uv_udp_send_t>;

// A Packet encapsulates serialized outbound QUIC data.
class Packet final : public PacketReq {
 private:
  struct Data;

 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static BaseObjectPtr<Packet> Create(
      Environment* env,
      Endpoint* endpoint,
      const SocketAddress& remote_address,
      const char* diagnostic_label = "<unknown>",
      size_t length = kDefaultMaxPacketLength);

  inline const SocketAddress& destination() const { return destination_; }
  inline bool is_pending() const { return !!handle_; }

  operator uv_buf_t() const;
  operator ngtcp2_vec() const;
  void Truncate(size_t len);
  inline size_t length() const { return data_ ? data_->len_ : 0; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Endpoint::Packet)
  SET_SELF_SIZE(Packet)

  std::string ToString() const;

  BaseObjectPtr<Packet> Clone() const;

  using Queue = std::deque<BaseObjectPtr<Packet>>;

  // Really should be private but MakeBaseObject needs to be able to see it.
  // Use Create() to create instances.
  Packet(Environment* env,
         Endpoint* endpoint,
         v8::Local<v8::Object> object,
         const SocketAddress& destination,
         size_t length,
         const char* diagnostic_label = "<unknown>");

  // Really should be private but MakeBaseObject needs to be able to see it.
  // Use Create() to create instances.
  Packet(Environment* env,
         Endpoint* endpoint,
         v8::Local<v8::Object> object,
         const SocketAddress& destination,
         std::shared_ptr<Data> data);

 private:
  struct Data final : public MemoryRetainer {
    uint8_t data_[kDefaultMaxPacketLength];
    uint8_t* ptr_ = data_;
    size_t len_ = kDefaultMaxPacketLength;
    std::string diagnostic_label_;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Data)
    SET_SELF_SIZE(Data)

    Data(size_t length, const char* diagnostic_label);
    ~Data() override;
  };

  inline void Attach(BaseObjectPtr<BaseObject> handle) {
    handle_ = std::move(handle);
  }
  void Done(int status);

  Endpoint* endpoint_;
  SocketAddress destination_;
  std::shared_ptr<Data> data_;

  BaseObjectPtr<BaseObject> handle_;

  friend class Endpoint;
  friend class UDP;
};

// =============================================================================
class QuicError final : public MemoryRetainer {
 public:
  enum class Type : int {
    TRANSPORT = NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT,
    APPLICATION = NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION,
    VERSION_NEGOTIATION =
        NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT_VERSION_NEGOTIATION,
    IDLE_CLOSE = NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT_IDLE_CLOSE,
  };

  inline QuicError(const std::string& reason = std::string())
      : reason_(reason), ptr_(&error_) {
    ngtcp2_connection_close_error_default(&error_);
  }

  inline QuicError(const ngtcp2_connection_close_error* ptr)
      : reason_(reinterpret_cast<const char*>(ptr->reason), ptr->reasonlen),
        ptr_(ptr) {}

  inline QuicError(const ngtcp2_connection_close_error& error)
      : reason_(reinterpret_cast<const char*>(error.reason), error.reasonlen),
        error_(error) {}

  inline Type type() const { return static_cast<Type>(ptr_->type); }
  inline error_code code() const { return ptr_->error_code; }
  inline const std::string& reason() const { return reason_; }
  inline uint64_t frameType() const { return ptr_->frame_type; }

  inline const ngtcp2_connection_close_error& operator*() const {
    return *ptr_;
  }
  inline const ngtcp2_connection_close_error* operator->() const {
    return ptr_;
  }
  inline const ngtcp2_connection_close_error* get() const { return ptr_; }

  operator const ngtcp2_connection_close_error*() const { return ptr_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicError)
  SET_SELF_SIZE(QuicError)

  static QuicError ForTransport(error_code code,
                                const std::string& reason = std::string());
  static QuicError ForApplication(error_code code,
                                  const std::string& reason = std::string());
  static QuicError ForVersionNegotiation(
      const std::string& reason = std::string());
  static QuicError ForIdleClose(const std::string& reason = std::string());
  static QuicError ForNgtcp2Error(int code,
                                  const std::string& reason = std::string());
  static QuicError ForTlsAlert(int code,
                               const std::string& reason = std::string());

  static QuicError From(Session* session);

  std::string ToString() const;

 private:
  const uint8_t* reason_c_str() const {
    return reinterpret_cast<const uint8_t*>(reason_.c_str());
  }

  std::string reason_;
  ngtcp2_connection_close_error error_;
  const ngtcp2_connection_close_error* ptr_ = nullptr;
};

// =============================================================================
// StatsBase is a base utility helper for classes (like Endpoint, Session, and
// Stream) that want to record statistics.
template <typename Traits>
class StatsBase {
 public:
  using Stats = typename Traits::Stats;

  inline explicit StatsBase(Environment* env, bool shared = false)
      : shared_(shared),
        stats_store_(
            v8::ArrayBuffer::NewBackingStore(env->isolate(), sizeof(Stats))),
        stats_(new(stats_store_->Data()) Stats) {
    DCHECK_NOT_NULL(stats_);
    stats_->created_at = uv_hrtime();
  }

  QUIC_NO_COPY_OR_MOVE(StatsBase)

  virtual ~StatsBase() = default;

  inline v8::Local<v8::BigUint64Array> ToBigUint64Array(Environment* env) {
    size_t size = sizeof(Stats);
    size_t count = size / sizeof(uint64_t);
    v8::Local<v8::ArrayBuffer> stats_buffer =
        v8::ArrayBuffer::New(env->isolate(), stats_store_);
    return v8::BigUint64Array::New(stats_buffer, 0, count);
  }

  struct StatsDebug final {
    using Base = typename Traits::Base;
    Base& ptr;
    inline explicit StatsDebug(Base& ptr_) : ptr(ptr_) {}
    inline std::string ToString() const {
      const auto gen = [&] {
        std::string out = "Statistics:\n";
        auto add_field = [&out](const char* name, uint64_t val) {
          out += "  ";
          out += std::string(name);
          out += ": ";
          out += std::to_string(val);
          out += "\n";
        };
        add_field("Duration", uv_hrtime() - ptr.GetStat(&Stats::created_at));
        Traits::ToString(ptr.stats(), add_field);
        return out;
      };
      if (ptr.shared_) {
        RwLock::ScopedLock lock(ptr->mutex_);
        return gen();
      }
      return gen();
    }
  };

  StatsDebug debug() const { return StatsDebug(*this); }

  inline const Stats& stats() const { return *stats_; }

  inline void StatsMemoryInfo(MemoryTracker* tracker) const {
    tracker->TrackField("stats_store", stats_store_);
  }

 protected:
  // Increments the given stat field by the given amount or 1 if no amount is
  // specified.
  inline void IncrementStat(uint64_t Stats::*member, uint64_t amount = 1) {
    if (shared_) {
      RwLock::ScopedLock lock(rwlock_);
      stats_->*member += std::min(amount, kMaxUint64 - stats_->*member);
    } else {
      stats_->*member += std::min(amount, kMaxUint64 - stats_->*member);
    }
  }

  // Sets an entirely new value for the given stat field
  inline void SetStat(uint64_t Stats::*member, uint64_t value) {
    if (shared_) {
      RwLock::ScopedLock lock(rwlock_);
      stats_->*member = value;
    } else {
      stats_->*member = value;
    }
  }

  // Sets the given stat field to the current uv_hrtime()
  inline void RecordTimestamp(uint64_t Stats::*member) {
    if (shared_) {
      RwLock::ScopedLock lock(rwlock_);
      stats_->*member = uv_hrtime();
    } else {
      stats_->*member = uv_hrtime();
    }
  }

  // Gets the current value of the given stat field
  inline uint64_t GetStat(uint64_t Stats::*member) const {
    return stats_->*member;
  }

 private:
  const bool shared_ = false;
  std::shared_ptr<v8::BackingStore> stats_store_;
  Stats* stats_ = nullptr;

  RwLock rwlock_;
};

void IllegalConstructor(const v8::FunctionCallbackInfo<v8::Value>& args);

// =============================================================================
// Store
class Store final : public MemoryRetainer {
 public:
  Store() = default;

  explicit Store(std::shared_ptr<v8::BackingStore> store,
                 size_t length,
                 size_t offset = 0);

  explicit Store(std::unique_ptr<v8::BackingStore> store,
                 size_t length,
                 size_t offset = 0);

  explicit Store(v8::Local<v8::ArrayBuffer> buffer);

  explicit Store(v8::Local<v8::ArrayBufferView> view);

  operator uv_buf_t() const;

  operator ngtcp2_vec() const;

  inline operator bool() const { return store_ != nullptr; }

  size_t length() const { return length_; }

  template <typename View>
  v8::Local<View> ToArrayBufferView(Environment* env) {
    if (!store_) {
      return View::New(v8::ArrayBuffer::New(env->isolate(), 0), 0, 0);
    }
    auto buffer = v8::ArrayBuffer::New(env->isolate(), store_);
    return View::New(buffer, offset_, length_);
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Store)
  SET_SELF_SIZE(Store)

 private:
  std::shared_ptr<v8::BackingStore> store_;
  size_t offset_ = 0;
  size_t length_ = 0;
};

// =============================================================================

template <typename T>
struct CallbackScopeBase final {
  BaseObjectPtr<T> ref;
  v8::Context::Scope context_scope;
  v8::TryCatch try_catch;

  explicit CallbackScopeBase(T* ptr)
      : ref(ptr),
        context_scope(ptr->env()->context()),
        try_catch(ptr->env()->isolate()) {}

  ~CallbackScopeBase() {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      errors::TriggerUncaughtException(ref->env()->isolate(), try_catch);
    }
  }
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
