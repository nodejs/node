#include "node_sockaddr-inl.h"  // NOLINT(build/include)
#include "env-inl.h"
#include "base_object-inl.h"
#include "memory_tracker-inl.h"
#include "uv.h"

#include <memory>
#include <string>
#include <vector>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

namespace {
template <typename T, typename F>
SocketAddress FromUVHandle(F fn, const T& handle) {
  SocketAddress addr;
  int len = sizeof(sockaddr_storage);
  if (fn(&handle, addr.storage(), &len) == 0)
    CHECK_EQ(static_cast<size_t>(len), addr.length());
  else
    addr.storage()->sa_family = 0;
  return addr;
}
}  // namespace

bool SocketAddress::ToSockAddr(
    int32_t family,
    const char* host,
    uint32_t port,
    sockaddr_storage* addr) {
  switch (family) {
    case AF_INET:
      return uv_ip4_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in*>(addr)) == 0;
    case AF_INET6:
      return uv_ip6_addr(
          host,
          port,
          reinterpret_cast<sockaddr_in6*>(addr)) == 0;
    default:
      UNREACHABLE();
  }
}

bool SocketAddress::New(
    const char* host,
    uint32_t port,
    SocketAddress* addr) {
  return New(AF_INET, host, port, addr) || New(AF_INET6, host, port, addr);
}

bool SocketAddress::New(
    int32_t family,
    const char* host,
    uint32_t port,
    SocketAddress* addr) {
  return ToSockAddr(family, host, port,
                    reinterpret_cast<sockaddr_storage*>(addr->storage()));
}

size_t SocketAddress::Hash::operator()(const SocketAddress& addr) const {
  size_t hash = 0;
  switch (addr.family()) {
    case AF_INET: {
      const sockaddr_in* ipv4 =
          reinterpret_cast<const sockaddr_in*>(addr.raw());
      hash_combine(&hash, ipv4->sin_port, ipv4->sin_addr.s_addr);
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* ipv6 =
          reinterpret_cast<const sockaddr_in6*>(addr.raw());
      const uint64_t* a =
          reinterpret_cast<const uint64_t*>(&ipv6->sin6_addr);
      hash_combine(&hash, ipv6->sin6_port, a[0], a[1]);
      break;
    }
    default:
      UNREACHABLE();
  }
  return hash;
}

SocketAddress SocketAddress::FromSockName(const uv_tcp_t& handle) {
  return FromUVHandle(uv_tcp_getsockname, handle);
}

SocketAddress SocketAddress::FromSockName(const uv_udp_t& handle) {
  return FromUVHandle(uv_udp_getsockname, handle);
}

SocketAddress SocketAddress::FromPeerName(const uv_tcp_t& handle) {
  return FromUVHandle(uv_tcp_getpeername, handle);
}

SocketAddress SocketAddress::FromPeerName(const uv_udp_t& handle) {
  return FromUVHandle(uv_udp_getpeername, handle);
}

