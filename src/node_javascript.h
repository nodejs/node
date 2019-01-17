#pragma once

#include <map>
#include <string>

#if __has_include(<string_view>)
#include <string_view>
using std::string_view;
#else
#include <experimental/string_view>
using std::experimental::string_view;
#endif



namespace node {

namespace native_module {

using NativeModuleRecordMap = std::map<std::string, string_view>;

class JavascriptEmbeddedCode {
 public:
  JavascriptEmbeddedCode();
 protected:
  string_view config_;
  NativeModuleRecordMap source_;
};

}  // namespace native_module

}  // namespace node
