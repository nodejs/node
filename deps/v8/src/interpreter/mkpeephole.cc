// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "src/globals.h"
#include "src/interpreter/bytecode-peephole-table.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {

namespace interpreter {

const char* ActionName(PeepholeAction action) {
  switch (action) {
#define CASE(Name)              \
  case PeepholeAction::k##Name: \
    return "PeepholeAction::k" #Name;
    PEEPHOLE_ACTION_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
      return "";
  }
}

std::string BytecodeName(Bytecode bytecode) {
  return "Bytecode::k" + std::string(Bytecodes::ToString(bytecode));
}

class PeepholeActionTableWriter final {
 public:
  static const size_t kNumberOfBytecodes =
      static_cast<size_t>(Bytecode::kLast) + 1;
  typedef std::array<PeepholeActionAndData, kNumberOfBytecodes> Row;

  void BuildTable();
  void Write(std::ostream& os);

 private:
  static const char* kIndent;
  static const char* kNamespaceElements[];

  void WriteHeader(std::ostream& os);
  void WriteIncludeFiles(std::ostream& os);
  void WriteClassMethods(std::ostream& os);
  void WriteUniqueRows(std::ostream& os);
  void WriteRowMap(std::ostream& os);
  void WriteRow(std::ostream& os, size_t row_index);
  void WriteOpenNamespace(std::ostream& os);
  void WriteCloseNamespace(std::ostream& os);

  PeepholeActionAndData LookupActionAndData(Bytecode last, Bytecode current);
  void BuildRow(Bytecode last, Row* row);
  size_t HashRow(const Row* row);
  void InsertRow(size_t row_index, const Row* const row, size_t row_hash,
                 std::map<size_t, size_t>* hash_to_row_map);
  bool RowsEqual(const Row* const first, const Row* const second);

  std::vector<Row>* table() { return &table_; }

  // Table of unique rows.
  std::vector<Row> table_;