namespace {
constexpr uint8_t mask[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

bool is_match_ipv4(
    const SocketAddress& one,
    const SocketAddress& two) {
  const sockaddr_in* one_in =
      reinterpret_cast<const sockaddr_in*>(one.data());
  const sockaddr_in* two_in =
      reinterpret_cast<const sockaddr_in*>(two.data());
  return memcmp(&one_in->sin_addr, &two_in->sin_addr, sizeof(uint32_t)) == 0;
}

bool is_match_ipv6(
    const SocketAddress& one,
    const SocketAddress& two) {
  const sockaddr_in6* one_in =
      reinterpret_cast<const sockaddr_in6*>(one.data());
  const sockaddr_in6* two_in =
      reinterpret_cast<const sockaddr_in6*>(two.data());
  return memcmp(&one_in->sin6_addr, &two_in->sin6_addr, 16) == 0;
}

bool is_match_ipv4_ipv6(
    const SocketAddress& ipv4,
    const SocketAddress& ipv6) {
  const sockaddr_in* check_ipv4 =
      reinterpret_cast<const sockaddr_in*>(ipv4.data());
  const sockaddr_in6* check_ipv6 =
      reinterpret_cast<const sockaddr_in6*>(ipv6.data());

  const uint8_t* ptr =
      reinterpret_cast<const uint8_t*>(&check_ipv6->sin6_addr);

  return memcmp(ptr, mask, sizeof(mask)) == 0 &&
         memcmp(ptr + sizeof(mask),
                &check_ipv4->sin_addr,
                sizeof(uint32_t)) == 0;
}

SocketAddress::CompareResult compare_ipv4(
    const SocketAddress& one,
    const SocketAddress& two) {
  const sockaddr_in* one_in =
      reinterpret_cast<const sockaddr_in*>(one.data());
  const sockaddr_in* two_in =
      reinterpret_cast<const sockaddr_in*>(two.data());

  if (one_in->sin_addr.s_addr < two_in->sin_addr.s_addr)
    return SocketAddress::CompareResult::LESS_THAN;
  else if (one_in->sin_addr.s_addr == two_in->sin_addr.s_addr)
    return SocketAddress::CompareResult::SAME;
  else
    return SocketAddress::CompareResult::GREATER_THAN;
}

SocketAddress::CompareResult compare_ipv6(
    const SocketAddress& one,
    const SocketAddress& two) {
  const sockaddr_in6* one_in =
      reinterpret_cast<const sockaddr_in6*>(one.data());
  const sockaddr_in6* two_in =
      reinterpret_cast<const sockaddr_in6*>(two.data());
  int ret = memcmp(&one_in->sin6_addr, &two_in->sin6_addr, 16);
  if (ret < 0)
    return SocketAddress::CompareResult::LESS_THAN;
  else if (ret > 0)
    return SocketAddress::CompareResult::GREATER_THAN;
  return SocketAddress::CompareResult::SAME;
}

SocketAddress::CompareResult compare_ipv4_ipv6(
    const SocketAddress& ipv4,
    const SocketAddress& ipv6) {
  const sockaddr_in* ipv4_in =
      reinterpret_cast<const sockaddr_in*>(ipv4.data());
  const sockaddr_in6 * ipv6_in =
      reinterpret_cast<const sockaddr_in6*>(ipv6.data());

  const uint8_t* ptr =
      reinterpret_cast<const uint8_t*>(&ipv6_in->sin6_addr);

  if (memcmp(ptr, mask, sizeof(mask)) != 0)
    return SocketAddress::CompareResult::NOT_COMPARABLE;

  int ret = memcmp(
      &ipv4_in->sin_addr,
      ptr + sizeof(mask),
      sizeof(uint32_t));

  if (ret < 0)
    return SocketAddress::CompareResult::LESS_THAN;
  else if (ret > 0)
    return SocketAddress::CompareResult::GREATER_THAN;
  return SocketAddress::CompareResult::SAME;
}

bool in_network_ipv4(
    const SocketAddress& ip,
    const SocketAddress& net,
    int prefix) {
  uint32_t mask = ((1 << prefix) - 1) << (32 - prefix);

  const sockaddr_in* ip_in =
      reinterpret_cast<const sockaddr_in*>(ip.data());
  const sockaddr_in* net_in =
      reinterpret_cast<const sockaddr_in*>(net.data());

  return (htonl(ip_in->sin_addr.s_addr) & mask) ==
         (htonl(net_in->sin_addr.s_addr) & mask);
}

bool in_network_ipv6(
    const SocketAddress& ip,
    const SocketAddress& net,
    int prefix) {
  // Special case, if prefix == 128, then just do a
  // straight comparison.
  if (prefix == 128)
    return compare_ipv6(ip, net) == SocketAddress::CompareResult::SAME;

  uint8_t r = prefix % 8;
  int len = (prefix - r) / 8;
  uint8_t mask = ((1 << r) - 1) << (8 - r);

  const sockaddr_in6* ip_in =
      reinterpret_cast<const sockaddr_in6*>(ip.data());
  const sockaddr_in6* net_in =
      reinterpret_cast<const sockaddr_in6*>(net.data());

  if (memcmp(&ip_in->sin6_addr, &net_in->sin6_addr, len) != 0)
    return false;

  const uint8_t* p1 = reinterpret_cast<const uint8_t*>(
      ip_in->sin6_addr.s6_addr);
  const uint8_t* p2 = reinterpret_cast<const uint8_t*>(
      net_in->sin6_addr.s6_addr);

  return (p1[len] & mask) == (p2[len] & mask);
}

bool in_network_ipv4_ipv6(
    const SocketAddress& ip,
    const SocketAddress& net,
    int prefix) {

  if (prefix == 128)
    return compare_ipv4_ipv6(ip, net) == SocketAddress::CompareResult::SAME;

  uint8_t r = prefix % 8;
  int len = (prefix - r) / 8;
  uint8_t mask = ((1 << r) - 1) << (8 - r);

  const sockaddr_in* ip_in =
      reinterpret_cast<const sockaddr_in*>(ip.data());
  const sockaddr_in6* net_in =
      reinterpret_cast<const sockaddr_in6*>(net.data());

  uint8_t ip_mask[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0};
  uint8_t* ptr = ip_mask;
  memcpy(ptr + 12, &ip_in->sin_addr, 4);

  if (memcmp(ptr, &net_in->sin6_addr, len) != 0)
    return false;

  ptr += len;
  const uint8_t* p2 = reinterpret_cast<const uint8_t*>(
      net_in->sin6_addr.s6_addr);

  return (ptr[0] & mask) == (p2[len] & mask);
}

bool in_network_ipv6_ipv4(
    const SocketAddress& ip,
    const SocketAddress& net,
    int prefix) {
  if (prefix == 32)
    return compare_ipv4_ipv6(net, ip) == SocketAddress::CompareResult::SAME;

  uint32_t m = ((1 << prefix) - 1) << (32 - prefix);

  const sockaddr_in6* ip_in =
      reinterpret_cast<const sockaddr_in6*>(ip.data());
  const sockaddr_in* net_in =
      reinterpret_cast<const sockaddr_in*>(net.data());

  const uint8_t* ptr =
      reinterpret_cast<const uint8_t*>(&ip_in->sin6_addr);

  if (memcmp(ptr, mask, sizeof(mask)) != 0)
    return false;

  ptr += sizeof(mask);
  uint32_t check = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];

  return (check & m) == (htonl(net_in->sin_addr.s_addr) & m);
}
}  // namespace

