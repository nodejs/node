// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CPP_BUILDER_H_
#define V8_TORQUE_CPP_BUILDER_H_

#include <stack>

#include "src/torque/ast.h"
#include "src/torque/types.h"

namespace v8 {
namespace internal {
namespace torque {
namespace cpp {

struct TemplateParameter {
  explicit TemplateParameter(std::string name) : name(std::move(name)) {}
  TemplateParameter(std::string type, std::string name)
      : name(std::move(name)), type(std::move(type)) {}

  std::string name;
  std::string type;
};

class Class {
 public:
  explicit Class(std::string name) : name_(std::move(name)) {}
  Class(std::vector<TemplateParameter> template_parameters, std::string name)
      : template_parameters_(std::move(template_parameters)),
        name_(std::move(name)) {}

  std::string GetName() const { return name_; }
  std::vector<TemplateParameter> GetTemplateParameters() const {
    return template_parameters_;
  }

 private:
  std::vector<TemplateParameter> template_parameters_;
  std::string name_;
};

#define FUNCTION_FLAG_LIST(V) \
  V(Inline, 0x01)             \
  V(V8Inline, 0x03)           \
  V(Const, 0x04)              \
  V(Constexpr, 0x08)          \
  V(Export, 0x10)             \
  V(Static, 0x20)             \
  V(Override, 0x40)

class Function {
 public:
  enum FunctionFlag {
#define ENTRY(name, value) k##name = value,
    FUNCTION_FLAG_LIST(ENTRY)
#undef ENTRY
  };

  struct Parameter {
    std::string type;
    std::string name;
    std::string default_value;

    Parameter(std::string type, std::string name,
              std::string default_value = {})
        : type(std::move(type)),
          name(std::move(name)),
          default_value(std::move(default_value)) {}
  };

  explicit Function(std::string name)
      : pos_(CurrentSourcePosition::Get()),
        owning_class_(nullptr),
        name_(std::move(name)) {}
  Function(Class* owning_class, std::string name)
      : pos_(CurrentSourcePosition::Get()),
        owning_class_(owning_class),
        name_(std::move(name)) {}
  ~Function() = default;

  static Function DefaultGetter(std::string return_type, Class* owner,
                                std::string name) {
    Function getter(owner, std::move(name));
    getter.SetReturnType(std::move(return_type));
    getter.SetInline();
    getter.SetConst();
    return getter;
  }

  static Function DefaultSetter(Class* owner, std::string name,
                                std::string parameter_type,
                                std::string parameter_name) {
    Function setter(owner, std::move(name));
    setter.SetReturnType("void");
    setter.AddParameter(std::move(parameter_type), std::move(parameter_name));
    setter.SetInline();
    return setter;
  }

  void SetFlag(FunctionFlag flag, bool value = true) {
    if (value) {
      flags_ = flags_ | flag;
    } else {
      flags_ = flags_.without(flag);
    }
  }
  void SetFlags(base::Flags<FunctionFlag> flags, bool value = true) {
    if (value) {
      flags_ |= flags;
    } else {
      flags_ &= ~flags;
    }
  }
  bool HasFlag(FunctionFlag flag) const { return (flags_ & flag) == flag; }
#define ACCESSOR(name, value)                            \
  void Set##name(bool v = true) { SetFlag(k##name, v); } \
  bool Is##name() const { return HasFlag(k##name); }
  FUNCTION_FLAG_LIST(ACCESSOR)
#undef ACCESSOR

  void SetDescription(std::string description) {
    description_ = std::move(description);
  }
  void SetName(std::string name) { name_ = std::move(name); }
  void SetReturnType(std::string return_type) {
    return_type_ = std::move(return_type);
  }
  void AddParameter(std::string type, std::string name = {},
                    std::string default_value = {}) {
    parameters_.emplace_back(std::move(type), std::move(name),
                             std::move(default_value));
  }
  void InsertParameter(int index, std::string type, std::string name = {},
                       std::string default_value = {}) {
    DCHECK_GE(index, 0);
    DCHECK_LE(index, parameters_.size());
    parameters_.insert(
        parameters_.begin() + index,
        Parameter(std::move(type), std::move(name), std::move(default_value)));
  }
  std::vector<Parameter> GetParameters() const { return parameters_; }
  std::vector<std::string> GetParameterNames() const {
    std::vector<std::string> names;
    std::transform(parameters_.begin(), parameters_.end(),
                   std::back_inserter(names),
                   [](const Parameter& p) { return p.name; });
    return names;
  }

  static constexpr int kAutomaticIndentation = -1;
  void PrintDeclaration(std::ostream& stream,
                        int indentation = kAutomaticIndentation) const;
  void PrintDefinition(std::ostream& stream,
                       const std::function<void(std::ostream&)>& builder,
                       int indentation = 0) const;
  void PrintInlineDefinition(std::ostream& stream,
                             const std::function<void(std::ostream&)>& builder,
                             int indentation = 2) const;
  void PrintBeginDefinition(std::ostream& stream, int indentation = 0) const;
  void PrintEndDefinition(std::ostream& stream, int indentation = 0) const;

 protected:
  void PrintDeclarationHeader(std::ostream& stream, int indentation) const;

 private:
  SourcePosition pos_;
  Class* owning_class_;
  std::string description_;
  std::string name_;
  std::string return_type_;
  std::vector<Parameter> parameters_;
  base::Flags<FunctionFlag> flags_;
};

DEFINE_OPERATORS_FOR_FLAGS(base::Flags<Function::FunctionFlag>)
#undef FUNCTION_FLAG_LIST

class File {
 public:
  explicit File(std::ostream& stream) : stream_(&stream) {}

  void BeginIncludeGuard(const std::string& name);
  void EndIncludeGuard(const std::string& name);
  void BeginNamespace(std::string name);
  void BeginNamespace(std::string name0, std::string name1);
  void EndNamespace(const std::string& name);
  void EndNamespace(const std::string& name0, const std::string& name1);

  void AddInclude(std::string include) { includes_.insert(std::move(include)); }

  template <typename T>
  File& operator<<(const T& value) {
    s() << value;
    return *this;
  }

 protected:
  std::ostream& s() { return *stream_; }

 private:
  std::ostream* stream_;
  std::set<std::string> includes_;
  std::stack<std::string> namespaces_;
};

class IncludeGuardScope {
 public:
  explicit IncludeGuardScope(File* file, std::string name)
      : file_(file), name_(std::move(name)) {
    file_->BeginIncludeGuard(name_);
  }
  IncludeGuardScope(const IncludeGuardScope&) = delete;
  IncludeGuardScope(IncludeGuardScope&& other) V8_NOEXCEPT : file_(nullptr),
                                                             name_() {
    std::swap(file_, other.file_);
    std::swap(name_, other.name_);
  }
  ~IncludeGuardScope() {
    if (file_) {
      file_->EndIncludeGuard(name_);
    }
  }
  IncludeGuardScope& operator=(const IncludeGuardScope&) = delete;
  IncludeGuardScope& operator=(IncludeGuardScope&& other) V8_NOEXCEPT {
    if (this != &other) {
      DCHECK_NULL(file_);
      DCHECK(name_.empty());
      std::swap(file_, other.file_);
      std::swap(name_, other.name_);
    }
    return *this;
  }

 private:
  File* file_;
  std::string name_;
};

}  // namespace cpp
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CPP_BUILDER_H_
