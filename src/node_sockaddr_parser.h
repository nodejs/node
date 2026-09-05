#ifndef SRC_NODE_SOCKADDR_PARSER_H_
#define SRC_NODE_SOCKADDR_PARSER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace node::sockaddr_parser {

// The length of the longest numeric address,
// "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255".
constexpr size_t kMaxHostLength = 45;

struct parse_result {
  bool is_ipv6;
  std::string_view host;
  uint16_t port;
  uint32_t scope_id;
};

// Splits input into its components. The host is left for uv_inet_pton to
// validate. See the grammar in doc/api/net.md.
std::optional<parse_result> ParseSocketAddress(std::string_view input);

}  // namespace node::sockaddr_parser

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SOCKADDR_PARSER_H_