// TODO(@jasnell): The implementations of is_match, compare, and
// is_in_network have not been performance optimized and could
// likely benefit from work on more performant approaches.

bool SocketAddress::is_match(const SocketAddress& other) const {
  switch (family()) {
    case AF_INET:
      switch (other.family()) {
        case AF_INET: return is_match_ipv4(*this, other);
        case AF_INET6: return is_match_ipv4_ipv6(*this, other);
      }
      break;
    case AF_INET6:
      switch (other.family()) {
        case AF_INET: return is_match_ipv4_ipv6(other, *this);
        case AF_INET6: return is_match_ipv6(*this, other);
      }
      break;
  }
  return false;
}

SocketAddress::CompareResult SocketAddress::compare(
    const SocketAddress& other) const {
  switch (family()) {
    case AF_INET:
      switch (other.family()) {
        case AF_INET: return compare_ipv4(*this, other);
        case AF_INET6: return compare_ipv4_ipv6(*this, other);
      }
      break;
    case AF_INET6:
      switch (other.family()) {
        case AF_INET: {
          CompareResult c = compare_ipv4_ipv6(other, *this);
          switch (c) {
            case SocketAddress::CompareResult::NOT_COMPARABLE:
              // Fall through
            case SocketAddress::CompareResult::SAME:
              return c;
            case SocketAddress::CompareResult::GREATER_THAN:
              return SocketAddress::CompareResult::LESS_THAN;
            case SocketAddress::CompareResult::LESS_THAN:
              return SocketAddress::CompareResult::GREATER_THAN;
          }
          break;
        }
        case AF_INET6: return compare_ipv6(*this, other);
      }
      break;
  }
  return SocketAddress::CompareResult::NOT_COMPARABLE;
}

bool SocketAddress::is_in_network(
    const SocketAddress& other,
    int prefix) const {

  switch (family()) {
    case AF_INET:
      switch (other.family()) {
        case AF_INET: return in_network_ipv4(*this, other, prefix);
        case AF_INET6: return in_network_ipv4_ipv6(*this, other, prefix);
      }
      break;
    case AF_INET6:
      switch (other.family()) {
        case AF_INET: return in_network_ipv6_ipv4(*this, other, prefix);
        case AF_INET6: return in_network_ipv6(*this, other, prefix);
      }
      break;
  }

  return false;
}

SocketAddressBlockList::SocketAddressBlockList(
    std::shared_ptr<SocketAddressBlockList> parent)
    : parent_(parent) {}

void SocketAddressBlockList::AddSocketAddress(
    const SocketAddress& address) {
  std::unique_ptr<Rule> rule =
      std::make_unique<SocketAddressRule>(address);
  rules_.emplace_front(std::move(rule));
  address_rules_[address] = rules_.begin();
}

void SocketAddressBlockList::RemoveSocketAddress(
    const SocketAddress& address) {
  auto it = address_rules_.find(address);
  if (it != std::end(address_rules_)) {
    rules_.erase(it->second);
    address_rules_.erase(it);
  }
}

