// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_PARSER_H_
#define V8_ASMJS_ASM_PARSER_H_

#include <memory>

#include "src/asmjs/asm-scanner.h"
#include "src/asmjs/asm-types.h"
#include "src/base/enum-set.h"
#include "src/base/vector.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Utf16CharacterStream;

namespace wasm {

// A custom parser + validator + wasm converter for asm.js:
// http://asmjs.org/spec/latest/
// This parser intentionally avoids the portion of JavaScript parsing
// that are not required to determine if code is valid asm.js code.
// * It is mostly one pass.
// * It bails out on unexpected input.
// * It assumes strict ordering insofar as permitted by asm.js validation rules.
// * It relies on a custom scanner that provides de-duped identifiers in two
//   scopes (local + module wide).
class AsmJsParser {
 public:
  // clang-format off
  enum StandardMember {
    kInfinity,
    kNaN,
#define V(_unused1, name, _unused2, _unused3) kMath##name,
    STDLIB_MATH_FUNCTION_LIST(V)
#undef V
#define V(name, _unused1) kMath##name,
    STDLIB_MATH_VALUE_LIST(V)
#undef V
#define V(name, _unused1, _unused2, _unused3) k##name,
    STDLIB_ARRAY_TYPE_LIST(V)
#undef V
  };
  // clang-format on

  using StdlibSet = base::EnumSet<StandardMember, uint64_t>;

  explicit AsmJsParser(Zone* zone, uintptr_t stack_limit,
                       Utf16CharacterStream* stream);
  bool Run();
  const char* failure_message() const { return failure_message_; }
  int failure_location() const { return failure_location_; }
  WasmModuleBuilder* module_builder() { return module_builder_; }
  const StdlibSet* stdlib_uses() const { return &stdlib_uses_; }

 private:
  // clang-format off
  enum class VarKind {
    kUnused,
    kLocal,
    kGlobal,
    kSpecial,
    kFunction,
    kTable,
    kImportedFunction,
#define V(_unused0, Name, _unused1, _unused2) kMath##Name,
    STDLIB_MATH_FUNCTION_LIST(V)
#undef V
#define V(Name, _unused1) kMath##Name,
    STDLIB_MATH_VALUE_LIST(V)
#undef V
  };
  // clang-format on

  // A single import in asm.js can require multiple imports in wasm, if the
  // function is used with different signatures. {cache} keeps the wasm
  // imports for the single asm.js import of name {function_name}.
  struct FunctionImportInfo {
    base::Vector<const char> function_name;
    ZoneUnorderedMap<FunctionSig, uint32_t> cache;

    // Constructor.
    FunctionImportInfo(base::Vector<const char> name, Zone* zone)
        : function_name(name), cache(zone) {}
  };

  struct VarInfo {
    AsmType* type = AsmType::None();
    WasmFunctionBuilder* function_builder = nullptr;
    FunctionImportInfo* import = nullptr;
    uint32_t mask = 0;
    uint32_t index = 0;
    VarKind kind = VarKind::kUnused;
    bool mutable_variable = true;
    bool function_defined = false;
  };

  struct GlobalImport {
    base::Vector<const char> import_name;
    ValueType value_type;
    VarInfo* var_info;
  };

  // Distinguish different kinds of blocks participating in {block_stack}. Each
  // entry on that stack represents one block in the wasm code, and determines
  // which block 'break' and 'continue' target in the current context:
  //  - kRegular: The target of a 'break' (with & without identifier).
  //              Pushed by an IterationStatement and a SwitchStatement.
  //  - kLoop   : The target of a 'continue' (with & without identifier).
  //              Pushed by an IterationStatement.
  //  - kNamed  : The target of a 'break' with a specific identifier.
  //              Pushed by a BlockStatement.
  //  - kOther  : Only used for internal blocks, can never be targeted.
  enum class BlockKind { kRegular, kLoop, kNamed, kOther };

