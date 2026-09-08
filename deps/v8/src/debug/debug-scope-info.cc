// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scope-info.h"

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"
#include "src/base/bit-field.h"
#include "src/base/numerics/safe_conversions.h"
#include "src/base/vector.h"
#include "src/common/globals.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/string-inl.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// ===========================================================================
// ByteArray: numeric_data
// ===========================================================================
// +-------------------------------------------------------------------------+
// | Header (4 bytes):                                                       |
// |   int32_t scope_count                                                   |
// +-------------------------------------------------------------------------+
// | Offset Table (scope_count * 4 bytes):                                   |
// |   uint32_t scope_offsets[scope_count]                                   |
// +-------------------------------------------------------------------------+
// | Scope Record 0 (at scope_offsets[0]):                                   |
// |   +0: int32_t  start_position                                           |
// |   +4: int32_t  end_position                                             |
// |   +8: int32_t  parent_scope_index (-1 for root script scope)            |
// |  +12: uint16_t flags (ScopeType, IsHidden, NeedsContext, HasSibling, etc)|
// |  +14: uint16_t var_count                                                |
// |  --- Dynamic Optional Fields (present conditionally based on flags) --- |
// |  [+16: int32_t  next_sibling_index]        (if HasSiblingBit is set)    |
// |  [+..: int32_t  context_id]                (if NeedsContextBit is set)  |
// |  [+..: uint16_t receiver_allocation_info]  (if HasThisDeclBit is set)   |
// |  [+..: uint16_t arguments_allocation_info] (if HasArgumentsBit is set)  |
// |  [+..: uint16_t + int32_t function_var]    (if HasFunctionVarBit is set)|
// |  --- Variables Array (var_count entries, 12 bytes each) ---             |
// |  [DebugVariableEntry 0]:                                                |
// |     +0: int16_t  slot_index                                             |
// |     +2: uint16_t location_mode_flags                                    |
// |     +4: int32_t  initializer_position                                   |
// |     +8: int32_t  name_index (into string_table FixedArray)              |
// |  [DebugVariableEntry 1]...                                              |
// +-------------------------------------------------------------------------+
// | Scope Record 1 (at scope_offsets[1])...                                 |
// +-------------------------------------------------------------------------+

