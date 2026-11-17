// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/568986689.');
const PAGE_SRC = `
var __status = 'init';     // phase-1 status, read by the client
var __status2 = 'init';    // phase-2 status, read by the client
var __hits1 = 0, __hits2 = 0;
var __stash = null;        // strong JS ref to the stale JSExternalObject
var __stashMapWord = -1;   // JSExternalObject map word, for verification
var K1, K2, g1, objB, graph2;
var __aG1 = 0, __aK1 = 0, __aK2 = 0, __aG2 = 0, __aObjB = 0, __stashTagged = 0;

var SB = (typeof Sandbox !== 'undefined') ? Sandbox : null;
var PAGE = 0x40000;  // V8 heap page size (256 KiB)

function __tagged(o) { return (SB.getAddressOf(o) | 1) >>> 0; }

function preparePhase1() {
  if (!SB) { __status = 'NO_SANDBOX_API'; return __status; }
  K1 = {}; K2 = {};
  g1 = {m1: K1, m2: K2};
  Object.defineProperty(g1, 'g',
      {enumerable: true, configurable: true, get: __g1getter});
  __stash = null;
  __status = 'prepared1';
  return __status;
}

function snapshotAddrs1() {
  __aG1 = __tagged(g1); __aK1 = __tagged(K1); __aK2 = __tagged(K2);
  __status = 'snap1 g1=0x' + __aG1.toString(16) + ' K1=0x' +
      __aK1.toString(16) + ' K2=0x' + __aK2.toString(16);
  return __status;
}

function __g1getter() {
  try {
    var probe = {};
    var probeAddr = SB.getAddressOf(probe);
    var base = probeAddr - (probeAddr % PAGE);
    var len = (probeAddr - base) & ~3;
    if (len < 64) { __status = 'G1 window-too-small'; return 1; }
    var ab = new SB.MemoryView(base, len);
    var u16 = new Uint16Array(ab);
    var u32 = new Uint32Array(ab);
    var g1lo = __aG1 & 0xFFFF, g1hi = __aG1 >>> 16;
    var k1lo = __aK1 & 0xFFFF, k1hi = __aK1 >>> 16;
    var k2lo = __aK2 & 0xFFFF, k2hi = __aK2 >>> 16;
    var hit = -1, hits = 0, n = u16.length - 14;
    for (var i = 0; i <= n; i += 2) {
      if (u16[i] === g1lo && u16[i + 1] === g1hi &&
          u16[i + 6] === k1lo && u16[i + 7] === k1hi &&
          u16[i + 12] === k2lo && u16[i + 13] === k2hi) { hit = i; hits++; }
    }
    __hits1 = hits;
    if (hit < 0) { __status = 'G1 scan-miss len=0x' + len.toString(16); return 1; }
    var extTagged = u32[(hit * 2 + 16) >> 2];
    if ((extTagged & 3) !== 1) { __status = 'G1 value-not-tagged'; return 1; }
    __stash = SB.getObjectAt(extTagged);
    var t = SB.getInstanceTypeOf(__stash);
    if (t !== 'JS_EXTERNAL_OBJECT_TYPE') {
      __status = 'G1 wrong-type: ' + t; __stash = null; return 1;
    }
    var sAddr = SB.getAddressOf(__stash);
    __stashMapWord = new Uint32Array(new SB.MemoryView(sAddr, 8))[0];
    __status = 'stash-ok hits=' + hits + ' ext=0x' + extTagged.toString(16);
  } catch (e) {
    __status = 'G1-error: ' + String(e && e.message || e);
  }
  return 1;
}

function preparePhase2() {
  objB = {};
  graph2 = {a: objB};                        // objB registered at 'a'
  Object.defineProperty(graph2, 'b',         // getter fires between 'a' and 'c'
      {enumerable: true, configurable: true, get: __g2getter});
  graph2.c = objB;                           // objB re-encountered at 'c'
  __status2 = 'prepared2';
  return __status2;
}

function snapshotAddrs2() {
  if (!__stash) { __status2 = 'no-stash'; return __status2; }
  __aG2 = __tagged(graph2); __aObjB = __tagged(objB);
  __stashTagged = __tagged(__stash);
  __status2 = 'snap2 stash=0x' + __stashTagged.toString(16);
  return __status2;
}

function __g2getter() {
  try {
    if (!__stashTagged) { __status2 = 'G2 not-armed'; return 2; }
    var probe = {};
    var probeAddr = SB.getAddressOf(probe);
    var base = probeAddr - (probeAddr % PAGE);
    var len = (probeAddr - base) & ~3;
    if (len < 64) { __status2 = 'G2 window-too-small'; return 2; }
    var ab = new SB.MemoryView(base, len);
    var u16 = new Uint16Array(ab);
    var u32 = new Uint32Array(ab);
    var g2lo = __aG2 & 0xFFFF, g2hi = __aG2 >>> 16;
    var bLo = __aObjB & 0xFFFF, bHi = __aObjB >>> 16;
    var hit = -1, hits = 0, n = u16.length - 8;
    for (var i = 0; i <= n; i += 2) {
      if (u16[i] === g2lo && u16[i + 1] === g2hi &&
          u16[i + 6] === bLo && u16[i + 7] === bHi) { hit = i; hits++; }
    }
    __hits2 = hits;
    if (hit < 0) { __status2 = 'G2 scan-miss len=0x' + len.toString(16); return 2; }
    var vIdx = (hit * 2 + 16) >> 2;        // objB entry's value slot
    var cur = u32[vIdx];
    if ((cur & 3) !== 1) { __status2 = 'G2 cur-not-tagged'; return 2; }
    var curOff = ((cur & ~3) >>> 0) - base;
    if (curOff >= 0 && curOff + 4 <= len && u32[curOff >> 2] !== __stashMapWord) {
      __status2 = 'G2 mapword-mismatch'; return 2;
    }
    u32[vIdx] = __stashTagged;
    __status2 = 'swap-ok hits=' + hits;
  } catch (e) {
    __status2 = 'G2-error: ' + String(e && e.message || e);
  }
  return 2;
}
'page-script-loaded';
`;
const MAX_ATTEMPTS = 8;

async function ev(expr) {
  const r =
      await Protocol.Runtime.evaluate({expression: expr, returnByValue: true});
  return r.result.result.value;
}

async function fullGC() {
  await Protocol.HeapProfiler.collectGarbage();
  await Protocol.HeapProfiler.collectGarbage();
}

(async function main() {
  await ev(PAGE_SRC);
  for (let attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    await ev('preparePhase1()');
    await fullGC();
    await ev('snapshotAddrs1()');
    await Protocol.Runtime.evaluate(
        {expression: 'g1', serializationOptions: {serialization: 'deep'}});
    const st1 = await ev('__status');
    if (!/^stash-ok/.test(st1)) continue;
    await ev('preparePhase2()');
    await fullGC();
    await ev('snapshotAddrs2()');
    await Protocol.Runtime.evaluate(
        {expression: 'graph2', serializationOptions: {serialization: 'deep'}});
    break;
  }
  InspectorTest.log('done');
  InspectorTest.completeTest();
})();
