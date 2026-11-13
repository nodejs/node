// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const kHeapObjectTag = 1;
const kSmiShift = 1;
const kScopeInfoForceContextAllocation = 1 << 23;

const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kFunctionContextType =
    Sandbox.getInstanceTypeIdFor('FUNCTION_CONTEXT_TYPE');
const kScopeInfoType = Sandbox.getInstanceTypeIdFor('SCOPE_INFO_TYPE');

const kContextOffset = Sandbox.getFieldOffset(kJSFunctionType, 'context');
const kScopeInfoOffset =
    Sandbox.getFieldOffset(kFunctionContextType, 'scope_info');
const kScopeInfoFlagsOffset = Sandbox.getFieldOffset(kScopeInfoType, 'flags');
const kScopeInfoContextLocalCountOffset =
    Sandbox.getFieldOffset(kScopeInfoType, 'context_local_count');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address >>> 0, true);
const write32 = (address, value) =>
    memory.setUint32(address >>> 0, value >>> 0, true);

function outer(mode) {
  {
    let local = 1;
    if (mode === 1) return () => local;
    return 0;
  }
}

const closure = outer(1);
const context =
    read32(Sandbox.getAddressOf(closure) + kContextOffset) - kHeapObjectTag;
const scopeInfo = read32(context + kScopeInfoOffset) - kHeapObjectTag;
const savedFlags = read32(scopeInfo + kScopeInfoFlagsOffset);
const savedCount = read32(scopeInfo + kScopeInfoContextLocalCountOffset);

// Corrupt ScopeInfo to have a negative context local count, causing
// Context::SizeFor to return -16, which underflows to a 0-sized region
// allocation request in MemoryAllocator.
write32(
    scopeInfo + kScopeInfoFlagsOffset,
    savedFlags | kScopeInfoForceContextAllocation);
write32(scopeInfo + kScopeInfoContextLocalCountOffset, -8 << kSmiShift);

try {
  outer(2);
} catch (e) {
}

// Restore ScopeInfo
write32(scopeInfo + kScopeInfoFlagsOffset, savedFlags);
write32(scopeInfo + kScopeInfoContextLocalCountOffset, savedCount);

gc();