  // Mapping of row index to unique row index.
  std::array<size_t, kNumberOfBytecodes> row_map_;
};

const char* PeepholeActionTableWriter::kIndent = "  ";
const char* PeepholeActionTableWriter::kNamespaceElements[] = {"v8", "internal",
                                                               "interpreter"};

// static
PeepholeActionAndData PeepholeActionTableWriter::LookupActionAndData(
    Bytecode last, Bytecode current) {
  // Optimize various accumulator loads followed by store accumulator
  // to an equivalent register load and loading the accumulator with
  // the register. The latter accumulator load can often be elided as
  // it is side-effect free and often followed by another accumulator
  // load so can be elided.
  if (current == Bytecode::kStar) {
    switch (last) {
      case Bytecode::kLdaNamedProperty:
        return {PeepholeAction::kTransformLdaStarToLdrLdarAction,
                Bytecode::kLdrNamedProperty};
      case Bytecode::kLdaKeyedProperty:
        return {PeepholeAction::kTransformLdaStarToLdrLdarAction,
                Bytecode::kLdrKeyedProperty};
      case Bytecode::kLdaGlobal:
        return {PeepholeAction::kTransformLdaStarToLdrLdarAction,
                Bytecode::kLdrGlobal};
      case Bytecode::kLdaContextSlot:
        return {PeepholeAction::kTransformLdaStarToLdrLdarAction,
                Bytecode::kLdrContextSlot};
      case Bytecode::kLdaUndefined:
        return {PeepholeAction::kTransformLdaStarToLdrLdarAction,
                Bytecode::kLdrUndefined};
      default:
        break;
    }
  }

  // ToName bytecodes can be replaced by Star with the same output register if
  // the value in the accumulator is already a name.
  if (current == Bytecode::kToName && Bytecodes::PutsNameInAccumulator(last)) {
    return {PeepholeAction::kChangeBytecodeAction, Bytecode::kStar};
  }

  // Nop are placeholders for holding source position information and can be
  // elided if there is no source information.
  if (last == Bytecode::kNop) {
    if (Bytecodes::IsJump(current)) {
      return {PeepholeAction::kElideLastBeforeJumpAction, Bytecode::kIllegal};
    } else {
      return {PeepholeAction::kElideLastAction, Bytecode::kIllegal};
    }
  }

  // The accumulator is invisible to the debugger. If there is a sequence
  // of consecutive accumulator loads (that don't have side effects) then
  // only the final load is potentially visible.
  if (Bytecodes::IsAccumulatorLoadWithoutEffects(last) &&
      Bytecodes::IsAccumulatorLoadWithoutEffects(current)) {
    return {PeepholeAction::kElideLastAction, Bytecode::kIllegal};
  }

  // The current instruction clobbers the accumulator without reading
  // it. The load in the last instruction can be elided as it has no
  // effect.
  if (Bytecodes::IsAccumulatorLoadWithoutEffects(last) &&
      Bytecodes::GetAccumulatorUse(current) == AccumulatorUse::kWrite) {
    return {PeepholeAction::kElideLastAction, Bytecode::kIllegal};
  }

  // Ldar and Star make the accumulator and register hold equivalent
  // values. Only the first bytecode is needed if there's a sequence
  // of back-to-back Ldar and Star bytecodes with the same operand.
  if (Bytecodes::IsLdarOrStar(last) && Bytecodes::IsLdarOrStar(current)) {
    return {PeepholeAction::kElideCurrentIfOperand0MatchesAction,
            Bytecode::kIllegal};
  }

  // TODO(rmcilroy): Add elide for consecutive mov to and from the same
  // register.

  // Remove ToBoolean coercion from conditional jumps where possible.
  if (Bytecodes::WritesBooleanToAccumulator(last)) {
    if (Bytecodes::IsJumpIfToBoolean(current)) {
      return {PeepholeAction::kChangeJumpBytecodeAction,
              Bytecodes::GetJumpWithoutToBoolean(current)};
    } else if (current == Bytecode::kToBooleanLogicalNot) {
      return {PeepholeAction::kChangeBytecodeAction, Bytecode::kLogicalNot};
    }
  }

  // Fuse LdaSmi followed by binary op to produce binary op with a
  // immediate integer argument. This savaes on dispatches and size.
  if (last == Bytecode::kLdaSmi) {
    switch (current) {
      case Bytecode::kAdd:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kAddSmi};
      case Bytecode::kSub:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kSubSmi};
      case Bytecode::kBitwiseAnd:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kBitwiseAndSmi};
      case Bytecode::kBitwiseOr:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kBitwiseOrSmi};
      case Bytecode::kShiftLeft:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kShiftLeftSmi};
      case Bytecode::kShiftRight:
        return {PeepholeAction::kTransformLdaSmiBinaryOpToBinaryOpWithSmiAction,
                Bytecode::kShiftRightSmi};
      default:
        break;
    }
  }

  // Fuse LdaZero followed by binary op to produce binary op with a
  // zero immediate argument. This saves dispatches, but not size.
  if (last == Bytecode::kLdaZero) {
    switch (current) {
      case Bytecode::kAdd:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kAddSmi};
      case Bytecode::kSub:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kSubSmi};
      case Bytecode::kBitwiseAnd:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kBitwiseAndSmi};
      case Bytecode::kBitwiseOr:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kBitwiseOrSmi};
      case Bytecode::kShiftLeft:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kShiftLeftSmi};
      case Bytecode::kShiftRight:
        return {
            PeepholeAction::kTransformLdaZeroBinaryOpToBinaryOpWithZeroAction,
            Bytecode::kShiftRightSmi};
      default:
        break;
    }
  }

  // If there is no last bytecode to optimize against, store the incoming
  // bytecode or for jumps emit incoming bytecode immediately.
  if (last == Bytecode::kIllegal) {
    if (Bytecodes::IsJump(current)) {
      return {PeepholeAction::kUpdateLastJumpAction, Bytecode::kIllegal};
    } else if (current == Bytecode::kNop) {
      return {PeepholeAction::kUpdateLastIfSourceInfoPresentAction,
              Bytecode::kIllegal};
    } else {
      return {PeepholeAction::kUpdateLastAction, Bytecode::kIllegal};
    }
  }

  // No matches, take the default action.
  if (Bytecodes::IsJump(current)) {
    return {PeepholeAction::kDefaultJumpAction, Bytecode::kIllegal};
  } else {
    return {PeepholeAction::kDefaultAction, Bytecode::kIllegal};
  }
}

void PeepholeActionTableWriter::Write(std::ostream& os) {
  WriteHeader(os);
  WriteIncludeFiles(os);
  WriteOpenNamespace(os);
  WriteUniqueRows(os);
  WriteRowMap(os);
  WriteClassMethods(os);
  WriteCloseNamespace(os);
}

void PeepholeActionTableWriter::WriteHeader(std::ostream& os) {
  os << "// Copyright 2016 the V8 project authors. All rights reserved.\n"
     << "// Use of this source code is governed by a BSD-style license that\n"
     << "// can be found in the LICENSE file.\n\n"
     << "// Autogenerated by " __FILE__ ". Do not edit.\n\n";
}

