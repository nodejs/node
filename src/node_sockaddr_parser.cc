#include "node_sockaddr_parser.h"

namespace node::sockaddr_parser {

namespace {

constexpr uint32_t kMaxPort = 65535;
constexpr uint32_t kMaxScopeId = UINT32_MAX;

// Not std::isdigit: that one is locale dependent.
std::optional<uint32_t> ToDigit(char c) {
  if (c < '0' || c > '9') return std::nullopt;
  return static_cast<uint32_t>(c - '0');
}

// A cursor over the input. A Read that fails consumes nothing.
class Parser {
 public:
  explicit Parser(std::string_view input) : remaining_(input) {}

  bool done() const { return remaining_.empty(); }

  bool ReadChar(char expected) {
    if (remaining_.empty() || remaining_.front() != expected) return false;
    remaining_.remove_prefix(1);
    return true;
  }

  std::optional<uint32_t> ReadNumber(uint32_t max_value);
  std::optional<uint32_t> ReadTaggedNumber(char tag, uint32_t max_value);
  std::optional<std::string_view> ReadHost(std::string_view delimiters);

  std::optional<parse_result> ReadIPv4SocketAddress();
  std::optional<parse_result> ReadIPv6SocketAddress();
  std::optional<parse_result> ReadSocketAddress();

 private:
  std::string_view remaining_;
};

std::optional<uint32_t> Parser::ReadNumber(uint32_t max_value) {
  const std::string_view start = remaining_;

  uint64_t value = 0;
  size_t digits = 0;

  while (!remaining_.empty()) {
    std::optional<uint32_t> digit = ToDigit(remaining_.front());
    if (!digit.has_value()) break;
    remaining_.remove_prefix(1);
    value = value * 10 + digit.value();
    digits++;
    if (value > max_value) break;
  }

  if (digits == 0 || value > max_value) {
    remaining_ = start;
    return std::nullopt;
  }

  return static_cast<uint32_t>(value);
}

std::optional<uint32_t> Parser::ReadTaggedNumber(char tag, uint32_t max_value) {
  const std::string_view start = remaining_;

  if (!ReadChar(tag)) return std::nullopt;

  std::optional<uint32_t> value = ReadNumber(max_value);
  if (!value.has_value()) remaining_ = start;

  return value;
}

std::optional<std::string_view> Parser::ReadHost(std::string_view delimiters) {
  const size_t end = remaining_.find_first_of(delimiters);
  const std::string_view host =
      end == std::string_view::npos ? remaining_ : remaining_.substr(0, end);

  // uv_inet_pton reads only up to the first NUL.
  if (host.size() > kMaxHostLength ||
      host.find('\0') != std::string_view::npos) {
    return std::nullopt;
  }

  remaining_.remove_prefix(host.size());

  return host;
}

std::optional<parse_result> Parser::ReadIPv4SocketAddress() {
  std::optional<std::string_view> host = ReadHost(":");
  if (!host.has_value()) return std::nullopt;

  parse_result result = {};
  result.host = *host;
  result.port =
      static_cast<uint16_t>(ReadTaggedNumber(':', kMaxPort).value_or(0));

  return result;
}

std::optional<parse_result> Parser::ReadIPv6SocketAddress() {
  if (!ReadChar('[')) return std::nullopt;

  std::optional<std::string_view> host = ReadHost("%]");
  if (!host.has_value()) return std::nullopt;

  parse_result result = {};
  result.is_ipv6 = true;
  result.host = *host;
  result.scope_id = ReadTaggedNumber('%', kMaxScopeId).value_or(0);

  if (!ReadChar(']')) return std::nullopt;

  result.port =
      static_cast<uint16_t>(ReadTaggedNumber(':', kMaxPort).value_or(0));

  return result;
}

std::optional<parse_result> Parser::ReadSocketAddress() {
  return remaining_.starts_with('[') ? ReadIPv6SocketAddress()
                                     : ReadIPv4SocketAddress();
}

}  // namespace

std::optional<parse_result> ParseSocketAddress(std::string_view input) {
  Parser parser(input);
  std::optional<parse_result> result = parser.ReadSocketAddress();
  if (!result.has_value() || !parser.done()) return std::nullopt;
  return result;
}

}  // namespace node::sockaddr_parser
