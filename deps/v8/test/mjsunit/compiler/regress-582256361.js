// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class DoubleHolder extends Array {}
class SmiHolder extends Array {}

const box = { f2: false, out: 0, arr: [1] };

function na_setter(arr_param) {
  let a = arr_param[0];
  let oob1 = box.arr[1073741822];
  let oob2 = this[1073741822];
  let b = arr_param[0];
  box.out = a + b;
}

function array_setter(v) {}

Object.defineProperty(Object.prototype, "na_trigger", { set: na_setter });
Object.defineProperty(Array.prototype, "na_trigger", { set: array_setter });

function access_na(o, arr) {
  o.na_trigger = arr;
}

function smi_setter(args_param) {
  access_na(args_param, this);
}

Object.defineProperty(SmiHolder.prototype, "smi_trigger", { set: smi_setter });
Object.defineProperty(Array.prototype, "smi_trigger", { set: array_setter });

function access_smi(o, args) {
  o.smi_trigger = args;
}

function double_getter() {
  let p0 = this[0];
  let arr = [{}];
  arr[0] = p0;
  if (box.f2) {
    box.f2 = true;
  }
  access_smi(arr, this);
  return box.out;
}

function array_getter() {
  return this[0];
}

Object.defineProperty(DoubleHolder.prototype, "trigger", { get: double_getter });
Object.defineProperty(Array.prototype, "trigger", { get: array_getter });

function access(o) {
  return o.trigger;
}

function victim(...args) {
  return access(args);
}

const double_arr = new DoubleHolder();
double_arr[0] = 1.1;

const smi_arr = new SmiHolder();
smi_arr[0] = 1;

const non_array = { 0: 1 };

%PrepareFunctionForOptimization(na_setter);
%PrepareFunctionForOptimization(array_setter);
%PrepareFunctionForOptimization(access_na);
%PrepareFunctionForOptimization(smi_setter);
%PrepareFunctionForOptimization(access_smi);
%PrepareFunctionForOptimization(double_getter);
%PrepareFunctionForOptimization(array_getter);
%PrepareFunctionForOptimization(access);
%PrepareFunctionForOptimization(victim);

for (let i = 0; i < 10; i++) {
  box.f2 = (i % 2 === 0);
  box.arr = (i % 2 === 0) ? [1] : [1, 2];
  access_na(non_array, smi_arr);
  access_na([{}], smi_arr);
  access_smi(smi_arr, non_array);
  access_smi([{}], [1, 2]);
  access(double_arr);
  access([1, 2]);
}

assertEquals(1, victim(1, 2));
%OptimizeFunctionOnNextCall(victim);
assertEquals(1, victim(1, 2));
