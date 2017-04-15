// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/source-position.h"
#include "src/compilation-info.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& out, const SourcePositionInfo& pos) {
  Handle<SharedFunctionInfo> function(pos.function);
  Handle<Script> script(Script::cast(function->script()));
  out << "<";
  if (script->name()->IsString()) {
    out << String::cast(script->name())->ToCString(DISALLOW_NULLS).get();
  } else {
    out << "unknown";
  }
  out << ":" << pos.line + 1 << ":" << pos.column + 1 << ">";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector<SourcePositionInfo>& stack) {
  bool first = true;
  for (const SourcePositionInfo& pos : stack) {
    if (!first) out << " inlined at ";
    out << pos;
    first = false;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const SourcePosition& pos) {
  if (pos.isInlined()) {
    out << "<inlined(" << pos.InliningId() << "):";
  } else {
    out << "<not inlined:";
  }
  out << pos.ScriptOffset() << ">";
  return out;
}

std::vector<SourcePositionInfo> SourcePosition::InliningStack(
    CompilationInfo* cinfo) const {
  SourcePosition pos = *this;
  std::vector<SourcePositionInfo> stack;
  while (pos.isInlined()) {
    const auto& inl = cinfo->inlined_functions()[pos.InliningId()];
    stack.push_back(SourcePositionInfo(pos, inl.shared_info));
    pos = inl.position.position;
  }
  stack.push_back(SourcePositionInfo(pos, cinfo->shared_info()));
  return stack;
}

std::vector<SourcePositionInfo> SourcePosition::InliningStack(
    Handle<Code> code) const {
  Handle<DeoptimizationInputData> deopt_data(
      DeoptimizationInputData::cast(code->deoptimization_data()));
  SourcePosition pos = *this;
  std::vector<SourcePositionInfo> stack;
  while (pos.isInlined()) {
    InliningPosition inl =
        deopt_data->InliningPositions()->get(pos.InliningId());
    Handle<SharedFunctionInfo> function(
        deopt_data->GetInlinedFunction(inl.inlined_function_id));
    stack.push_back(SourcePositionInfo(pos, function));
    pos = inl.position;
  }
  Handle<SharedFunctionInfo> function(
      SharedFunctionInfo::cast(deopt_data->SharedFunctionInfo()));
  stack.push_back(SourcePositionInfo(pos, function));
  return stack;
}

void SourcePosition::Print(std::ostream& out,
                           SharedFunctionInfo* function) const {
  Script* script = Script::cast(function->script());
  Object* source_name = script->name();
  Script::PositionInfo pos;
  script->GetPositionInfo(ScriptOffset(), &pos, Script::WITH_OFFSET);
  out << "<";
  if (source_name->IsString()) {
    out << String::cast(source_name)
               ->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL)
               .get();
  } else {
    out << "unknown";
  }
  out << ":" << pos.line + 1 << ":" << pos.column + 1 << ">";
}

void SourcePosition::Print(std::ostream& out, Code* code) const {
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  if (!isInlined()) {
    SharedFunctionInfo* function(
        SharedFunctionInfo::cast(deopt_data->SharedFunctionInfo()));
    Print(out, function);
  } else {
    InliningPosition inl = deopt_data->InliningPositions()->get(InliningId());
    if (inl.inlined_function_id == -1) {
      out << *this;
    } else {
      SharedFunctionInfo* function =
          deopt_data->GetInlinedFunction(inl.inlined_function_id);
      Print(out, function);
    }
    out << " inlined at ";
    inl.position.Print(out, code);
  }
}

SourcePositionInfo::SourcePositionInfo(SourcePosition pos,
                                       Handle<SharedFunctionInfo> f)
    : position(pos), function(f) {
  Handle<Script> script(Script::cast(function->script()));
  Script::PositionInfo info;
  if (Script::GetPositionInfo(script, pos.ScriptOffset(), &info,
                              Script::WITH_OFFSET)) {
    line = info.line;
    column = info.column;
  }
}

}  // namespace internal
}  // namespace v8
