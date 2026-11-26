// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

// Regression test for the RegExp parser: a pattern that changes between
// ScanForCaptures and the main parse must not compile a back-reference to a
// capture group that does not exist.

const kSeqOneByteStringType =
    Sandbox.getInstanceTypeIdFor('SEQ_ONE_BYTE_STRING_TYPE');
const kSeqCharsOffset =
    Sandbox.getFieldOffset(kSeqOneByteStringType, 'chars');

// '\1' is followed by no capture group, so it is an octal escape and the
// pattern cannot match the subject. If the scan triggered by '\1' sees the
// tail as '(a', '\1' becomes a back-reference to capture 1. The main parse
// only accepts the pattern if it then sees the tail as 'aa'.
//
// The registers of capture 1 alias the two loop counters. Both loops match
// four characters, so the back-reference would be empty and the pattern would
// match.
const kSubject = 'xxxxyyyyaa';
// Parsing the filler keeps the scan and the main parse of the tail apart.
const kFiller = 'z'.repeat(4096);
// Each pattern is one attempt to change the tail between the two reads.
const kPatternCount = 500;

const patterns = [];
const regexps = [];
for (let i = 0; i < kPatternCount; i++) {
  // The index keeps each pattern out of the compilation cache.
  const pattern =
      ['^x{4,101}y{4,102}\\1(?:', kFiller, i, '){0}aa'].join('');
  assertEquals(kSeqOneByteStringType, Sandbox.getInstanceTypeIdOf(pattern));
  patterns.push(pattern);
  regexps.push(new RegExp(pattern));
}

// Promote the patterns to old space so that a minor GC does not move them
// while the worker writes to them.
gc();
gc();

const shared = new SharedArrayBuffer(8);
const control = new Int32Array(shared);
// control[0]: stop flag (0 = run, 1 = stop)
// control[1]: address of the last two pattern characters, or 0
Atomics.store(control, 0, 0);
Atomics.store(control, 1, 0);

const worker = new Worker(`
  onmessage = function(e) {
    const control = new Int32Array(e.data);
    const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    postMessage('ready');
    while (Atomics.load(control, 0) === 0) {
      const addr = Atomics.load(control, 1) >>> 0;
      if (addr === 0) continue;
      for (let i = 0; i < 1000; i++) {
        mem.setUint16(addr, 0x6128, true);  // '(a'
        mem.setUint16(addr, 0x6161, true);  // 'aa'
      }
    }
  };
`, {type: 'string'});

worker.postMessage(shared);
assertEquals('ready', worker.getMessage());

try {
  for (let i = 0; i < kPatternCount; i++) {
    const pattern = patterns[i];
    const tail =
        Sandbox.getAddressOf(pattern) + kSeqCharsOffset + pattern.length - 2;
    Atomics.store(control, 1, tail | 0);
    let result = null;
    try {
      // The first execution parses the pattern again.
      result = regexps[i].exec(kSubject);
    } catch (e) {
      // The main parse saw the unterminated group.
      assertInstanceof(e, SyntaxError);
    }
    Atomics.store(control, 1, 0);
    assertNull(result);
  }
} finally {
  Atomics.store(control, 0, 1);
  worker.terminate();
}