  // One entry in the {block_stack}, see {BlockKind} above for details. Blocks
  // without a label have {kTokenNone} set as their label.
  struct BlockInfo {
    BlockKind kind;
    AsmJsScanner::token_t label;
  };

  // Helper class to make {TempVariable} safe for nesting.
  class TemporaryVariableScope;

  template <typename T>
  class CachedVectors {
   public:
    explicit CachedVectors(Zone* zone) : reusable_vectors_(zone) {}

    Zone* zone() const { return reusable_vectors_.zone(); }

    inline void fill(ZoneVector<T>* vec) {
      if (reusable_vectors_.empty()) return;
      reusable_vectors_.back().swap(*vec);
      reusable_vectors_.pop_back();
      vec->clear();
    }

    inline void reuse(ZoneVector<T>* vec) {
      reusable_vectors_.emplace_back(std::move(*vec));
    }

   private:
    ZoneVector<ZoneVector<T>> reusable_vectors_;
  };

  template <typename T>
  class CachedVector final : public ZoneVector<T> {
   public:
    explicit CachedVector(CachedVectors<T>* cache)
        : ZoneVector<T>(cache->zone()), cache_(cache) {
      cache->fill(this);
    }
    ~CachedVector() { cache_->reuse(this); }

   private:
    CachedVectors<T>* cache_;
  };

  Zone* zone_;
  AsmJsScanner scanner_;
  WasmModuleBuilder* module_builder_;
  WasmFunctionBuilder* current_function_builder_;
  AsmType* return_type_ = nullptr;
  uintptr_t stack_limit_;
  StdlibSet stdlib_uses_;
  base::Vector<VarInfo> global_var_info_;
  base::Vector<VarInfo> local_var_info_;
  size_t num_globals_ = 0;

  CachedVectors<ValueType> cached_valuetype_vectors_{zone_};
  CachedVectors<AsmType*> cached_asm_type_p_vectors_{zone_};
  CachedVectors<AsmJsScanner::token_t> cached_token_t_vectors_{zone_};
  CachedVectors<int32_t> cached_int_vectors_{zone_};

  int function_temp_locals_offset_;
  int function_temp_locals_used_;
  int function_temp_locals_depth_;

  // Error Handling related
  bool failed_ = false;
  const char* failure_message_;
  int failure_location_ = kNoSourcePosition;

  // Module Related.
  AsmJsScanner::token_t stdlib_name_ = kTokenNone;
  AsmJsScanner::token_t foreign_name_ = kTokenNone;
  AsmJsScanner::token_t heap_name_ = kTokenNone;

  static const AsmJsScanner::token_t kTokenNone = 0;

  // Track if parsing a heap assignment.
  bool inside_heap_assignment_ = false;
  AsmType* heap_access_type_ = nullptr;

  ZoneVector<BlockInfo> block_stack_;

  // Types used for stdlib function and their set up.
  AsmType* stdlib_dq2d_;
  AsmType* stdlib_dqdq2d_;
  AsmType* stdlib_i2s_;
  AsmType* stdlib_ii2s_;
  AsmType* stdlib_minmax_;
  AsmType* stdlib_abs_;
  AsmType* stdlib_ceil_like_;
  AsmType* stdlib_fround_;

  // When making calls, the return type is needed to lookup signatures.
  // For `+callsite(..)` or `fround(callsite(..))` use this value to pass
  // along the coercion.
  AsmType* call_coercion_ = nullptr;

  // The source position associated with the above {call_coercion}.
  size_t call_coercion_position_;

  // When making calls, the coercion can also appear in the source stream
  // syntactically "behind" the call site. For `callsite(..)|0` use this
  // value to flag that such a coercion must happen.
  AsmType* call_coercion_deferred_ = nullptr;

  // The source position at which requesting a deferred coercion via the
  // aforementioned {call_coercion_deferred} is allowed.
  size_t call_coercion_deferred_position_;