void SocketAddressBlockList::AddSocketAddressRange(
    const SocketAddress& start,
    const SocketAddress& end) {
  std::unique_ptr<Rule> rule =
      std::make_unique<SocketAddressRangeRule>(start, end);
  rules_.emplace_front(std::move(rule));
}

void SocketAddressBlockList::AddSocketAddressMask(
    const SocketAddress& network,
    int prefix) {
  std::unique_ptr<Rule> rule =
      std::make_unique<SocketAddressMaskRule>(network, prefix);
  rules_.emplace_front(std::move(rule));
}

bool SocketAddressBlockList::Apply(const SocketAddress& address) {
  for (const auto& rule : rules_) {
    if (rule->Apply(address))
      return true;
  }
  return parent_ ? parent_->Apply(address) : false;
}

SocketAddressBlockList::SocketAddressRule::SocketAddressRule(
    const SocketAddress& address_)
    : address(address_) {}

SocketAddressBlockList::SocketAddressRangeRule::SocketAddressRangeRule(
    const SocketAddress& start_,
    const SocketAddress& end_)
    : start(start_),
      end(end_) {}

SocketAddressBlockList::SocketAddressMaskRule::SocketAddressMaskRule(
    const SocketAddress& network_,
    int prefix_)
    : network(network_),
      prefix(prefix_) {}

bool SocketAddressBlockList::SocketAddressRule::Apply(
    const SocketAddress& address) {
  return this->address.is_match(address);
}

std::string SocketAddressBlockList::SocketAddressRule::ToString() {
  std::string ret = "Address: ";
  ret += address.family() == AF_INET ? "IPv4" : "IPv6";
  ret += " ";
  ret += address.address();
  return ret;
}

bool SocketAddressBlockList::SocketAddressRangeRule::Apply(
    const SocketAddress& address) {
  return address >= start && address <= end;
}

std::string SocketAddressBlockList::SocketAddressRangeRule::ToString() {
  std::string ret = "Range: ";
  ret += start.family() == AF_INET ? "IPv4" : "IPv6";
  ret += " ";
  ret += start.address();
  ret += "-";
  ret += end.address();
  return ret;
}

bool SocketAddressBlockList::SocketAddressMaskRule::Apply(
    const SocketAddress& address) {
  return address.is_in_network(network, prefix);
}

std::string SocketAddressBlockList::SocketAddressMaskRule::ToString() {
  std::string ret = "Subnet: ";
  ret += network.family() == AF_INET ? "IPv4" : "IPv6";
  ret += " ";
  ret += network.address();
  ret += "/" + std::to_string(prefix);
  return ret;
}

MaybeLocal<Array> SocketAddressBlockList::ListRules(Environment* env) {
  std::vector<Local<Value>> rules;
  for (const auto& rule : rules_) {
    Local<Value> str;
    if (!rule->ToV8String(env).ToLocal(&str))
      return MaybeLocal<Array>();
    rules.push_back(str);
  }
  return Array::New(env->isolate(), rules.data(), rules.size());
}

void SocketAddressBlockList::MemoryInfo(node::MemoryTracker* tracker) const {
  tracker->TrackField("rules", rules_);
}

void SocketAddressBlockList::SocketAddressRule::MemoryInfo(
    node::MemoryTracker* tracker) const {
  tracker->TrackField("address", address);
}

void SocketAddressBlockList::SocketAddressRangeRule::MemoryInfo(
    node::MemoryTracker* tracker) const {
  tracker->TrackField("start", start);
  tracker->TrackField("end", end);
}

void SocketAddressBlockList::SocketAddressMaskRule::MemoryInfo(
    node::MemoryTracker* tracker) const {
  tracker->TrackField("network", network);
}

SocketAddressBlockListWrap::SocketAddressBlockListWrap(
    Environment* env, Local<Object> wrap)
    : BaseObject(env, wrap) {
  MakeWeak();
}

BaseObjectPtr<SocketAddressBlockListWrap> SocketAddressBlockListWrap::New(
    Environment* env) {
  Local<Object> obj;
  if (!env->blocklist_instance_template()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return {};
  }
  BaseObjectPtr<SocketAddressBlockListWrap> wrap =
      MakeDetachedBaseObject<SocketAddressBlockListWrap>(env, obj);
  CHECK(wrap);
  return wrap;
}

void SocketAddressBlockListWrap::New(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new SocketAddressBlockListWrap(env, args.This());
}

