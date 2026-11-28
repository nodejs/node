#ifndef SRC_NODE_URL_PATTERN_H_
#define SRC_NODE_URL_PATTERN_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "ada.h"
#include "base_object.h"
#include "util.h"

#include <v8.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace node::url_pattern {

#define URL_PATTERN_COMPONENTS(V)                                              \
  V(Protocol, protocol)                                                        \
  V(Username, username)                                                        \
  V(Password, password)                                                        \
  V(Hostname, hostname)                                                        \
  V(Port, port)                                                                \
  V(Pathname, pathname)                                                        \
  V(Search, search)                                                            \
  V(Hash, hash)

// By default, ada::url_pattern doesn't ship with any regex library.
// Ada has a std::regex implementation, but it is considered unsafe and does
// not have a fully compliant ecmascript syntax support. Therefore, Ada
// supports passing custom regex provider that conforms to the following
// class and function structure. For more information, please look into
// url_pattern_regex.h inside github.com/ada-url/ada.
class URLPatternRegexProvider {
 public:
  using regex_type = v8::Global<v8::RegExp>;
  static std::optional<regex_type> create_instance(std::string_view pattern,
                                                   bool ignore_case);
  static std::optional<std::vector<std::optional<std::string>>> regex_search(
      std::string_view input, const regex_type& pattern);
  static bool regex_match(std::string_view input, const regex_type& pattern);
};

class URLPattern : public BaseObject {
 public:
  URLPattern(Environment* env,
             v8::Local<v8::Object> object,
             ada::url_pattern<URLPatternRegexProvider>&& url_pattern);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);

  // V8 APIs
  // - Functions
  static void Exec(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Test(const v8::FunctionCallbackInfo<v8::Value>& info);
  // - Component Getters
#define URL_PATTERN_COMPONENT_GETTERS(name, _)                                 \
  static void name(const v8::FunctionCallbackInfo<v8::Value>& info);
  URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS
  // - Has Regexp Groups
  static void HasRegexpGroups(const v8::FunctionCallbackInfo<v8::Value>& info);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(URLPattern)
  SET_SELF_SIZE(URLPattern)

  class URLPatternInit {
   public:
    static std::optional<ada::url_pattern_init> FromJsObject(
        Environment* env, v8::Local<v8::Object> obj);
    static v8::MaybeLocal<v8::Value> ToJsObject(
        Environment* env, const ada::url_pattern_init& init);
  };

  class URLPatternOptions {
   public:
    static std::optional<ada::url_pattern_options> FromJsObject(
        Environment* env, v8::Local<v8::Object> obj);
  };

  class URLPatternResult {
   public:
    static v8::MaybeLocal<v8::Value> ToJSValue(
        Environment* env, const ada::url_pattern_result& result);
    // Returns a URLPatternListResult: a URLPatternResult whose `pattern`
    // member is the URLPattern that produced `result`.
    static v8::MaybeLocal<v8::Value> ToJSValue(
        Environment* env,
        const ada::url_pattern_result& result,
        v8::Local<v8::Object> pattern);
  };

  class URLPatternComponentResult {
   public:
    static v8::MaybeLocal<v8::Object> ToJSObject(
        Environment* env, const ada::url_pattern_component_result& result);
  };

 private:
  friend class URLPatternList;

  ada::url_pattern<URLPatternRegexProvider> url_pattern_;
  // Getter methods
#define URL_PATTERN_COMPONENT_GETTERS(name, _) v8::MaybeLocal<v8::Value> name();
  URL_PATTERN_COMPONENTS(URL_PATTERN_COMPONENT_GETTERS)
#undef URL_PATTERN_COMPONENT_GETTERS
  bool HasRegExpGroups() const;
  // Public API
  v8::MaybeLocal<v8::Value> Exec(
      Environment* env,
      const ada::url_pattern_input& input,
      std::optional<std::string_view>& baseURL);  // NOLINT (runtime/references)
  bool Test(
      Environment* env,
      const ada::url_pattern_input& input,
      std::optional<std::string_view>& baseURL);  // NOLINT (runtime/references)

#define URL_PATTERN_CACHED_VALUES(_, name) v8::Global<v8::Value> name;
  URL_PATTERN_COMPONENTS(URL_PATTERN_CACHED_VALUES)
#undef URL_PATTERN_CACHED_VALUES
};

// url.URLPatternList, which is specified in
// https://github.com/whatwg/urlpattern/pull/166. The list matches an input
// against its patterns in list order and reports the first one that matches.
class URLPatternList : public BaseObject {
 public:
  URLPatternList(Environment* env,
                 v8::Local<v8::Object> object,
                 std::vector<BaseObjectPtr<URLPattern>>&& patterns);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Exec(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Test(const v8::FunctionCallbackInfo<v8::Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(URLPatternList)
  SET_SELF_SIZE(URLPatternList)

  // The URL components that a URLPattern matches against. They are computed
  // once per test() or exec() call, as the URLPattern "match" algorithm
  // computes them before running the component regular expressions.
  struct Components {
    std::string protocol;
    std::string username;
    std::string password;
    std::string hostname;
    std::string port;
    std::string pathname;
    std::string search;
    std::string hash;
  };

 private:
  void BuildPathnameIndex();
  bool Matches(size_t index, const Components& components) const;
  // Returns the index of the first pattern that matches `components`, in list
  // order. When `any_match` is true, it may return any matching pattern.
  std::optional<size_t> FindMatch(const Components& components,
                                  bool any_match) const;

  std::vector<BaseObjectPtr<URLPattern>> patterns_;
  // ada::url_pattern_list is a compiled index of pathname patterns that
  // reports the most specific matching pathname, not the first pattern that
  // matches the whole URL. It is used to skip patterns that cannot match:
  // when no indexed pathname matches, no indexed pattern matches either.
  std::optional<ada::url_pattern_list<URLPatternRegexProvider>> pathname_index_;
  // Maps a route index of `pathname_index_` to a position in `patterns_`.
  std::vector<size_t> indexed_patterns_;
  // Positions in `patterns_` of the patterns that `pathname_index_` does not
  // cover. They are tested whether or not an indexed pathname matches.
  std::vector<size_t> unindexed_patterns_;
};

}  // namespace node::url_pattern

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_URL_PATTERN_H_