  // The code position of the last heap access shift by an immediate value.
  // For `heap[expr >> value:NumericLiteral]` this indicates from where to
  // delete code when the expression is used as part of a valid heap access.
  // Will be set to {kNoHeapAccessShift} if heap access shift wasn't matched.
  size_t heap_access_shift_position_;
  uint32_t heap_access_shift_value_;
  static const size_t kNoHeapAccessShift = -1;

  // Used to track the last label we've seen so it can be matched to later
  // statements it's attached to.
  AsmJsScanner::token_t pending_label_ = kTokenNone;

  // Global imports. The list of imported variables that are copied during
  // module instantiation into a corresponding global variable.
  ZoneLinkedList<GlobalImport> global_imports_;

  Zone* zone() { return zone_; }

  inline bool Peek(AsmJsScanner::token_t token) {
    return scanner_.Token() == token;
  }

  inline bool PeekForZero() {
    return (scanner_.IsUnsigned() && scanner_.AsUnsigned() == 0);
  }

  inline bool Check(AsmJsScanner::token_t token) {
    if (scanner_.Token() == token) {
      scanner_.Next();
      return true;
    } else {
      return false;
    }
  }

  inline bool CheckForZero() {
    if (scanner_.IsUnsigned() && scanner_.AsUnsigned() == 0) {
      scanner_.Next();
      return true;
    } else {
      return false;
    }
  }

  inline bool CheckForDouble(double* value) {
    if (scanner_.IsDouble()) {
      *value = scanner_.AsDouble();
      scanner_.Next();
      return true;
    } else {
      return false;
    }
  }

  inline bool CheckForUnsigned(uint32_t* value) {
    if (scanner_.IsUnsigned()) {
      *value = scanner_.AsUnsigned();
      scanner_.Next();
      return true;
    } else {
      return false;
    }
  }

  inline bool CheckForUnsignedBelow(uint32_t limit, uint32_t* value) {
    if (scanner_.IsUnsigned() && scanner_.AsUnsigned() < limit) {
      *value = scanner_.AsUnsigned();
      scanner_.Next();
      return true;
    } else {
      return false;
    }
  }

  inline AsmJsScanner::token_t Consume() {
    AsmJsScanner::token_t ret = scanner_.Token();
    scanner_.Next();
    return ret;
  }

  void SkipSemicolon();

  VarInfo* GetVarInfo(AsmJsScanner::token_t token);
  uint32_t VarIndex(VarInfo* info);
  void DeclareGlobal(VarInfo* info, bool mutable_variable, AsmType* type,
                     ValueType vtype, WasmInitExpr init = WasmInitExpr());
  void DeclareStdlibFunc(VarInfo* info, VarKind kind, AsmType* type);
  void AddGlobalImport(base::Vector<const char> name, AsmType* type,
                       ValueType vtype, bool mutable_variable, VarInfo* info);

  // Allocates a temporary local variable. The given {index} is absolute within
  // the function body, consider using {TemporaryVariableScope} when nesting.
  uint32_t TempVariable(int index);

  // Preserves a copy of the scanner's current identifier string in the zone.
  base::Vector<const char> CopyCurrentIdentifierString();

  // Use to set up block stack layers (including synthetic ones for if-else).
  // Begin/Loop/End below are implemented with these plus code generation.
  void BareBegin(BlockKind kind, AsmJsScanner::token_t label = 0);
  void BareEnd();
  int FindContinueLabelDepth(AsmJsScanner::token_t label);
  int FindBreakLabelDepth(AsmJsScanner::token_t label);

  // Use to set up actual wasm blocks/loops.
  void Begin(AsmJsScanner::token_t label = 0);
  void Loop(AsmJsScanner::token_t label = 0);
  void End();

  void InitializeStdlibTypes();

  FunctionSig* ConvertSignature(AsmType* return_type,
                                const ZoneVector<AsmType*>& params);

