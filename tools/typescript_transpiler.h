#ifndef TOOLS_TYPESCRIPT_TRANSPILER_H_
#define TOOLS_TYPESCRIPT_TRANSPILER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace node {
namespace js2c {

class TypeScriptTranspiler {
 public:
  TypeScriptTranspiler();
  ~TypeScriptTranspiler();

  TypeScriptTranspiler(const TypeScriptTranspiler&) = delete;
  TypeScriptTranspiler& operator=(const TypeScriptTranspiler&) = delete;

  int Initialize(const char* argv0);
  int Strip(std::string_view source,
            std::string_view filename,
            std::vector<char>* output);
  std::string_view LastError() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace js2c
}  // namespace node

#endif  // TOOLS_TYPESCRIPT_TRANSPILER_H_