namespace {

struct ScopeRecord {
  int32_t start_position;
  int32_t end_position;
  int32_t parent_scope_index;
  uint16_t flags;
  uint16_t var_count;
};

static_assert(std::is_trivial_v<ScopeRecord>);
static_assert(std::is_standard_layout_v<ScopeRecord>);
// Ensure ScopeRecord has no padding. Update when adding new fields.
static_assert(sizeof(ScopeRecord) == 16);

struct DebugVariableEntry {
  int16_t slot_index;
  uint16_t location_mode_flags;
  int32_t initializer_position;
  int32_t name_index;
};

static_assert(std::is_trivial_v<DebugVariableEntry>);
static_assert(std::is_standard_layout_v<DebugVariableEntry>);
static_assert(sizeof(DebugVariableEntry) == 12);

using ScopeTypeBits = base::BitField<ScopeType, 0, 4, uint16_t>;
using HasChildrenBit = ScopeTypeBits::Next<bool, 1>;
using HasSiblingBit = HasChildrenBit::Next<bool, 1>;
using IsHiddenBit = HasSiblingBit::Next<bool, 1>;
using LanguageModeBit = IsHiddenBit::Next<LanguageMode, 1>;
using IsArrowScopeBit = LanguageModeBit::Next<bool, 1>;
using HasThisDeclarationBit = IsArrowScopeBit::Next<bool, 1>;
using HasThisReferenceBit = HasThisDeclarationBit::Next<bool, 1>;
using HasSimpleParametersBit = HasThisReferenceBit::Next<bool, 1>;
using SloppyEvalCanExtendVarsBit = HasSimpleParametersBit::Next<bool, 1>;
using NeedsContextBit = SloppyEvalCanExtendVarsBit::Next<bool, 1>;
using HasArgumentsBit = NeedsContextBit::Next<bool, 1>;
using HasFunctionVarBit = HasArgumentsBit::Next<bool, 1>;
static_assert(HasFunctionVarBit::kLastUsedBit < 16);

using VariableLocationBits = base::BitField<VariableLocation, 0, 4, uint16_t>;
using VariableModeBits = VariableLocationBits::Next<VariableMode, 4>;
using IsSyntheticBit = VariableModeBits::Next<bool, 1>;
using IsReceiverBit = IsSyntheticBit::Next<bool, 1>;
static_assert(IsReceiverBit::kLastUsedBit < 16);

// Encodes receiver, arguments, or function variable allocation info into a
// 16-bit word:
// - Bits 0..1: VariableAllocationInfo (NONE, STACK, CONTEXT, UNUSED)
// - Bits 2..15: 14-bit signed slot/parameter index
//
// In V8, stack-allocated receiver variables have parameter index -1 (allocated
// via DeclarationScope::AllocateReceiver). Therefore, the 14-bit index field is
// treated as a signed two's-complement integer, requiring sign-extension upon
// decoding.
uint16_t EncodeAllocInfo(VariableAllocationInfo info, int index) {
  DCHECK_GE(index, -1);
  DCHECK_LE(index, 0x1FFF);
  uint16_t raw_index = static_cast<uint16_t>(index & 0x3FFF);
  return static_cast<uint16_t>(info) | (raw_index << 2);
}

std::pair<VariableAllocationInfo, int> DecodeAllocInfo(uint16_t val) {
  int raw_index = static_cast<int>(val >> 2);
  // Sign-extend 14-bit signed integer to int (e.g. 0x3FFF -> -1).
  if (raw_index & 0x2000) raw_index |= ~0x3FFF;
  return {static_cast<VariableAllocationInfo>(val & 3), raw_index};
}

int32_t GetScopeCount(Tagged<DebugScriptScopeInfo> info) {
  Tagged<ByteArray> bytes = info->numeric_data();
  DCHECK_GE(bytes->length().value(), kInt32Size);
  return base::ReadUnalignedValue<int32_t>(bytes->begin());
}

uint32_t GetScopeOffset(Tagged<DebugScriptScopeInfo> info, int scope_index) {
  CHECK_GE(scope_index, 0);
  CHECK_LT(scope_index, GetScopeCount(info));
  const uint8_t* ptr =
      info->numeric_data()->begin() + kInt32Size + scope_index * kUInt32Size;
  return base::ReadUnalignedValue<uint32_t>(ptr);
}

class ByteArrayWriter {
 public:
  explicit ByteArrayWriter(Address cursor) : cursor_(cursor) {}

  template <typename T>
  void Write(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    base::WriteUnalignedValue<T>(cursor_, value);
    cursor_ += sizeof(T);
  }

  Address cursor() const { return cursor_; }

 private:
  Address cursor_;
};

}  // namespace

const uint8_t* DebugScriptScope::payload() const {
  return info_->numeric_data()->begin() + offset_;
}

uint16_t DebugScriptScope::flags() const {
  return base::ReadUnalignedValue<ScopeRecord>(payload()).flags;
}

DebugScriptScope DebugScriptScope::FromIndex(
    DirectHandle<DebugScriptScopeInfo> info, int scope_index) {
  CHECK_GE(scope_index, 0);
  CHECK_LT(scope_index, GetScopeCount(*info));
  uint32_t offset = GetScopeOffset(*info, scope_index);
  return DebugScriptScope(info, scope_index, offset);
}

std::optional<DebugScriptScope> DebugScriptScope::parent() const {
  int parent_idx = parent_index();
  if (parent_idx == -1) return std::nullopt;
  return FromIndex(info_, parent_idx);
}

std::optional<DebugScriptScope> DebugScriptScope::first_child() const {
  if (!HasChildrenBit::decode(flags())) return std::nullopt;
  return FromIndex(info_, scope_index_ + 1);
}

std::optional<DebugScriptScope> DebugScriptScope::next_sibling() const {
  if (!HasSiblingBit::decode(flags())) return std::nullopt;
  const uint8_t* sibling_ptr = payload() + next_sibling_offset();
  int sibling_idx = base::ReadUnalignedValue<int32_t>(sibling_ptr);
  return FromIndex(info_, sibling_idx);
}

int DebugScriptScope::start_position() const {
  return base::ReadUnalignedValue<ScopeRecord>(payload()).start_position;
}