void PeepholeActionTableWriter::WriteIncludeFiles(std::ostream& os) {
  os << "#include \"src/interpreter/bytecode-peephole-table.h\"\n\n";
}

void PeepholeActionTableWriter::WriteUniqueRows(std::ostream& os) {
  os << "const PeepholeActionAndData PeepholeActionTable::row_data_["
     << table_.size() << "][" << kNumberOfBytecodes << "] = {\n";
  for (size_t i = 0; i < table_.size(); ++i) {
    os << "{\n";
    WriteRow(os, i);
    os << "},\n";
  }
  os << "};\n\n";
}

void PeepholeActionTableWriter::WriteRowMap(std::ostream& os) {
  os << "const PeepholeActionAndData* const PeepholeActionTable::row_["
     << kNumberOfBytecodes << "] = {\n";
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    os << kIndent << " PeepholeActionTable::row_data_[" << row_map_[i]
       << "], \n";
  }
  os << "};\n\n";
}

void PeepholeActionTableWriter::WriteRow(std::ostream& os, size_t row_index) {
  const Row row = table_.at(row_index);
  for (PeepholeActionAndData action_data : row) {
    os << kIndent << "{" << ActionName(action_data.action) << ","
       << BytecodeName(action_data.bytecode) << "},\n";
  }
}

void PeepholeActionTableWriter::WriteOpenNamespace(std::ostream& os) {
  for (auto element : kNamespaceElements) {
    os << "namespace " << element << " {\n";
  }
  os << "\n";
}

void PeepholeActionTableWriter::WriteCloseNamespace(std::ostream& os) {
  for (auto element : kNamespaceElements) {
    os << "}  // namespace " << element << "\n";
  }
}

void PeepholeActionTableWriter::WriteClassMethods(std::ostream& os) {
  os << "// static\n"
     << "const PeepholeActionAndData*\n"
     << "PeepholeActionTable::Lookup(Bytecode last, Bytecode current) {\n"
     << kIndent
     << "return &row_[Bytecodes::ToByte(last)][Bytecodes::ToByte(current)];\n"
     << "}\n\n";
}

void PeepholeActionTableWriter::BuildTable() {
  std::map<size_t, size_t> hash_to_row_map;
  Row row;
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    uint8_t byte_value = static_cast<uint8_t>(i);
    Bytecode last = Bytecodes::FromByte(byte_value);
    BuildRow(last, &row);
    size_t row_hash = HashRow(&row);
    InsertRow(i, &row, row_hash, &hash_to_row_map);
  }
}

void PeepholeActionTableWriter::BuildRow(Bytecode last, Row* row) {
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    uint8_t byte_value = static_cast<uint8_t>(i);
    Bytecode current = Bytecodes::FromByte(byte_value);
    PeepholeActionAndData action_data = LookupActionAndData(last, current);
    row->at(i) = action_data;
  }
}

// static
bool PeepholeActionTableWriter::RowsEqual(const Row* const first,
                                          const Row* const second) {
  return memcmp(first, second, sizeof(*first)) == 0;
}

// static
void PeepholeActionTableWriter::InsertRow(
    size_t row_index, const Row* const row, size_t row_hash,
    std::map<size_t, size_t>* hash_to_row_map) {
  // Insert row if no existing row matches, otherwise use existing row.
  auto iter = hash_to_row_map->find(row_hash);
  if (iter == hash_to_row_map->end()) {
    row_map_[row_index] = table()->size();
    table()->push_back(*row);
  } else {
    row_map_[row_index] = iter->second;

    // If the following DCHECK fails, the HashRow() is not adequate.
    DCHECK(RowsEqual(&table()->at(iter->second), row));
  }
}

// static
size_t PeepholeActionTableWriter::HashRow(const Row* row) {
  static const size_t kHashShift = 3;
  std::size_t result = (1u << 31) - 1u;
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(row);
  for (size_t i = 0; i < sizeof(*row); ++i) {
    size_t top_bits = result >> (kBitsPerByte * sizeof(size_t) - kHashShift);
    result = (result << kHashShift) ^ top_bits ^ raw_data[i];
  }
  return result;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

int main(int argc, const char* argv[]) {
  CHECK_EQ(argc, 2);

  std::ofstream ofs(argv[1], std::ofstream::trunc);
  v8::internal::interpreter::PeepholeActionTableWriter writer;
  writer.BuildTable();
  writer.Write(ofs);
  ofs.flush();
  ofs.close();

  return 0;
}
