#include "permission/env_permission.h"
#include "util.h"

static std::string_view TrimString(const std::string_view& str) {
  static const std::string whitespace = " \n\r\t";
  size_t first = str.find_first_not_of(whitespace);
  if (first == std::string_view::npos) return "";

  size_t last = str.find_last_not_of(whitespace);
  return str.substr(first, last - first + 1);
}

namespace node {

namespace permission {

bool EnvPermission::IsGranted(const std::string_view& param) {
  bool has_wildcard_granted = granted_patterns_->Lookup("*");
  bool has_wildcard_denied = denied_patterns_->Lookup("*");

  if (param.empty()) {
    bool has_any_denied = (has_wildcard_denied || !denied_patterns_->Empty());
    return has_wildcard_granted && !has_any_denied;
  }

  if (has_wildcard_denied) {
    return false;
  }

  if (has_wildcard_granted && denied_patterns_->Empty()) {
    return true;
  }

  if (denied_patterns_->Lookup(param)) {
    return false;
  }

  if (!has_wildcard_granted && !granted_patterns_->Lookup(param)) {
    return false;
  }

  return true;
}

bool EnvPermission::Apply(const std::shared_ptr<Patterns>& patterns,
                          const std::string_view& pattern) {
  const auto pos = pattern.find_first_of('*');
  if (pos != std::string_view::npos) {
    if (pattern.substr(pos + 1).size() > 0) {
      // Filter out this input if a wildcard is followed by any character. (e.g:
      // *ODE,  *ODE*, N*DE) Only a string with a wild card of the last
      // character is accepted. (e.g: *, -*, NODE*)
      return false;
    }
  }
  return patterns->Insert(std::string(pattern));
}

void EnvPermission::Apply(const std::string& allow, PermissionScope scope) {
  if (scope == PermissionScope::kEnvironment) {
    for (const auto& str : SplitString(allow, ',', true)) {
      const std::string_view token = TrimString(str);
      if (token.front() == '-') {
        Apply(denied_patterns_, token.substr(1));
      } else {
        Apply(granted_patterns_, token);
      }
    }
  }
}

bool EnvPermission::is_granted(PermissionScope scope,
                               const std::string_view& param) {
  if (scope == PermissionScope::kEnvironment) {
    return IsGranted(param);
  }
  return false;
}

bool EnvPermission::Patterns::Insert(const std::string& s) {
#ifdef _WIN32
  return SearchTree::Insert(ToUpper(s));
#else
  return SearchTree::Insert(s);
#endif
}

bool EnvPermission::Patterns::Lookup(const std::string_view& s,
                                     bool when_empty_return) {
#ifdef _WIN32
  return SearchTree::Lookup(ToUpper(std::string(s)), false);
#endif
  return SearchTree::Lookup(s, false);
}

}  // namespace permission
}  // namespace node
