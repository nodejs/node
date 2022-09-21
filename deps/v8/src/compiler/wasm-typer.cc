// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-typer.h"

#include "src/base/logging.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/utils/utils.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...) \
  if (v8_flags.trace_wasm_typer) PrintF(__VA_ARGS__);

WasmTyper::WasmTyper(Editor* editor, MachineGraph* mcgraph,
                     uint32_t function_index)
    : AdvancedReducer(editor),
      function_index_(function_index),
      mcgraph_(mcgraph),
      graph_zone_(mcgraph->graph()->zone()) {}

namespace {
bool AllInputsTyped(Node* node) {
  for (int i = 0; i < node->op()->ValueInputCount(); i++) {
    if (!NodeProperties::IsTyped(NodeProperties::GetValueInput(node, i))) {
      return false;
    }
  }
  return true;
}

// Traverse the fields of a struct until we find one at offset equal to
// {offset}, and return its type.
// If we are in a 32-bit platform, the code has undergone int64 lowering:
// loads from i64 fields have been transformed into a pair of i32 loads. The
// first load has the offset of the original field, and the second one has
// an offset which is greater by size of i32.
// TODO(manoskouk): Improve this.
wasm::ValueType StructFieldFromOffset(const wasm::StructType* type,
                                      uint32_t offset, bool is_32) {
  for (uint32_t index = 0; index < type->field_count(); index++) {
    uint32_t field_offset = wasm::ObjectAccess::ToTagged(
        WasmStruct::kHeaderSize + type->field_offset(index));
    if (is_32 && type->field(index) == wasm::kWasmI64 &&
        field_offset + wasm::kWasmI32.value_kind_size() == offset) {
      return wasm::kWasmI32;
    }
    if (field_offset == offset) {
      wasm::ValueType field_type = type->field(index);
      return is_32 && field_type == wasm::kWasmI64 ? wasm::kWasmI32
                                                   : field_type.Unpacked();
    }
  }
  return wasm::kWasmBottom;
}

}  // namespace

