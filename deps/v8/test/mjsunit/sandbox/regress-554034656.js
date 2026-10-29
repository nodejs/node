// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

// 65,536 wrapped parameters make the uint16 parameter count wrap to one,
// while assigning parameter 65,535 writes outside the native argument frame.

const source = 'function victim(){' + ' '.repeat(64) + 'return 0;}';
(0, eval)(source);  // Leave the zero-parameter victim lazily compiled.

const target = globalThis.victim;
const names = Array.from({length: 0x10000}, (_, i) => 'p' + i);
const replacement = (' '.repeat('function victim'.length) + 'p65535=1;')
    .padEnd(source.length, ' ');

const sfi = Sandbox.dereferenceTaggedPointerField(
    target, 'shared_function_info');
const script = Sandbox.dereferenceTaggedPointerField(sfi, 'script');
const elements = Sandbox.dereferenceTaggedPointerField(names, 'elements');
const offsetOf = (object, field) => Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdOf(object), field);

// Derive otherwise-unexposed fields from neighboring supported API fields.
const sfiFlags = offsetOf(sfi, 'formal_parameter_count') + 6;
const scriptPosition = offsetOf(script, 'wasm_managed_native_module');
const scriptSource = scriptPosition - 36;
const wrappedArguments = scriptPosition - 4;
const oldFlags = Sandbox.readObjectField(sfi, sfiFlags, 32);
const wrappedFlags = ((oldFlags & ~(7 << 7)) | (4 << 7)) >>> 0;
const tagged = object => (Sandbox.getAddressOf(object) + 1) >>> 0;

// Only these three writes modify memory inside the V8 sandbox.
Sandbox.corruptObjectField(script, scriptSource, tagged(replacement), 32);
Sandbox.corruptObjectField(script, wrappedArguments, tagged(elements), 32);
Sandbox.corruptObjectField(sfi, sfiFlags, wrappedFlags, 32);

assertThrows(() => target(), SyntaxError);
