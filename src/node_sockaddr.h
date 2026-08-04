#ifndef SRC_NODE_SOCKADDR_H_
#define SRC_NODE_SOCKADDR_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "env.h"
#include "memory_tracker.h"
#include "node.h"
#include "node_worker.h"
#include "uv.h"
#include "v8.h"

#include <compare>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

namespace node {

class Environment;

class SocketAddress : public MemoryRetainer {
 public:
  struct Hash {
    size_t operator()(const SocketAddress& addr) const;
  };

  // Hashes and compares only the IP address, ignoring the port.
  // Useful for per-host connection counting where clients from
  // the same IP but different ports should be treated as one host.
  struct IpHash {
    size_t operator()(const SocketAddress& addr) const;
  };
  struct IpEqual {
    bool operator()(const SocketAddress& a, const SocketAddress& b) const;
  };

  inline bool operator==(const SocketAddress& other) const;
  inline bool operator!=(const SocketAddress& other) const;

  inline std::partial_ordering operator<=>(const SocketAddress& other) const;

  inline static bool is_numeric_host(const char* hostname);
  inline static bool is_numeric_host(const char* hostname, int family);

  // Returns true if converting {family, host, port} to *addr succeeded.
  static bool ToSockAddr(int32_t family,
                         const char* host,
                         uint32_t port,
                         sockaddr_storage* addr);

  // Returns true if converting {family, host, port} to *addr succeeded.
  static bool New(int32_t family,
                  const char* host,
                  uint32_t port,
                  SocketAddress* addr);

  static bool New(const char* host, uint32_t port, SocketAddress* addr);

  // Returns the port for an IPv4 or IPv6 address.
  inline static int GetPort(const sockaddr* addr);
  inline static int GetPort(const sockaddr_storage* addr);

  // Returns the numeric host as a string for an IPv4 or IPv6 address.
  inline static std::string GetAddress(const sockaddr* addr);
  inline static std::string GetAddress(const sockaddr_storage* addr);

  // Returns the struct length for an IPv4, IPv6 or UNIX domain.
  inline static size_t GetLength(const sockaddr* addr);
  inline static size_t GetLength(const sockaddr_storage* addr);

  SocketAddress() = default;

  inline explicit SocketAddress(const sockaddr* addr);
  inline SocketAddress(const SocketAddress& addr);
  inline SocketAddress& operator=(const sockaddr* other);
  inline SocketAddress& operator=(const SocketAddress& other);

  inline const sockaddr& operator*() const;
  inline const sockaddr* operator->() const;

  inline const sockaddr* data() const;
  inline const uint8_t* raw() const;
  inline sockaddr* storage();
  inline size_t length() const;

  inline int family() const;
  inline std::string address() const;
  inline int port() const;

  // Returns true if the given other SocketAddress is a match
  // for this one. The addresses are a match if:
  // 1. They are the same family and match identically
  // 2. They are different family but match semantically (
  //     for instance, an IPv4 address in IPv6 notation)
  bool is_match(const SocketAddress& other) const;

  // Compares this SocketAddress to the given other SocketAddress.
  std::partial_ordering compare(const SocketAddress& other) const;

  // Returns true if this SocketAddress is within the subnet
  // identified by the given network address and CIDR prefix.
  bool is_in_network(const SocketAddress& network, int prefix) const;

  // If the SocketAddress is an IPv6 address, returns the
  // current value of the IPv6 flow label, if set. Otherwise
  // returns 0.
  inline uint32_t flow_label() const;

  // If the SocketAddress is an IPv6 address, sets the
  // current value of the IPv6 flow label. If not an
  // IPv6 address, set_flow_label is a non-op. It
  // is important to note that the flow label,
  // while represented as an uint32_t, the flow
  // label is strictly limited to 20 bits, and
  // this will assert if any value larger than
  // 20-bits is specified.
  inline void set_flow_label(uint32_t label = 0);

  inline void Update(uint8_t* data, size_t len);
  inline void Update(const sockaddr* data, size_t len);

  static SocketAddress FromSockName(const uv_udp_t& handle);
  static SocketAddress FromSockName(const uv_tcp_t& handle);
  static SocketAddress FromPeerName(const uv_udp_t& handle);
  static SocketAddress FromPeerName(const uv_tcp_t& handle);

  inline v8::MaybeLocal<v8::Object> ToJS(
      Environment* env,
      v8::Local<v8::Object> obj = v8::Local<v8::Object>()) const;

  inline std::string ToString() const;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SocketAddress)
  SET_SELF_SIZE(SocketAddress)

  template <typename T>
  using Map = std::unordered_map<SocketAddress, T, Hash>;

