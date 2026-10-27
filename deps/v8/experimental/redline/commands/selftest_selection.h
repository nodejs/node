// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_SELECTION_H_
#define V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_SELECTION_H_

#include <string>

#include "experimental/redline/commands/selftest_checker.h"

namespace redline {

// Offline checks (no target needed) of the four selection points: which
// corpus program is mutated next, which partner Splice/Combine get, which
// mutator is applied, and which generators CodeGen inserts.
//
// Every check drives the real selection code with a fixed seed and writes
// what was picked, one line per pick, into a text trace. The trace is
// compared with the file of the same name in `testdata_dir/selection/`.
// Those golden files were recorded before the selection code was moved
// behind the strategy interface, so a passing check proves that the default
// strategies pick exactly what the old code picked, draw for draw.
//
// With `write_golden` the traces are written instead of compared (only for
// deliberate behaviour changes; say so in the CL description).
void RunSelectionChecks(Checker* checker, const std::string& testdata_dir,
                        bool write_golden);

// Offline: every FAST node kind comes out of at least one generator, except
// the kinds on a short list with a reason each (DESIGN.md 9). A new node
// kind fails this check until a generator produces it or it is listed.
void RunNodeKindChecks(Checker* checker);

// Offline: every preset in experimental/redline/presets loads and names only
// existing strategies, mutators and generators. Skipped if the directory is not
// found.
void RunPresetChecks(Checker* checker);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_COMMANDS_SELFTEST_SELECTION_H_
