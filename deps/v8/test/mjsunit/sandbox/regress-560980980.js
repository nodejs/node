// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

function makeClass() {
  class C {
    m() {}
  }
  return C;
}

%PrepareFunctionForOptimization(makeClass);
makeClass();

// Retrieve the ClassBoilerplate from the bytecode constant pool.
const bytecode = %GetBytecode(makeClass);
const kClassBoilerplateType =
    Sandbox.getInstanceTypeIdFor("CLASS_BOILERPLATE_TYPE");
let boilerplate = null;
for (let i = 0; i < bytecode.constant_pool.length; i++) {
  const elem = bytecode.constant_pool[i];
  if (Sandbox.getInstanceTypeOf(elem) === "CLASS_BOILERPLATE_TYPE") {
    boilerplate = elem;
    break;
  }
}
assertTrue(boilerplate !== null);

const kInstancePropsOffset = Sandbox.getFieldOffset(
    kClassBoilerplateType, "instance_properties_template");
const template =
    Sandbox.dereferenceTaggedPointerField(boilerplate, kInstancePropsOffset);
assertTrue(template !== null);

// The dynamic method argument index for `m` is Smi(3) (raw value 6).
// Corrupt it to Smi(4) (raw value 8) which equals args.length().
let found = false;
for (let offset = 0; offset < Sandbox.getSizeOf(template); offset += 4) {
  if (Sandbox.readObjectField(template, offset) === (3 << 1)) {
    Sandbox.corruptObjectField(template, offset, 4 << 1);
    found = true;
    break;
  }
}
assertTrue(found);

// Re-executing makeClass() must safely abort via SBXCHECK_LT.
makeClass();

assertUnreachable("Argument index out of bounds should be trapped by SBXCHECK");