Reduction WasmTyper::Reduce(Node* node) {
  using TypeInModule = wasm::TypeInModule;
  TypeInModule computed_type;
  switch (node->opcode()) {
    case IrOpcode::kTypeGuard: {
      if (!AllInputsTyped(node)) return NoChange();
      TypeInModule guarded_type = TypeGuardTypeOf(node->op()).AsWasm();
      TypeInModule input_type =
          NodeProperties::GetType(NodeProperties::GetValueInput(node, 0))
              .AsWasm();
      // Note: The intersection type might be bottom. In this case, we are in a
      // dead branch: Type this node as bottom and wait for the
      // WasmGCOperatorReducer to remove it.
      computed_type = wasm::Intersection(guarded_type, input_type);
      break;
    }
    case IrOpcode::kWasmTypeCast: {
      if (!AllInputsTyped(node)) return NoChange();
      TypeInModule object_type =
          NodeProperties::GetType(NodeProperties::GetValueInput(node, 0))
              .AsWasm();
      TypeInModule rtt_type =
          NodeProperties::GetType(NodeProperties::GetValueInput(node, 1))
              .AsWasm();
      wasm::ValueType to_type =
          wasm::ValueType::RefNull(rtt_type.type.ref_index());
      computed_type = wasm::Intersection(object_type.type, to_type,
                                         object_type.module, rtt_type.module);
      break;
    }
    case IrOpcode::kAssertNotNull: {
      {
        Node* object = NodeProperties::GetValueInput(node, 0);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);

        // Optimize the common pattern where a TypeCast is followed by an
        // AssertNotNull: Reverse the order of these operations, as this will
        // unlock more optimizations later.
        // We are implementing this in the typer so we can retype the nodes.
        while (control->opcode() == IrOpcode::kWasmTypeCast &&
               effect == object && control == object &&
               !NodeProperties::GetType(object).AsWasm().type.is_bottom()) {
          Node* initial_object = NodeProperties::GetValueInput(object, 0);
          Node* previous_control = NodeProperties::GetControlInput(object);
          Node* previous_effect = NodeProperties::GetEffectInput(object);
          ReplaceWithValue(node, object);
          node->ReplaceInput(NodeProperties::FirstValueIndex(node),
                             initial_object);
          node->ReplaceInput(NodeProperties::FirstEffectIndex(node),
                             previous_effect);
          node->ReplaceInput(NodeProperties::FirstControlIndex(node),
                             previous_control);
          object->ReplaceInput(NodeProperties::FirstValueIndex(object), node);
          object->ReplaceInput(NodeProperties::FirstEffectIndex(object), node);
          object->ReplaceInput(NodeProperties::FirstControlIndex(object), node);
          Revisit(node);
          Revisit(object);
          object = initial_object;
          control = previous_control;
          effect = previous_effect;
        }
      }

      if (!AllInputsTyped(node)) return NoChange();
      TypeInModule object_type =
          NodeProperties::GetType(NodeProperties::GetValueInput(node, 0))
              .AsWasm();
      computed_type = {object_type.type.AsNonNull(), object_type.module};
      break;
    }
    case IrOpcode::kPhi: {
      if (!AllInputsTyped(node)) {
        bool is_loop_phi =
            NodeProperties::GetControlInput(node)->opcode() == IrOpcode::kLoop;
        // For a merge phi, we need all inputs to be typed.
        if (!is_loop_phi) return NoChange();
        // For a loop phi, we can forward the non-recursive-input type. We can
        // recompute the type when the rest of the inputs' types are computed.
        Node* non_recursive_input = NodeProperties::GetValueInput(node, 0);
        if (!NodeProperties::IsTyped(non_recursive_input)) return NoChange();
        computed_type = NodeProperties::GetType(non_recursive_input).AsWasm();
        TRACE("function: %d, loop phi node: %d, type: %s\n", function_index_,
              node->id(), computed_type.type.name().c_str());
        break;
      }
      computed_type =
          NodeProperties::GetType(NodeProperties::GetValueInput(node, 0))
              .AsWasm();
      for (int i = 1; i < node->op()->ValueInputCount(); i++) {
        Node* input = NodeProperties::GetValueInput(node, i);
        TypeInModule input_type = NodeProperties::GetType(input).AsWasm();
        // We do not want union of types from unreachable branches.
        if (!input_type.type.is_bottom()) {
          computed_type = wasm::Union(computed_type, input_type);
        }
      }
      TRACE(
          "function: %d, phi node: %d, input#: %d, input0:%d:%s, input1:%d:%s, "
          "type: %s\n",
          function_index_, node->id(), node->op()->ValueInputCount(),
          node->InputAt(0)->id(),
          NodeProperties::GetType(node->InputAt(0))
              .AsWasm()
              .type.name()
              .c_str(),
          node->InputAt(1)->id(),
          node->op()->ValueInputCount() > 1
              ? NodeProperties::GetType(node->InputAt(1))
                    .AsWasm()
                    .type.name()
                    .c_str()
              : "<control>",
          computed_type.type.name().c_str());
      break;
    }
    case IrOpcode::kLoadFromObject:
    case IrOpcode::kLoadImmutableFromObject: {
      Node* object = NodeProperties::GetValueInput(node, 0);
      Node* offset = NodeProperties::GetValueInput(node, 1);
      // This can happen either because the object has not been typed yet, or
      // because it is an internal VM object (e.g. the instance).
      if (!NodeProperties::IsTyped(object)) return NoChange();
      TypeInModule object_type = NodeProperties::GetType(object).AsWasm();
      // This can happen in unreachable branches.
      if (object_type.type.is_bottom()) {
        computed_type = {wasm::kWasmBottom, object_type.module};
        break;
      }
      if (object_type.type.is_rtt()) return NoChange();

      DCHECK(object_type.type.is_object_reference());

      IntPtrMatcher m(offset);
      // Do not modify if we are getting the map.
      if (m.Is(wasm::ObjectAccess::ToTagged(HeapObject::kMapOffset))) {
        return NoChange();
      }
      // Do not modify if we are retrieving the array length.
      if (object_type.type.is_reference_to(wasm::HeapType::kArray) &&
          m.Is(wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset))) {
        return NoChange();
      }
      // Do not modify if we are retrieving anything from a string or a view on
      // a string.
      if (object_type.type.is_reference_to(wasm::HeapType::kString) ||
          object_type.type.is_reference_to(wasm::HeapType::kStringViewWtf8) ||
          object_type.type.is_reference_to(wasm::HeapType::kStringViewWtf16) ||
          object_type.type.is_reference_to(wasm::HeapType::kStringViewIter)) {
        return NoChange();
      }
      uint32_t ref_index = object_type.type.ref_index();
      DCHECK(object_type.module->has_type(ref_index));
      wasm::TypeDefinition type_def = object_type.module->types[ref_index];
      switch (type_def.kind) {
        case wasm::TypeDefinition::kFunction:
          // This can happen for internal structures only.
          return NoChange();
        case wasm::TypeDefinition::kStruct: {
          wasm::ValueType field_type = StructFieldFromOffset(
              type_def.struct_type, static_cast<uint32_t>(m.ResolvedValue()),
              mcgraph_->machine()->Is32());
          if (field_type.is_bottom()) {
            FATAL(
                "Error - Bottom struct field. function: %d, node %d:%s, "
                "input0: %d, type: %s, offset %d\n",
                function_index_, node->id(), node->op()->mnemonic(),
                node->InputAt(0)->id(), object_type.type.name().c_str(),
                static_cast<int>(m.ResolvedValue()));
          }
          computed_type = {field_type, object_type.module};
          break;
        }
        case wasm::TypeDefinition::kArray: {
          // Do not modify if we are retrieving the array length.
          if (m.Is(wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset))) {
            return NoChange();
          }
          wasm::ValueType element_type = type_def.array_type->element_type();
          // We have to consider that, after int64 lowering in 32-bit platforms,
          // loads from i64 arrays get transformed into pairs of i32 loads.
          computed_type = {
              mcgraph_->machine()->Is32() && element_type == wasm::kWasmI64
                  ? wasm::kWasmI32
                  : element_type.Unpacked(),
              object_type.module};
          break;
        }
      }
      break;
    }
    default:
      return NoChange();
  }

  if (NodeProperties::IsTyped(node)) {
    TypeInModule current_type = NodeProperties::GetType(node).AsWasm();
    if (!(current_type.type.is_bottom() || computed_type.type.is_bottom() ||
          wasm::IsSubtypeOf(current_type.type, computed_type.type,
                            current_type.module, computed_type.module) ||
          wasm::IsSubtypeOf(computed_type.type, current_type.type,
                            computed_type.module, current_type.module))) {
      FATAL(
          "Error - Incompatible types. function: %d, node: %d:%s, input0:%d, "
          "current %s, computed %s\n",
          function_index_, node->id(), node->op()->mnemonic(),
          node->InputAt(0)->id(), current_type.type.name().c_str(),
          computed_type.type.name().c_str());
    }

    if (wasm::EquivalentTypes(current_type.type, computed_type.type,
                              current_type.module, computed_type.module)) {
      return NoChange();
    }
  }

  TRACE("function: %d, node: %d:%s, from: %s, to: %s\n", function_index_,
        node->id(), node->op()->mnemonic(),
        NodeProperties::IsTyped(node)
            ? NodeProperties::GetType(node).AsWasm().type.name().c_str()
            : "<untyped>",
        computed_type.type.name().c_str());

  NodeProperties::SetType(node, Type::Wasm(computed_type, graph_zone_));
  return Changed(node);
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
