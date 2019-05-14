// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Compare `in` operator on different types of arrays.

const size = 1e5;
let packed_smi = [];
let packed_double = [];
let packed_elements = [];
let holey_smi = new Array(size);
let holey_double = new Array(size);
let holey_elements = new Array(size);
let sparse_smi = new Array(size);
let sparse_double = new Array(size);
let sparse_elements = new Array(size);
let typed_uint8 = new Uint8Array(size);
let typed_int32 = new Int32Array(size);
let typed_float = new Float64Array(size);

for (let i = 0; i < size; ++i) {
    packed_smi[i] = i;
    packed_double[i] = i + 0.1;
    packed_elements[i] = "" + i;
    holey_smi[i] = i;
    holey_double[i] = i + 0.1;
    holey_elements[i] = "" + i;
    typed_uint8[i] = i % 0x100;
    typed_int32[i] = i;
    typed_float[i] = i + 0.1;
}

let sparse = 0;
for (let i = 0; i < size; i += 100) {
    ++sparse;
    sparse_smi[i] = i;
    sparse_double[i] = i + 0.1;
    sparse_elements[i] = "" + i;
}

// ----------------------------------------------------------------------------
// Benchmark: Packed SMI
// ----------------------------------------------------------------------------

function PackedSMI() {
    let cnt = 0;
    let ary = packed_smi;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Packed Double
// ----------------------------------------------------------------------------

function PackedDouble() {
    let cnt = 0;
    let ary = packed_double;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Packed Elements
// ----------------------------------------------------------------------------

function PackedElements() {
    let cnt = 0;
    let ary = packed_elements;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Holey SMI
// ----------------------------------------------------------------------------

function HoleySMI() {
    let cnt = 0;
    let ary = holey_smi;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Holey Double
// ----------------------------------------------------------------------------

function HoleyDouble() {
    let cnt = 0;
    let ary = holey_double;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Holey Elements
// ----------------------------------------------------------------------------

function HoleyElements() {
    let cnt = 0;
    let ary = holey_elements;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Sparse SMI
// ----------------------------------------------------------------------------

function SparseSMI() {
    let cnt = 0;
    let ary = sparse_smi;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != sparse) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Sparse Double
// ----------------------------------------------------------------------------

function SparseDouble() {
    let cnt = 0;
    let ary = sparse_double;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != sparse) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Sparse Elements
// ----------------------------------------------------------------------------

function SparseElements() {
    let cnt = 0;
    let ary = sparse_elements;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != sparse) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Typed Uint8
// ----------------------------------------------------------------------------

function TypedUint8() {
    let cnt = 0;
    let ary = typed_uint8;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Typed Int32
// ----------------------------------------------------------------------------

function TypedInt32() {
    let cnt = 0;
    let ary = typed_int32;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Typed Float64
// ----------------------------------------------------------------------------

function TypedFloat64() {
    let cnt = 0;
    let ary = typed_float;
    for (let i = 0; i < ary.length; ++i) {
        if (i in ary) ++cnt;
    }

    if (cnt != size) throw 666;
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayInOperator(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 5, f) ]);
}

CreateBenchmark('PackedSMI', PackedSMI);
CreateBenchmark('PackedDouble', PackedDouble);
CreateBenchmark('PackedElements', PackedElements);
CreateBenchmark('HoleySMI', HoleySMI);
CreateBenchmark('HoleyDouble', HoleyDouble);
CreateBenchmark('HoleyElements', HoleyElements);
CreateBenchmark('SparseSMI', SparseSMI);
CreateBenchmark('SparseDouble', SparseDouble);
CreateBenchmark('SparseElements', SparseElements);
CreateBenchmark('TypedUint8', TypedUint8);
CreateBenchmark('TypedInt32', TypedInt32);
CreateBenchmark('TypedFloat64', TypedFloat64);

BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = true;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