int DebugScriptScope::end_position() const {
  return base::ReadUnalignedValue<ScopeRecord>(payload()).end_position;
}

int DebugScriptScope::parent_index() const {
  return base::ReadUnalignedValue<ScopeRecord>(payload()).parent_scope_index;
}

ScopeType DebugScriptScope::scope_type() const {
  return ScopeTypeBits::decode(flags());
}

bool DebugScriptScope::is_script_scope() const {
  return scope_type() == ScopeType::SCRIPT_SCOPE ||
         scope_type() == ScopeType::REPL_MODE_SCOPE;
}

bool DebugScriptScope::is_function_scope() const {
  return scope_type() == ScopeType::FUNCTION_SCOPE;
}

bool DebugScriptScope::is_block_scope() const {
  return scope_type() == ScopeType::BLOCK_SCOPE ||
         scope_type() == ScopeType::CLASS_SCOPE;
}

bool DebugScriptScope::is_declaration_scope() const {
  return is_script_scope() || is_function_scope() ||
         scope_type() == ScopeType::MODULE_SCOPE ||
         scope_type() == ScopeType::EVAL_SCOPE;
}

LanguageMode DebugScriptScope::language_mode() const {
  return LanguageModeBit::decode(flags());
}

bool DebugScriptScope::is_arrow_scope() const {
  return IsArrowScopeBit::decode(flags());
}

bool DebugScriptScope::is_class_scope() const {
  return scope_type() == ScopeType::CLASS_SCOPE;
}

bool DebugScriptScope::is_with_scope() const {
  return scope_type() == ScopeType::WITH_SCOPE;
}

bool DebugScriptScope::is_module_scope() const {
  return scope_type() == ScopeType::MODULE_SCOPE;
}

bool DebugScriptScope::is_eval_scope() const {
  return scope_type() == ScopeType::EVAL_SCOPE;
}

bool DebugScriptScope::is_catch_scope() const {
  return scope_type() == ScopeType::CATCH_SCOPE;
}

bool DebugScriptScope::is_repl_mode_scope() const {
  return scope_type() == ScopeType::REPL_MODE_SCOPE;
}

bool DebugScriptScope::is_hidden() const {
  return IsHiddenBit::decode(flags());
}

bool DebugScriptScope::has_this_declaration() const {
  return HasThisDeclarationBit::decode(flags());
}

bool DebugScriptScope::has_this_reference() const {
  return HasThisReferenceBit::decode(flags());
}

bool DebugScriptScope::has_simple_parameters() const {
  return HasSimpleParametersBit::decode(flags());
}

bool DebugScriptScope::has_arguments() const {
  return HasArgumentsBit::decode(flags());
}

bool DebugScriptScope::has_function_variable() const {
  return HasFunctionVarBit::decode(flags());
}

bool DebugScriptScope::sloppy_eval_can_extend_vars() const {
  return SloppyEvalCanExtendVarsBit::decode(flags());
}

bool DebugScriptScope::needs_context() const {
  return NeedsContextBit::decode(flags());
}

size_t DebugScriptScope::next_sibling_offset() const {
  return sizeof(ScopeRecord);
}

size_t DebugScriptScope::context_id_offset() const {
  return next_sibling_offset() +
         (HasSiblingBit::decode(flags()) ? kInt32Size : 0);
}

size_t DebugScriptScope::receiver_info_offset() const {
  return context_id_offset() +
         (NeedsContextBit::decode(flags()) ? kInt32Size : 0);
}

size_t DebugScriptScope::arguments_info_offset() const {
  return receiver_info_offset() +
         (HasThisDeclarationBit::decode(flags()) ? kUInt16Size : 0);
}

size_t DebugScriptScope::function_variable_offset() const {
  return arguments_info_offset() +
         (HasArgumentsBit::decode(flags()) ? kUInt16Size : 0);
}

size_t DebugScriptScope::variables_offset() const {
  return function_variable_offset() +
         (HasFunctionVarBit::decode(flags()) ? (kUInt16Size + kInt32Size) : 0);
}

size_t DebugScriptScope::record_size() const {
  return variables_offset() + variable_count() * sizeof(DebugVariableEntry);
}

int DebugScriptScope::unique_id_in_script() const {
  if (!needs_context()) return -3;
  return base::ReadUnalignedValue<int32_t>(payload() + context_id_offset());
}