void SocketAddressBlockListWrap::AddAddress(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SocketAddressBlockListWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());

  sockaddr_storage address;
  Utf8Value value(args.GetIsolate(), args[0]);
  int32_t family;
  if (!args[1]->Int32Value(env->context()).To(&family))
    return;

  if (!SocketAddress::ToSockAddr(family, *value, 0, &address))
    return;

  wrap->AddSocketAddress(
      SocketAddress(reinterpret_cast<const sockaddr*>(&address)));

  args.GetReturnValue().Set(true);
}

void SocketAddressBlockListWrap::AddRange(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SocketAddressBlockListWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsInt32());

  sockaddr_storage address[2];
  Utf8Value start(args.GetIsolate(), args[0]);
  Utf8Value end(args.GetIsolate(), args[1]);
  int32_t family;
  if (!args[2]->Int32Value(env->context()).To(&family))
    return;

  if (!SocketAddress::ToSockAddr(family, *start, 0, &address[0]) ||
      !SocketAddress::ToSockAddr(family, *end, 0, &address[1])) {
    return;
  }

  SocketAddress start_addr(reinterpret_cast<const sockaddr*>(&address[0]));
  SocketAddress end_addr(reinterpret_cast<const sockaddr*>(&address[1]));

  // Starting address must come before the end address
  if (start_addr > end_addr)
    return args.GetReturnValue().Set(false);

  wrap->AddSocketAddressRange(start_addr, end_addr);

  args.GetReturnValue().Set(true);
}

void SocketAddressBlockListWrap::AddSubnet(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SocketAddressBlockListWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsInt32());

  sockaddr_storage address;
  Utf8Value network(args.GetIsolate(), args[0]);
  int32_t family;
  int32_t prefix;
  if (!args[1]->Int32Value(env->context()).To(&family) ||
      !args[2]->Int32Value(env->context()).To(&prefix)) {
    return;
  }

  if (!SocketAddress::ToSockAddr(family, *network, 0, &address))
    return;

  CHECK_IMPLIES(family == AF_INET, prefix <= 32);
  CHECK_IMPLIES(family == AF_INET6, prefix <= 128);
  CHECK_GE(prefix, 0);

  wrap->AddSocketAddressMask(
      SocketAddress(reinterpret_cast<const sockaddr*>(&address)),
      prefix);

  args.GetReturnValue().Set(true);
}

void SocketAddressBlockListWrap::Check(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SocketAddressBlockListWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());

  sockaddr_storage address;
  Utf8Value value(args.GetIsolate(), args[0]);
  int32_t family;
  if (!args[1]->Int32Value(env->context()).To(&family))
    return;

  if (!SocketAddress::ToSockAddr(family, *value, 0, &address))
    return;

  args.GetReturnValue().Set(
      wrap->Apply(SocketAddress(reinterpret_cast<const sockaddr*>(&address))));
}

void SocketAddressBlockListWrap::GetRules(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SocketAddressBlockListWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Local<Array> rules;
  if (wrap->ListRules(env).ToLocal(&rules))
    args.GetReturnValue().Set(rules);
}

void SocketAddressBlockListWrap::Initialize(
    Local<Object> target,
    Local<Value> unused,
    Local<Context> context,
    void* priv) {
  Environment* env = Environment::GetCurrent(context);

  Local<String> name = FIXED_ONE_BYTE_STRING(env->isolate(), "BlockList");
  Local<FunctionTemplate> t =
      env->NewFunctionTemplate(SocketAddressBlockListWrap::New);
  t->InstanceTemplate()->SetInternalFieldCount(BaseObject::kInternalFieldCount);
  t->SetClassName(name);

  env->SetProtoMethod(t, "addAddress", SocketAddressBlockListWrap::AddAddress);
  env->SetProtoMethod(t, "addRange", SocketAddressBlockListWrap::AddRange);
  env->SetProtoMethod(t, "addSubnet", SocketAddressBlockListWrap::AddSubnet);
  env->SetProtoMethod(t, "check", SocketAddressBlockListWrap::Check);
  env->SetProtoMethod(t, "getRules", SocketAddressBlockListWrap::GetRules);

  env->set_blocklist_instance_template(t->InstanceTemplate());
  target->Set(env->context(), name,
              t->GetFunction(env->context()).ToLocalChecked()).FromJust();

  NODE_DEFINE_CONSTANT(target, AF_INET);
  NODE_DEFINE_CONSTANT(target, AF_INET6);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    block_list,
    node::SocketAddressBlockListWrap::Initialize)
