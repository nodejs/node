// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var re;
var str;

function Exec() {
  re.exec(str);
}

function SkipUntilChar1Byte() {
  str = %FlattenString('X'.repeat(1_000_000) + 'b');
  re = /[b]/;
}

function SkipUntilChar2Byte() {
  str = %FlattenString('\ud83d\udca9'.repeat(500_000) + '\u2615');
  re = /[\u2615]/;
}

function SkipUntilCharAnd1Byte() {
  str = %FlattenString('X'.repeat(1_000_000) + 'b');
  re = /[bc]/;
}

function SkipUntilCharAnd2Byte() {
  str = %FlattenString('\ud83d\udca9'.repeat(500_000) + '\u2615');
  re = /.{3}[\u2615]/;
}

function SkipUntilCharOrChar1Byte() {
  str = %FlattenString('X'.repeat(1_000_000) + 'b');
  re = /[^bd]*[bd]/;
}

function SkipUntilCharOrChar2Byte() {
  str = %FlattenString('\ud83d\udca9'.repeat(500_000) + '\u2615');
  re = /[^\u2615\u2728]*[\u2615\u2728]/;
}

function SkipUntilBitInTable1Byte() {
  str = %FlattenString('X'.repeat(1_000_000) + 'b');
  re = /[bd]/;
}

function SkipUntilBitInTable2Byte() {
  str = %FlattenString('\ud83d\udca9'.repeat(500_000) + '\u2615');
  re = /[\u2615\u2728]/;
}

function SkipUntilGtOrNotBitInTable1Byte() {
  str = %FlattenString('X'.repeat(1_000_000) + 'b');
  re = /^[ACEGIKMOQXS]*b/;
}

function SkipUntilGtOrNotBitInTable2Byte() {
  str = %FlattenString('X'.repeat(500_000) + '\u2615');
  re = /^[ACEGIKMOQXS]*\u2615/;
}

function SkipUntilOneOfMasked() {
  str = %FlattenString('X'.repeat(1_000_000) + 'YXZZ');
  re = /XZYY|YXZZ/i;
}

function SkipUntilOneOfMasked3() {
  str = %FlattenString('X'.repeat(1_000_000) + '<bceg');
  re = /<bdfh|<bceg|<abcd/i;
}

var benchmarks = [
  SkipUntilChar1Byte,
  SkipUntilCharAnd1Byte,
  SkipUntilCharOrChar1Byte,
  SkipUntilBitInTable1Byte,
  SkipUntilGtOrNotBitInTable1Byte,
  SkipUntilOneOfMasked,
  SkipUntilOneOfMasked3,
  SkipUntilChar2Byte,
  SkipUntilCharAnd2Byte,
  SkipUntilCharOrChar2Byte,
  SkipUntilBitInTable2Byte,
  SkipUntilGtOrNotBitInTable2Byte,
];

function CreateBenchmarkSuite(benchmarks) {
  benchmarks.forEach(
      (bench => new BenchmarkSuite(bench.name, [1000], [
         new Benchmark(bench.name, false, false, 0, Exec, bench, null, null, 2)
       ])));
}

CreateBenchmarkSuite(benchmarks);
