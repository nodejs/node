// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_BUILTIN_REDUCER_H_
#define V8_COMPILER_JS_BUILTIN_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct FieldAccess;
class JSGraph;
class JSOperatorBuilder;
class SimplifiedOperatorBuilder;
class TypeCache;

class V8_EXPORT_PRIVATE JSBuiltinReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  JSBuiltinReducer(Editor* editor, JSGraph* jsgraph,
                   CompilationDependencies* dependencies,
                   Handle<Context> native_context);
  ~JSBuiltinReducer() final {}

  const char* reducer_name() const override { return "JSBuiltinReducer"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceArrayIsArray(Node* node);

  Reduction ReduceDateNow(Node* node);
  Reduction ReduceDateGetTime(Node* node);
  Reduction ReduceGlobalIsFinite(Node* node);
  Reduction ReduceGlobalIsNaN(Node* node);
  Reduction ReduceNumberParseInt(Node* node);
  Reduction ReduceObjectCreate(Node* node);
  Node* ToNumber(Node* value);
  Node* ToUint32(Node* value);

  Graph* graph() const;
  Factory* factory() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Handle<Context> native_context() const { return native_context_; }
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  JSOperatorBuilder* javascript() const;
  CompilationDependencies* dependencies() const { return dependencies_; }

  CompilationDependencies* const dependencies_;
  JSGraph* const jsgraph_;
  Handle<Context> const native_context_;
  TypeCache const& type_cache_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_BUILTIN_REDUCER_H_
