// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LAYOUT_LOADER_H_
#define V8_TORQUE_LAYOUT_LOADER_H_

#include <string>

namespace v8::internal::torque {

// Loads the metagen layout JSON (schema in tools/metagen/layout_ir.py)
// and verifies every record for a @cppObjectLayoutDefinition class
// against the finalized Torque class type: base, base size, header
// size, and every field's name, offset, element size, storage
// category, constness, synchronization, and array shape must match.
//
// A mismatch is a Torque error at the field's position. A malformed
// file (unreadable, invalid JSON, wrong schema version) aborts
// compilation. The JSON contains every layout defined in C++, so
// records without a Torque class are skipped.
//
// Must run after TypeOracle::FinalizeAggregateTypes().
void VerifyCppLayouts(const std::string& layout_json_path);

// Populates @cppObjectLayoutDefinition classes in CurrentAst with fields
// built from the layout JSON. Classes can omit their .tq field declarations
// entirely, making the C++ layout authoritative. During migration, a class
// with a .tq body must declare equivalent fields; a mismatch is a Torque error.
// A class without a record keeps its .tq fields; without a .tq body,
// a missing record is an error. A class with an unannotated tail keeps
// its .tq fields or undefined layout (BigIntBase).
// Must run after parsing, before CompileCurrentAst.
//
// `positions_path` optionally names the positions JSON (per-member C++
// source positions). With it, diagnostics for the built fields and for
// annotation arguments point at the C++ member; without it they point at
// the .tq class declaration.
void ImportCppLayouts(const std::string& layout_json_path,
                      const std::string& positions_path);

}  // namespace v8::internal::torque

#endif  // V8_TORQUE_LAYOUT_LOADER_H_
