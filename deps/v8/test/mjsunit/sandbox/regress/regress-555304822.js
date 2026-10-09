// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing --allow-natives-syntax

const cage = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const functionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const sfiType = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
const sfiOffset = Sandbox.getFieldOffset(functionType, 'shared_function_info');
const dispatchHandleOffset =
    Sandbox.getFieldOffset(functionType, 'dispatch_handle');
const formalCountOffset =
    Sandbox.getFieldOffset(sfiType, 'formal_parameter_count');

function objectOffset(object) {
  return Sandbox.getAddressOf(object) >>> 0;
}
function getSfiOffset(fn) {
  return (cage.getUint32(objectOffset(fn) + sfiOffset, true) - 1) >>> 0;
}
function readFormalCount(fn) {
  return cage.getUint16(getSfiOffset(fn) + formalCountOffset, true);
}
function writeFormalCount(fn, value) {
  cage.setUint16(getSfiOffset(fn) + formalCountOffset, value, true);
}
function readDispatchHandle(fn) {
  return cage.getUint32(objectOffset(fn) + dispatchHandleOffset, true);
}
function writeDispatchHandle(fn, value) {
  cage.setUint32(objectOffset(fn) + dispatchHandleOffset, value, true);
}

// Part 1: Corrupting SFI::formal_parameter_count in the sandbox must not cause
// TurboFan to emit mismatched underapplication padding or corrupt SP/locals.
function target1(a) {
  return a === undefined ? 1 : 2;
}
%NeverOptimizeFunction(target1);

const doubles = new Float64Array(48);
for (let i = 0; i < doubles.length; ++i) doubles[i] = i + 0.25;

const declarations = [];
const additions = [];
for (let i = 0; i < doubles.length; ++i) {
  declarations.push(`const v${i} = doubles[${i}];`);
  additions.push(`v${i}`);
}
const factory =
    new Function('target', `return function caller1(run, alternate, doubles) {
       if (!run) return 0;
       alternate |= 0;
       ${declarations.join('\n')}
       target();
       const total = ${additions.join(' + ')};
       return alternate + (total > 0 ? 1 : 0);
     };`);
const caller1 = factory(target1);

%PrepareFunctionForOptimization(caller1);
assertEquals(0x101, caller1(true, 0x100, doubles));
assertEquals(0x101, caller1(true, 0x100, doubles));

const authenticCount = readFormalCount(target1);
writeFormalCount(target1, 8);
%OptimizeFunctionOnNextCall(caller1);
assertEquals(0x101, caller1(true, 0x100, doubles));
writeFormalCount(target1, authenticCount);

// Part 2: Defense-in-depth runtime check. If a JSFunction's dispatch_handle is
// swapped in-sandbox to a target with fewer parameters than the caller's padded
// argument_count (so argument_count > parameter_count while
// argc_reg < argument_count), CallJSFunction must abort with
// kJSSignatureMismatch.
function makeTarget() {
  return function targetMany(a, b, c, d) {
    return 10;
  };
}
const t1 = makeTarget();
const t2 = makeTarget();
function targetFew() {
  return 20;
}
%NeverOptimizeFunction(t1);
%NeverOptimizeFunction(t2);
%NeverOptimizeFunction(targetFew);
%PrepareFunctionForOptimization(t1);
%PrepareFunctionForOptimization(t2);
%PrepareFunctionForOptimization(targetFew);
t1();
t2();
targetFew();

function caller2(fn) {
  return fn();
}
%PrepareFunctionForOptimization(caller2);
assertEquals(10, caller2(t1));
assertEquals(10, caller2(t2));
%OptimizeFunctionOnNextCall(caller2);
assertEquals(10, caller2(t1));

// Swap t1's dispatch handle to targetFew's dispatch handle (0 params).
// caller2 pushes 5 slots (receiver + 4 padded undefineds) with argc_reg = 1,
// while targetFew expects 1 slot (receiver). This triggers SbxCheck abort.
writeDispatchHandle(t1, readDispatchHandle(targetFew));
caller2(t1);