  template <typename T>
  using IpMap = std::unordered_map<SocketAddress, T, IpHash, IpEqual>;

 private:
  sockaddr_storage address_;
};

class SocketAddressBase : public BaseObject {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static BaseObjectPtr<SocketAddressBase> Create(
      Environment* env, std::shared_ptr<SocketAddress> address);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Detail(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LegacyDetail(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFlowLabel(const v8::FunctionCallbackInfo<v8::Value>& args);

  SocketAddressBase(Environment* env,
                    v8::Local<v8::Object> wrap,
                    std::shared_ptr<SocketAddress> address);

  inline const std::shared_ptr<SocketAddress>& address() const {
    return address_;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SocketAddressBase)
  SET_SELF_SIZE(SocketAddressBase)

  BaseObject::TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  class TransferData : public worker::TransferData {
   public:
    inline explicit TransferData(const SocketAddressBase* wrap)
        : address_(wrap->address_) {}

    inline explicit TransferData(std::shared_ptr<SocketAddress> address)
        : address_(std::move(address)) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(SocketAddressBase::TransferData)
    SET_SELF_SIZE(TransferData)

   private:
    std::shared_ptr<SocketAddress> address_;
  };

 private:
  std::shared_ptr<SocketAddress> address_;
};

template <typename T>
class SocketAddressLRU : public MemoryRetainer {
 public:
  using Type = typename T::Type;

  inline explicit SocketAddressLRU(size_t max_size);

  // If the item already exists, returns a reference to
  // the existing item, adjusting items position in the
  // LRU. If the item does not exist, emplaces the item
  // and returns the new item. The caller provides a
  // timestamp to avoid redundant uv_hrtime() calls.
  Type* Upsert(const SocketAddress& address, uint64_t now);

  // Returns a reference to the item if it exists, or
  // nullptr. The position in the LRU is not modified.
  Type* Peek(const SocketAddress& address) const;

  size_t size() const { return map_.size(); }
  size_t max_size() const { return max_size_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SocketAddressLRU)
  SET_SELF_SIZE(SocketAddressLRU)

 private:
  using Pair = std::pair<SocketAddress, Type>;
  using Iterator = typename std::list<Pair>::iterator;

  void CheckExpired(uint64_t now);

  std::list<Pair> list_;
  SocketAddress::Map<Iterator> map_;
  size_t max_size_;
};

// A BlockList is used to evaluate whether a given
// SocketAddress should be accepted for inbound or
// outbound network activity.
class SocketAddressBlockList : public MemoryRetainer {
 public:
  explicit SocketAddressBlockList(
      std::shared_ptr<SocketAddressBlockList> parent = {});
  ~SocketAddressBlockList() = default;

  void AddSocketAddress(const SocketAddress& address);

  void AddSocketAddresses(const SocketAddress* addresses, size_t count);

  void RemoveSocketAddress(const SocketAddress& address);

  void AddSocketAddressRange(const SocketAddress& start,
                             const SocketAddress& end);

  void RemoveSocketAddressRange(const SocketAddress& start,
                                const SocketAddress& end);

  void AddSocketAddressMask(const SocketAddress& address, int prefix);

  void RemoveSocketAddressMask(const SocketAddress& address, int prefix);

  bool Apply(const SocketAddress& address);

  void Clear();

  size_t size() const {
    return address_count_ + rules_.size() + subnet_rules_.size();
  }

  v8::MaybeLocal<v8::Array> ListRules(Environment* env);

  struct Rule : public MemoryRetainer {
    virtual bool Apply(const SocketAddress& address) = 0;
    inline v8::MaybeLocal<v8::Value> ToV8String(Environment* env);
    virtual std::string ToString() = 0;
  };

  struct SocketAddressRangeRule final : Rule {
    SocketAddress start;
    SocketAddress end;

    SocketAddressRangeRule(const SocketAddress& start,
                           const SocketAddress& end);

    bool Apply(const SocketAddress& address) override;
    std::string ToString() override;

    void MemoryInfo(node::MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(SocketAddressRangeRule)
    SET_SELF_SIZE(SocketAddressRangeRule)
  };

  struct SocketAddressMaskRule final : Rule {
    SocketAddress network;
    int prefix;

    SocketAddressMaskRule(const SocketAddress& address, int prefix);

    bool Apply(const SocketAddress& address) override;
    std::string ToString() override;

    void MemoryInfo(node::MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(SocketAddressMaskRule)
    SET_SELF_SIZE(SocketAddressMaskRule)
  };

  void MemoryInfo(node::MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SocketAddressBlockList)
  SET_SELF_SIZE(SocketAddressBlockList)

  // A compressed radix trie for O(prefix_length) subnet lookups.
  // Each node has two children (bit 0, bit 1). A node marked
  // terminal means all addresses matching the prefix up to that
  // depth are blocked. On insert, if a new prefix is shorter than
  // or equal to an existing one, the subtree is pruned (the shorter
  // prefix subsumes all longer ones). On lookup, we walk the bits
  // of the address and return true as soon as we hit a terminal node.
  class SubnetTrie {
   public:
    SubnetTrie() = default;
    ~SubnetTrie() = default;

    // Insert a subnet (network address bytes, prefix length in bits).
    // If a broader prefix already exists, the insert is a no-op.
    // If this prefix is broader than existing children, they are pruned.
    void Insert(const uint8_t* address_bytes, int prefix_length);

    // Returns true if the given address falls within any inserted subnet.
    bool Lookup(const uint8_t* address_bytes, int address_bits) const;

    // Remove all entries.
    void Clear();

    bool empty() const { return root_ == nullptr; }

    size_t size() const { return count_; }

   private:
    struct Node {
      std::unique_ptr<Node> children[2];
      bool terminal = false;
    };

    std::unique_ptr<Node> root_;
    size_t count_ = 0;
  };

 private:
  // Lock-free implementation used by both AddSocketAddress and
  // AddSocketAddresses. Caller must hold the write lock.
  void AddSocketAddressImpl(const SocketAddress& address);
  bool ListRules(Environment* env, v8::LocalVector<v8::Value>* vec);

  std::shared_ptr<SocketAddressBlockList> parent_;
  // Range rules only. Scanned linearly by Apply().
  std::list<std::unique_ptr<Rule>> rules_;
  // Exact address rules. Keyed by IP only (port-insensitive) so that
  // Apply() can perform O(1) lookups regardless of the port on the
  // checked address. Not included in rules_ to avoid redundant scanning.
  SocketAddress::IpMap<SocketAddress> address_rules_;
  // User-visible address count (not inflated by cross-family dual-insert).
  size_t address_count_ = 0;
  // Subnet/mask rules stored in radix tries for O(prefix_length) lookup.
  // Separate tries for IPv4 (max 32-bit depth) and IPv6 (max 128-bit).
  SubnetTrie ipv4_subnets_;
  SubnetTrie ipv6_subnets_;
  // Subnet metadata kept for ListRules serialization only.
  std::list<std::unique_ptr<SocketAddressMaskRule>> subnet_rules_;

  // RwLock allows concurrent Apply() calls (shared/read lock) while
  // mutations (Add*/Remove*/Clear) take an exclusive/write lock.
  mutable RwLock mutex_;
};

class SocketAddressBlockListWrap : public BaseObject {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);

  static BaseObjectPtr<SocketAddressBlockListWrap> New(Environment* env);
  static BaseObjectPtr<SocketAddressBlockListWrap> New(
      Environment* env, std::shared_ptr<SocketAddressBlockList> blocklist);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddAddress(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddAddresses(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddRange(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddSubnet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveAddress(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveRange(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveSubnet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Check(const v8::FunctionCallbackInfo<v8::Value>& args);
  static bool FastCheck(v8::Local<v8::Object> receiver,
                        v8::Local<v8::Object> addr_obj);
  static void CheckString(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetRules(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Clear(const v8::FunctionCallbackInfo<v8::Value>& args);

  SocketAddressBlockListWrap(Environment* env,
                             v8::Local<v8::Object> wrap,
                             std::shared_ptr<SocketAddressBlockList> blocklist =
                                 std::make_shared<SocketAddressBlockList>());

  inline const std::shared_ptr<SocketAddressBlockList>& blocklist() const {
    return blocklist_;
  }

  void MemoryInfo(node::MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SocketAddressBlockListWrap)
  SET_SELF_SIZE(SocketAddressBlockListWrap)

  BaseObject::TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

  class TransferData : public worker::TransferData {
   public:
    inline explicit TransferData(const SocketAddressBlockListWrap* wrap)
        : blocklist_(wrap->blocklist_) {}

    inline explicit TransferData(
        std::shared_ptr<SocketAddressBlockList> blocklist)
        : blocklist_(std::move(blocklist)) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(SocketAddressBlockListWrap::TransferData)
    SET_SELF_SIZE(TransferData)

   private:
    std::shared_ptr<SocketAddressBlockList> blocklist_;
  };

 private:
  std::shared_ptr<SocketAddressBlockList> blocklist_;
  static v8::CFunction fast_check_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SOCKADDR_H_