std::pair<VariableAllocationInfo, int> DebugScriptScope::receiver_info() const {
  if (!has_this_declaration()) return {VariableAllocationInfo::NONE, -1};
  return DecodeAllocInfo(
      base::ReadUnalignedValue<uint16_t>(payload() + receiver_info_offset()));
}

std::pair<VariableAllocationInfo, int> DebugScriptScope::arguments_info()
    const {
  if (!has_arguments()) return {VariableAllocationInfo::NONE, -1};
  return DecodeAllocInfo(
      base::ReadUnalignedValue<uint16_t>(payload() + arguments_info_offset()));
}

const uint8_t* DebugScriptScope::function_variable_payload() const {
  if (!has_function_variable()) return nullptr;
  return payload() + function_variable_offset();
}

std::pair<VariableAllocationInfo, int>
DebugScriptScope::function_variable_info() const {
  const uint8_t* ptr = function_variable_payload();
  if (!ptr) return {VariableAllocationInfo::NONE, -1};
  return DecodeAllocInfo(base::ReadUnalignedValue<uint16_t>(ptr));
}

Tagged<String> DebugScriptScope::function_variable_name() const {
  const uint8_t* ptr = function_variable_payload();
  if (!ptr) return {};
  int32_t name_index = base::ReadUnalignedValue<int32_t>(ptr + kUInt16Size);
  DCHECK_GE(name_index, 0);
  DCHECK_LT(static_cast<uint32_t>(name_index),
            info_->string_table()->length().value());
  return Cast<String>(info_->string_table()->get(name_index));
}

int DebugScriptScope::variable_count() const {
  return base::ReadUnalignedValue<ScopeRecord>(payload()).var_count;
}

const uint8_t* DebugScriptScope::variables_payload() const {
  return payload() + variables_offset();
}

DebugVariableInfo DebugScriptScope::variable(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, variable_count());
  const uint8_t* entry_ptr =
      variables_payload() + index * sizeof(DebugVariableEntry);
  DebugVariableEntry entry =
      base::ReadUnalignedValue<DebugVariableEntry>(entry_ptr);
  Tagged<String> name;
  if (entry.name_index >= 0) {
    DCHECK_LT(static_cast<uint32_t>(entry.name_index),
              info_->string_table()->length().value());
    name = Cast<String>(info_->string_table()->get(entry.name_index));
  }
  return DebugVariableInfo{
      .name = name,
      .location = VariableLocationBits::decode(entry.location_mode_flags),
      .index = entry.slot_index,
      .mode = VariableModeBits::decode(entry.location_mode_flags),
      .initializer_position = entry.initializer_position,
      .is_synthetic = IsSyntheticBit::decode(entry.location_mode_flags),
      .is_receiver = IsReceiverBit::decode(entry.location_mode_flags),
  };
}

