// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing --allow-natives-syntax

function* gen() {
  yield 1;
}

// Ensure it's compiled so SFI has the BytecodeArray.
gen();

let sfi_addr = Sandbox.readObjectField(gen, "shared_function_info");
let sfi = Sandbox.getObjectAt(sfi_addr);
let handle = Sandbox.readObjectField(sfi, "trusted_function_data");

// Unpublish the trusted data.
Sandbox.unpublishTrustedHandle(handle);

// Trigger CreateGeneratorObject (CSA builtin).
// This should crash here as we try to untag an unpublished object.
gen();

assertUnreachable("Untagging unaware of TBI");