  void ValidateModule();            // 6.1 ValidateModule
  void ValidateModuleParameters();  // 6.1 ValidateModule - parameters
  void ValidateModuleVars();        // 6.1 ValidateModule - variables
  void ValidateModuleVar(bool mutable_variable);
  void ValidateModuleVarImport(VarInfo* info, bool mutable_variable);
  void ValidateModuleVarStdlib(VarInfo* info);
  void ValidateModuleVarNewStdlib(VarInfo* info);
  void ValidateModuleVarFromGlobal(VarInfo* info, bool mutable_variable);

  void ValidateExport();         // 6.2 ValidateExport
  void ValidateFunctionTable();  // 6.3 ValidateFunctionTable
  void ValidateFunction();       // 6.4 ValidateFunction
  void ValidateFunctionParams(ZoneVector<AsmType*>* params);
  void ValidateFunctionLocals(size_t param_count,
                              ZoneVector<ValueType>* locals);
  void ValidateStatement();              // 6.5 ValidateStatement
  void Block();                          // 6.5.1 Block
  void ExpressionStatement();            // 6.5.2 ExpressionStatement
  void EmptyStatement();                 // 6.5.3 EmptyStatement
  void IfStatement();                    // 6.5.4 IfStatement
  void ReturnStatement();                // 6.5.5 ReturnStatement
  bool IterationStatement();             // 6.5.6 IterationStatement
  void WhileStatement();                 // 6.5.6 IterationStatement - while
  void DoStatement();                    // 6.5.6 IterationStatement - do
  void ForStatement();                   // 6.5.6 IterationStatement - for
  void BreakStatement();                 // 6.5.7 BreakStatement
  void ContinueStatement();              // 6.5.8 ContinueStatement
  void LabelledStatement();              // 6.5.9 LabelledStatement
  void SwitchStatement();                // 6.5.10 SwitchStatement
  void ValidateCase();                   // 6.6. ValidateCase
  void ValidateDefault();                // 6.7 ValidateDefault
  AsmType* ValidateExpression();         // 6.8 ValidateExpression
  AsmType* Expression(AsmType* expect);  // 6.8.1 Expression
  AsmType* NumericLiteral();             // 6.8.2 NumericLiteral
  AsmType* Identifier();                 // 6.8.3 Identifier
  AsmType* CallExpression();             // 6.8.4 CallExpression
  AsmType* MemberExpression();           // 6.8.5 MemberExpression
  AsmType* AssignmentExpression();       // 6.8.6 AssignmentExpression
  AsmType* UnaryExpression();            // 6.8.7 UnaryExpression
  AsmType* MultiplicativeExpression();   // 6.8.8 MultiplicativeExpression
  AsmType* AdditiveExpression();         // 6.8.9 AdditiveExpression
  AsmType* ShiftExpression();            // 6.8.10 ShiftExpression
  AsmType* RelationalExpression();       // 6.8.11 RelationalExpression
  AsmType* EqualityExpression();         // 6.8.12 EqualityExpression
  AsmType* BitwiseANDExpression();       // 6.8.13 BitwiseANDExpression
  AsmType* BitwiseXORExpression();       // 6.8.14 BitwiseXORExpression
  AsmType* BitwiseORExpression();        // 6.8.15 BitwiseORExpression
  AsmType* ConditionalExpression();      // 6.8.16 ConditionalExpression
  AsmType* ParenthesizedExpression();    // 6.8.17 ParenthesiedExpression
  AsmType* ValidateCall();               // 6.9 ValidateCall
  bool PeekCall();                       // 6.9 ValidateCall - helper
  void ValidateHeapAccess();             // 6.10 ValidateHeapAccess
  void ValidateFloatCoercion();          // 6.11 ValidateFloatCoercion

  // Used as part of {ForStatement}. Scans forward to the next `)` in order to
  // skip over the third expression in a for-statement. This is one piece that
  // makes this parser not be a pure single-pass.
  void ScanToClosingParenthesis();

  // Used as part of {SwitchStatement}. Collects all case labels in the current
  // switch-statement, then resets the scanner position. This is one piece that
  // makes this parser not be a pure single-pass.
  void GatherCases(ZoneVector<int32_t>* cases);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_ASMJS_ASM_PARSER_H_