Handle<DebugScriptScopeInfo> SerializeDebugScriptScopeInfo(
    Isolate* isolate, DeclarationScope* script_scope) {
  DCHECK_NOT_NULL(script_scope);
  DCHECK(script_scope->is_script_scope());

  Zone* zone = script_scope->zone();
  ZoneVector<const AstRawString*> string_table(zone);
  ZoneAbslFlatHashMap<const AstRawString*, int32_t> string_map(zone);

  auto get_or_insert_string = [&](const AstRawString* raw_name) -> int32_t {
    if (raw_name == nullptr) return -1;
    auto it = string_map.find(raw_name);
    if (it != string_map.end()) return it->second;
    int32_t index = base::checked_cast<int32_t>(string_table.size());
    string_map.emplace(raw_name, index);
    string_table.push_back(raw_name);
    return index;
  };

  std::vector<Scope*> all_scopes;
  std::unordered_map<Scope*, int32_t> scope_to_index;

  auto collect = [&](auto& self, Scope* scope) -> void {
    int32_t index = base::checked_cast<int32_t>(all_scopes.size());
    all_scopes.push_back(scope);
    scope_to_index.emplace(scope, index);
    for (Scope* inner = scope->inner_scope(); inner != nullptr;
         inner = inner->sibling()) {
      self(self, inner);
    }
  };
  collect(collect, script_scope);

  auto find_scope_index = [&](Scope* s) -> int32_t {
    if (s == nullptr) return -1;
    auto it = scope_to_index.find(s);
    return it != scope_to_index.end() ? it->second : -1;
  };

  // Stage 1: Pre-calculate exact required byte size and scope offsets.
  size_t total_size = kInt32Size + all_scopes.size() * kUInt32Size;
  std::vector<uint32_t> offsets;
  offsets.reserve(all_scopes.size());

  for (size_t i = 0; i < all_scopes.size(); ++i) {
    offsets.push_back(base::checked_cast<uint32_t>(total_size));
    total_size += sizeof(ScopeRecord);
    if (all_scopes[i]->sibling() != nullptr) {
      total_size += kInt32Size;
    }
    if (all_scopes[i]->NeedsContext()) {
      total_size += kInt32Size;
    }
    if (all_scopes[i]->is_declaration_scope() &&
        all_scopes[i]->AsDeclarationScope()->has_this_declaration()) {
      total_size += kUInt16Size;
    }
    if (all_scopes[i]->is_declaration_scope() &&
        all_scopes[i]->AsDeclarationScope()->arguments() != nullptr) {
      total_size += kUInt16Size;
    }
    if (all_scopes[i]->is_declaration_scope() &&
        all_scopes[i]->AsDeclarationScope()->function_var() != nullptr) {
      total_size += kUInt16Size + kInt32Size;
    }
    uint16_t var_count = base::checked_cast<uint16_t>(std::distance(
        all_scopes[i]->locals()->begin(), all_scopes[i]->locals()->end()));
    total_size += var_count * sizeof(DebugVariableEntry);
  }

  // Stage 2: Allocate a ByteArray and write directly into it with
  // ByteArrayWriter.
  Handle<ByteArray> byte_array = isolate->factory()->NewByteArray(
      base::checked_cast<uint32_t>(total_size), AllocationType::kOld);

  ByteArrayWriter header_writer(reinterpret_cast<Address>(byte_array->begin()));
  header_writer.Write<int32_t>(base::checked_cast<int32_t>(all_scopes.size()));

  for (size_t i = 0; i < all_scopes.size(); ++i) {
    header_writer.Write<uint32_t>(offsets[i]);
  }

  for (size_t i = 0; i < all_scopes.size(); ++i) {
    Address record =
        reinterpret_cast<Address>(byte_array->begin()) + offsets[i];
    Scope* scope = all_scopes[i];

    int32_t next_sibling = find_scope_index(scope->sibling());
    bool has_sibling = next_sibling != -1;
    bool needs_context = scope->NeedsContext();
    bool has_this_decl = false;
    bool has_arguments = false;
    bool has_function_var = false;

    uint16_t flags = 0;
    flags = ScopeTypeBits::update(flags, scope->scope_type());
    flags = HasChildrenBit::update(flags, scope->inner_scope() != nullptr);
    flags = HasSiblingBit::update(flags, has_sibling);
    flags = IsHiddenBit::update(flags, scope->is_hidden());
    flags = LanguageModeBit::update(flags, scope->language_mode());
    flags = HasThisReferenceBit::update(flags, scope->HasThisReference());
    flags = NeedsContextBit::update(flags, needs_context);
    if (scope->is_declaration_scope()) {
      DeclarationScope* decl_scope = scope->AsDeclarationScope();
      has_this_decl = decl_scope->has_this_declaration();
      has_arguments = decl_scope->arguments() != nullptr;
      has_function_var = decl_scope->function_var() != nullptr;
      flags = IsArrowScopeBit::update(flags, decl_scope->is_arrow_scope());
      flags = HasThisDeclarationBit::update(flags, has_this_decl);
      flags = HasSimpleParametersBit::update(
          flags, decl_scope->has_simple_parameters());
      flags = SloppyEvalCanExtendVarsBit::update(
          flags, decl_scope->sloppy_eval_can_extend_vars());
    }
    flags = HasArgumentsBit::update(flags, has_arguments);
    flags = HasFunctionVarBit::update(flags, has_function_var);

    uint16_t var_count = base::checked_cast<uint16_t>(
        std::distance(scope->locals()->begin(), scope->locals()->end()));

    ByteArrayWriter writer(record);
    writer.Write<ScopeRecord>(ScopeRecord{
        .start_position = scope->start_position(),
        .end_position = scope->end_position(),
        .parent_scope_index = find_scope_index(scope->outer_scope()),
        .flags = flags,
        .var_count = var_count,
    });

    if (has_sibling) {
      writer.Write<int32_t>(next_sibling);
    }
    if (needs_context) {
      // We only need the context ID to match DebugScopeInfo against runtime
      // ScopeInfo. V8 omits runtime ScopeInfo for any scope that doesn't need a
      // context so we don't need to waste the bytes.
      writer.Write<int32_t>(scope->UniqueIdInScript());
    }
    if (has_this_decl) {
      Variable* var = scope->AsDeclarationScope()->receiver();
      VariableAllocationInfo info = var->location() == VariableLocation::CONTEXT
                                        ? VariableAllocationInfo::CONTEXT
                                        : VariableAllocationInfo::STACK;
      writer.Write<uint16_t>(EncodeAllocInfo(info, var->index()));
    }
    if (has_arguments) {
      Variable* var = scope->AsDeclarationScope()->arguments();
      VariableAllocationInfo info = var->location() == VariableLocation::CONTEXT
                                        ? VariableAllocationInfo::CONTEXT
                                        : VariableAllocationInfo::STACK;
      writer.Write<uint16_t>(EncodeAllocInfo(info, var->index()));
    }
    if (has_function_var) {
      Variable* var = scope->AsDeclarationScope()->function_var();
      VariableAllocationInfo info = var->location() == VariableLocation::CONTEXT
                                        ? VariableAllocationInfo::CONTEXT
                                        : VariableAllocationInfo::STACK;
      writer.Write<uint16_t>(EncodeAllocInfo(info, var->index()));
      int32_t name_index = get_or_insert_string(var->raw_name());
      writer.Write<int32_t>(name_index);
    }

    for (Variable* var : *scope->locals()) {
      const AstRawString* raw = var->raw_name();
      // LINT.IfChange(VariableIsSynthetic)
      // Keep in sync with ScopeInfo::VariableIsSynthetic() in
      // src/objects/scope-info.cc.
      bool is_synthetic =
          raw != nullptr &&
          (raw->IsEmpty() || raw->FirstCharacter() == '.' ||
           raw->IsPrivateName() || raw->IsOneByteEqualTo("this"));
      // LINT.ThenChange(/src/objects/scope-info.cc:VariableIsSynthetic)
      // LINT.IfChange(VariableIsReceiver)
      // Keep in sync with Variable::IsReceiver() in src/ast/variables.h.
      // Variable::IsReceiver() asserts IsParameter() in debug builds.
      bool is_receiver = var->IsParameter() && var->IsReceiver();
      // LINT.ThenChange(/src/ast/variables.h:VariableIsReceiver)
      uint16_t var_flags = 0;
      var_flags = VariableLocationBits::update(var_flags, var->location());
      var_flags = VariableModeBits::update(var_flags, var->mode());
      var_flags = IsSyntheticBit::update(var_flags, is_synthetic);
      var_flags = IsReceiverBit::update(var_flags, is_receiver);

      writer.Write<DebugVariableEntry>(DebugVariableEntry{
          .slot_index = base::checked_cast<int16_t>(var->index()),
          .location_mode_flags = var_flags,
          .initializer_position = var->initializer_position(),
          .name_index = get_or_insert_string(var->raw_name()),
      });
    }

    size_t expected_size =
        (i + 1 < all_scopes.size() ? offsets[i + 1] : total_size) - offsets[i];
    CHECK_EQ(writer.cursor(), record + expected_size);
  }

  DirectHandle<FixedArray> final_string_table;
  if (string_table.empty()) {
    final_string_table = isolate->factory()->empty_fixed_array();
  } else {
    uint32_t string_count = base::checked_cast<uint32_t>(string_table.size());
    Handle<FixedArray> table =
        isolate->factory()->NewFixedArray(string_count, AllocationType::kOld);
    for (uint32_t i = 0; i < string_count; ++i) {
      table->set(i, *string_table[i]->string());
    }
    final_string_table = table;
  }
  return isolate->factory()->NewDebugScriptScopeInfo(byte_array,
                                                     final_string_table);
}

