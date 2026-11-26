// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

// Regression test verifying that IrRegExpData::capture_name_map_ is
// initialized once at RegExp creation and cannot be cleared or mutated when
// CompileIrregexpFromSource re-parses a corrupted in-sandbox pattern string
// for another subject encoding.

const kSeqOneByteStringType =
    Sandbox.getInstanceTypeIdFor('SEQ_ONE_BYTE_STRING_TYPE');
const kSeqTwoByteStringType =
    Sandbox.getInstanceTypeIdFor('SEQ_TWO_BYTE_STRING_TYPE');
const kSeqCharsOffset =
    Sandbox.getFieldOffset(kSeqOneByteStringType, 'chars');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function overwriteAsciiChars(str, replacement) {
  assertEquals(kSeqOneByteStringType, Sandbox.getInstanceTypeIdOf(str));
  const addr = Sandbox.getAddressOf(str);
  for (let i = 0; i < replacement.length; i++) {
    memory.setUint8(addr + kSeqCharsOffset + i, replacement.charCodeAt(i));
  }
}

// 1. Deterministic check: mutating original_source from unnamed to named
// captures (or vice versa) before compiling for UC16 must not alter
// capture_name_map_ or break ResultsCache / CopyMatches.
(function testResultsCacheInvariance() {
  // Internalize via eval so ResultsCache::Lookup hits on subsequent calls.
  const longOneByte = eval('"' + 'A'.repeat(0x1100) + '"');

  const unnamedSource = ['(.)(?:a*)', '(?:cache0)?'].join('');
  const reUnnamed = new RegExp(unnamedSource, 'g');
  longOneByte.replace(reUnnamed, () => 'x');
  overwriteAsciiChars(unnamedSource, '(?<foo>.)');
  assertNotNull(reUnnamed.exec('A\u1234'));
  let unnamedArgc = 0;
  longOneByte.replace(reUnnamed, (...args) => {
    unnamedArgc = args.length;
    return 'x';
  });
  assertEquals(4, unnamedArgc);

  const namedSource = ['(?<foo>.)', '(?:cache1)?'].join('');
  const reNamed = new RegExp(namedSource, 'g');
  let firstGroups;
  longOneByte.replace(reNamed, (...args) => {
    if (firstGroups === undefined) firstGroups = args[args.length - 1];
    return 'x';
  });
  firstGroups.mutated = true;
  overwriteAsciiChars(namedSource, '(.)(?:a*)');
  assertNotNull(reNamed.exec('A\u1234'));
  let secondGroups;
  longOneByte.replace(reNamed, (...args) => {
    if (secondGroups === undefined) secondGroups = args[args.length - 1];
    return 'x';
  });
  assertEquals('A', secondGroups.foo);
  assertEquals(undefined, secondGroups.mutated);
})();

// 2. Concurrent check: flipping subject's map between 1-byte and 2-byte
// SeqString maps during SearchRegExpMultiple triggers UC16 compilation
// mid-loop and must not clear capture_name_map_ to null.
(function testConcurrentEncodingFlipInReplace() {
  // Use 0x00414141 ('A' in every byte and '\u4141' as two-byte) followed by an
  // adjacent tenured string so 2-byte reads stay in initialized OldSpace.
  const oneByteString = Array(100).fill('A').join('');
  const paddingString = Array(200).fill('A').join('');
  const twoByteString = Array(100).fill('\u4141').join('');
  assertEquals(
      kSeqOneByteStringType, Sandbox.getInstanceTypeIdOf(oneByteString));
  assertEquals(
      kSeqTwoByteStringType, Sandbox.getInstanceTypeIdOf(twoByteString));

  // Tenuring ensures Minor GC does not relocate or scan oneByteString while
  // the worker toggles its map word.
  gc();
  gc();

  const oneByteAddr = Sandbox.getAddressOf(oneByteString);
  const twoByteAddr = Sandbox.getAddressOf(twoByteString);
  const oneByteMap = memory.getUint32(oneByteAddr, true);
  const twoByteMap = memory.getUint32(twoByteAddr, true);

  // Use 25 capture groups so registers_per_match = 52 and
  // GlobalExecRunner::max_matches() = 98 / 52 = 1, forcing FetchNext() to call
  // IrregexpExecRaw on every match.
  function makePreparedRegExp(id) {
    const patternStr =
        ['(?<foo>.)', '()'.repeat(24), '(?:' + id + ')?'].join('');
    const re = new RegExp(patternStr, 'g');
    re.exec('A');
    re.lastIndex = 0;
    overwriteAsciiChars(patternStr, '(.)(?:a*)');
    return re;
  }

  const regexps = [];
  for (let i = 0; i < 300; i++) {
    regexps.push(makePreparedRegExp(i));
  }

  const shared = new SharedArrayBuffer(16);
  const control = new Int32Array(shared);
  Atomics.store(control, 0, 0);
  Atomics.store(control, 1, oneByteAddr | 0);
  Atomics.store(control, 2, oneByteMap | 0);
  Atomics.store(control, 3, twoByteMap | 0);

  const worker = new Worker(`
    onmessage = function(e) {
      const ctrl = new Int32Array(e.data);
      const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
      const addr = Atomics.load(ctrl, 1) >>> 0;
      const valA = Atomics.load(ctrl, 2) >>> 0;
      const valB = Atomics.load(ctrl, 3) >>> 0;
      postMessage('ready');
      while (Atomics.load(ctrl, 0) === 0) {
        for (let i = 0; i < 1000; i++) {
          mem.setUint32(addr, valA, true);
          mem.setUint32(addr, valB, true);
        }
      }
    };
  `, {type: 'string'});

  worker.postMessage(shared);
  assertEquals('ready', worker.getMessage());

  try {
    for (const re of regexps) {
      oneByteString.replace(re, (...args) => {
        assertEquals('string', typeof args[args.length - 1].foo);
        return 'x';
      });
    }
  } finally {
    Atomics.store(control, 0, 1);
    worker.terminate();
    memory.setUint32(oneByteAddr, oneByteMap, true);
  }
  assertEquals(200, paddingString.length);
})();
