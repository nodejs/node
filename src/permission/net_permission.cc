#include "net_permission.h"
#include "debug_utils-inl.h"
#include "json_utils.h"
#include "util.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace node {

namespace permission {

int is_ip(const std::string& address) {
  char buf[IPV6_CHAR_LEN];
  if (uv_inet_pton(AF_INET, address.c_str(), &buf) == 0) {
    return 4;
  }
  if (uv_inet_pton(AF_INET6, address.c_str(), &buf) == 0) {
    return 6;
  }
  return 0;
}

bool is_digit(const std::string& str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

bool convert_ip_into_bitset(std::bitset<IPV6_BIT_LEN>* bitset,
                            const std::string& str,
                            int ip_version) {
  int family = ip_version == 6 ? AF_INET6 : AF_INET;
  int char_len = ip_version == 6 ? IPV6_CHAR_LEN : IPV4_CHAR_LEN;
  int position = 0;
  char buf[IPV6_CHAR_LEN];
  // IP(like 127.0.0.1) in buf is [01111111][00000000][00000000][00000001]
  if (uv_inet_pton(family, str.c_str(), &buf) == 0) {
    // Handle char by char
    for (int i = char_len - 1; i >= 0; i--) {
      // Handle byte by byte
      for (int j = 0; j < 8; j++) {
        bitset->set(position++, !!(buf[i] & (1 << j)));
      }
    }
    return true;
  }
  return false;
}

void NetPermission::Apply(Environment* env,
                          const std::vector<std::string>& allow,
                          PermissionScope scope) {
  // For Debug
  auto cleanup = OnScopeLeave([&]() { Print(); });
  using std::string_view_literals::operator""sv;
  for (const std::string& res : allow) {
    // Allow multiple item in one flag, like --allow-net-udp=127.0.0.1,localhost
    const std::vector<std::string_view> addresses = SplitString(res, ","sv);
    for (const auto& address : addresses) {
      switch (scope) {
        case PermissionScope::kNetUDP:
          // address is like *, *:*, host, host:*, host:port, *:port,
          // host/netmask:port
          if (address == "*"sv || address == "*:*"sv) {
            deny_all_udp_ = false;
            allow_all_udp_ = true;
            granted_udp_.clear();
            return;
          }
          GrantAccess(scope, address);
          continue;
        default:
          continue;
      }
    }
  }
}

void NetPermission::GrantAccess(PermissionScope scope,
                                const std::string_view& param) {
  std::unique_ptr<ParseResult> result = Parse(param);
  std::bitset<IPV6_BIT_LEN> ip;
  std::bitset<IPV6_BIT_LEN> netmask;
  std::unique_ptr<NetMaskNode> net_mask_node;
  int ip_version = is_ip(result->address);

  if (ip_version > 0 && !result->netmask.empty()) {
    if (is_digit(result->netmask)) {  // 127.0.0.1/24
      int netmask_len = std::stoi(result->netmask);
      if (netmask_len == 0) {
        std::cerr << "invalid netmask: " << result->netmask << std::endl;
        return;
      }
      int position = ip_version == 6 ? IPV6_BIT_LEN : IPV4_BIT_LEN;
      while (netmask_len--) {
        netmask.set(--position, true);
      }
    } else if (!convert_ip_into_bitset(
                   &netmask,
                   result->netmask,
                   ip_version)) {  // 127.0.0.1/255.255.255.0
      std::cerr << "invalid netmask: " << result->netmask << std::endl;
      return;
    }
    if (!convert_ip_into_bitset(&ip, result->address, ip_version)) {
      std::cerr << "invalid netmask: " << result->address << std::endl;
      return;
    }
    std::bitset<IPV6_BIT_LEN> network = netmask & ip;
    net_mask_node = std::make_unique<NetMaskNode>(netmask, network);
  }
  auto node = std::make_unique<NetNode>(
      result->address, result->port, std::move(net_mask_node));
  granted_udp_.push_back(std::move(node));
  deny_all_udp_ = false;
}

bool NetPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               const std::string_view& param = "") const {
  switch (perm) {
    case PermissionScope::kNetUDP:
      return !deny_all_udp_ &&
             (allow_all_udp_ || CheckoutResource(granted_udp_, param));
    default:
      return false;
  }
}

bool NetPermission::CheckoutResource(
    const std::vector<std::unique_ptr<NetNode>>& list,
    const std::string_view& param) const {
  if (param.empty()) {
    return false;
  }
  using std::string_view_literals::operator""sv;
  const std::vector<std::string_view> address_port = SplitString(param, "/"sv);
  int size = address_port.size();
  CHECK(size == 1 || size == 2);
  std::string address;
  std::string port;
  if (size == 2) {
    address = address_port[0];
    port = address_port[1];
  } else {
    address = address_port[0];
    port = "*";
  }
  for (uint32_t i = 0; i < list.size(); i++) {
    auto node = list[i].get();
    int ip_version = is_ip(address);
    if (ip_version > 0 && node->net_mask_node) {
      std::bitset<IPV6_BIT_LEN> ip;
      if (!convert_ip_into_bitset(&ip, address, ip_version)) {
        return false;
      }
      std::bitset<IPV6_BIT_LEN> network = node->net_mask_node->netmask & ip;
      if (node->net_mask_node->network == network &&
          (node->port == "*" || node->port == port)) {
        return true;
      }
    } else if ((node->address == "*" || node->address == address) &&
               (node->port == "*" || node->port == port)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<NetPermission::ParseResult> NetPermission::Parse(
    const std::string_view& param) const {
  using std::string_view_literals::operator""sv;
  size_t ipv6_idx = param.find("]"sv);
  bool is_ipv6 = ipv6_idx != std::string::npos;
  size_t net_mask_idx = param.find("/"sv);
  bool has_net_mask = net_mask_idx != std::string::npos;
  size_t port_idx = param.find(":"sv, is_ipv6 ? ipv6_idx : 0);
  bool has_port = port_idx != std::string::npos;
  std::string address;
  std::string net_mask;
  std::string port;
  if (is_ipv6) {
    address = param.substr(1, ipv6_idx - 1);
  } else {
    if (has_net_mask) {
      address = param.substr(0, net_mask_idx);
    } else if (has_port) {
      address = param.substr(0, port_idx);
    } else {
      address = param;
    }
  }
  if (has_net_mask) {
    if (has_port) {
      net_mask = param.substr(net_mask_idx + 1, port_idx - net_mask_idx - 1);
    } else {
      net_mask = param.substr(net_mask_idx + 1);
    }
  }
  if (has_port) {
    port = param.substr(port_idx + 1);
  }
  if (port.empty()) {
    port = "*";
  }
  return std::unique_ptr<ParseResult>(new ParseResult(address, net_mask, port));
}

void NetPermission::Print() const {
  if (UNLIKELY(per_process::enabled_debug_list.enabled(
          DebugCategory::PERMISSION_MODEL))) {
    std::cout << "net-udp-net: " << std::endl;
    std::cout << "  deny_all_udp_: " << deny_all_udp_ << std::endl;
    std::cout << "  allow_all_udp_: " << allow_all_udp_ << std::endl;
    JSONWriter writer(std::cout, false);
    writer.json_arraystart("Nodes");
    for (uint32_t i = 0; i < granted_udp_.size(); i++) {
      auto node = granted_udp_[i].get();
      writer.json_start();
      writer.json_keyvalue("address", node->address);
      writer.json_keyvalue("port", node->port);
      if (node->net_mask_node) {
        writer.json_keyvalue("netmask",
                             node->net_mask_node->netmask.to_string());
        writer.json_keyvalue("network",
                             node->net_mask_node->network.to_string());
      }
      writer.json_end();
    }
    writer.json_arrayend();
    std::cout << std::endl;
  }
}

}  // namespace permission
}  // namespace node