#ifdef VERIFY_HEAP
// DebugScriptScopeInfo::DebugScriptScopeInfoVerify is placed here instead of in
// objects-debug.cc to keep the exact layout of numeric_data local to
// debug-scope-info.cc.
void DebugScriptScopeInfo::DebugScriptScopeInfoVerify(Isolate* isolate) {
  CHECK(Is<Struct>(this));
  CHECK(Is<DebugScriptScopeInfo>(this));
  Object::VerifyPointer(isolate, numeric_data_.load());
  Object::VerifyPointer(isolate, string_table_.load());
  CHECK(IsFixedArray(string_table()));

  Tagged<ByteArray> bytes = numeric_data();
  CHECK_GE(bytes->length().value(), kInt32Size);
  int scope_count = GetScopeCount(this);
  CHECK_GE(scope_count, 0);

  size_t header_and_table_size = kInt32Size + scope_count * kUInt32Size;
  CHECK_GE(static_cast<size_t>(bytes->length().value()), header_and_table_size);

  HandleScope handle_scope(isolate);
  DirectHandle<DebugScriptScopeInfo> info_handle(this, isolate);

  for (int i = 0; i < scope_count; ++i) {
    uint32_t offset = GetScopeOffset(this, i);
    CHECK_EQ(offset % kUInt16Size, 0);
    CHECK_GE(offset, header_and_table_size);
    DebugScriptScope scope = DebugScriptScope::FromIndex(info_handle, i);
    CHECK_EQ(scope.scope_index(), i);
    CHECK_LE(scope.start_position(), scope.end_position());

    size_t record_size = scope.record_size();
    CHECK_LE(offset + record_size,
             static_cast<size_t>(bytes->length().value()));

    if (scope.needs_context()) {
      CHECK_GE(scope.unique_id_in_script(), -2);
    } else {
      CHECK_EQ(scope.unique_id_in_script(), -3);
    }

    if (!scope.has_this_declaration()) {
      CHECK_EQ(scope.receiver_info(),
               (std::pair{VariableAllocationInfo::NONE, -1}));
    }

    if (!scope.has_arguments()) {
      CHECK_EQ(scope.arguments_info(),
               (std::pair{VariableAllocationInfo::NONE, -1}));
    }

    if (!scope.has_function_variable()) {
      CHECK_EQ(scope.function_variable_info(),
               (std::pair{VariableAllocationInfo::NONE, -1}));
      CHECK(scope.function_variable_name().is_null());
    } else {
      CHECK(!scope.function_variable_name().is_null());
      const uint8_t* ptr = scope.function_variable_payload();
      CHECK_NOT_NULL(ptr);
      int32_t name_index = base::ReadUnalignedValue<int32_t>(ptr + kUInt16Size);
      CHECK_GE(name_index, 0);
      CHECK_LT(static_cast<uint32_t>(name_index),
               string_table()->length().value());
      CHECK(IsString(string_table()->get(name_index)));
    }

    CHECK_EQ(scope.variable_count(),
             base::ReadUnalignedValue<ScopeRecord>(scope.payload()).var_count);
    for (int v = 0; v < scope.variable_count(); ++v) {
      DebugVariableInfo var = scope.variable(v);
      if (!var.name.is_null()) {
        CHECK(IsString(var.name));
      }
    }

    if (i == 0) {
      CHECK(!scope.parent().has_value());
    } else {
      CHECK(scope.parent().has_value());
      CHECK_GE(scope.parent()->scope_index(), 0);
      CHECK_LT(scope.parent()->scope_index(), i);
    }

    if (auto first_child = scope.first_child()) {
      CHECK_LT(i + 1, scope_count);
      CHECK_EQ(first_child->scope_index(), i + 1);
      CHECK(first_child->parent().has_value());
      CHECK_EQ(first_child->parent()->scope_index(), i);
    }

    if (auto sibling = scope.next_sibling()) {
      CHECK_GT(sibling->scope_index(), i);
      CHECK_LT(sibling->scope_index(), scope_count);
      if (i == 0) {
        CHECK(!sibling->parent().has_value());
      } else {
        CHECK(sibling->parent().has_value());
        CHECK_EQ(sibling->parent()->scope_index(),
                 scope.parent()->scope_index());
      }
    }
  }
}
#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8
