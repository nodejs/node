// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --regexp-tier-up-ticks=1

// 1. Build a RegExp with 1560 capture groups so that capture group 1550
// corresponds to result vector offset 0x3070 (elements 3100 and 3101).
const pattern = '()'.repeat(1549) + '(.*)' +
    '()'.repeat(11);
const re = new RegExp(pattern);

const workerScript = `
  load('test/mjsunit/mjsunit.js');

  const re = ` +
    re.toString() + `;

  // Prime tier-up with a 65-character OneByte string.
  // Group 1550 matches the 65 characters and populates output[3101] = 65.
  re.exec('a'.repeat(65));

  // Prepare a TwoByte string for the next execution.
  const subject = %FlattenString(('foo' + String.fromCharCode(0x1234))
                    .repeat(5000));
  const oneByteSample = %FlattenString('bar'.repeat(7000));
  const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  const oneByteMap = mem.getUint32(Sandbox.getAddressOf(oneByteSample), true);

  postMessage({
    subjectAddr: Sandbox.getAddressOf(subject),
    oneByteMap: oneByteMap
  });

  onmessage = function() {
    const result = re.exec(subject);
    assertEquals(subject, result[1550]);
    postMessage("done");
  };
`;

const w = new Worker(workerScript, {type: 'string'});
const msg = w.getMessage();

%BlockAt('IrregexpExecRaw_JIT', 5000);
w.postMessage('go');
assertTrue(%WaitUntilBlocked('IrregexpExecRaw_JIT', 5000));

// Mutate the subject string's map from TwoByte to OneByte while blocked.
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
mem.setUint32(msg.subjectAddr, msg.oneByteMap, true);

// Resume the worker.
// Without the fix, NativeRegExpMacroAssembler::Execute re-derived is_one_byte
// from the mutated subject, loading latin1_code_ (RegExpInterpreterTrampoline)
// and invoking it via the C++ ABI, causing a wild jump outside the sandbox
// (0x4100000000). With the fix, is_one_byte is passed through from
// EnsureCompiledIrregexp, ensuring the compiled native code is invoked, and an
// SBXCHECK guarantees that the code is CodeKind::REGEXP.
assertTrue(%Resume('IrregexpExecRaw_JIT'));
assertEquals('done', w.getMessage());
