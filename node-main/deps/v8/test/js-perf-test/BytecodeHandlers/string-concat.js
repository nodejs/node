// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('ShortString-StringConcat-2', ShortStringConcat2);
addBenchmark('ShortString-StringConcat-3', ShortStringConcat3);
addBenchmark('ShortString-StringConcat-5', ShortStringConcat5);
addBenchmark('ShortString-StringConcat-10', ShortStringConcat10);
addBenchmark('LongString-StringConcat-2', LongStringConcat2);
addBenchmark('LongString-StringConcat-3', LongStringConcat3);
addBenchmark('LongString-StringConcat-5', LongStringConcat5);
addBenchmark('LongString-StringConcat-10', LongStringConcat10);
addBenchmark('NumberString-StringConcat-2', NumberStringConcat2);
addBenchmark('NumberString-StringConcat-3', NumberStringConcat3);
addBenchmark('NumberString-StringConcat-5', NumberStringConcat5);
addBenchmark('NumberString-StringConcat-10', NumberStringConcat10);

function stringConcat2(a) {
  for (var i = 0; i < 100; ++i) {
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
    "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a; "s" + a;
  }
}

function stringConcat3(a, b) {
  for (var i = 0; i < 100; ++i) {
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
    "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b; "s" + a + b;
  }
}

function stringConcat5(a, b, c, d) {
  for (var i = 0; i < 100; ++i) {
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
    "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d; "s" + a + b + c + d;
  }
}

function stringConcat10(a, b, c, d, e, f, g, h, i) {
  for (var j = 0; j < 100; ++j) {
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
    "s" + a + b + c + d + e + f + g + h + i; "s" + a + b + c + d + e + f + g + h + i;
  }
}

function ShortStringConcat2() {
  stringConcat2("a");
}

function ShortStringConcat3() {
  stringConcat3("a", "b");
}

function ShortStringConcat5() {
  stringConcat5("a", "b", "c", "d");
}

function ShortStringConcat10() {
  stringConcat10("a", "b", "c", "d", "e", "f", "g", "h", "i");
}

function LongStringConcat2() {
  stringConcat2("long string over consmin");
}

function LongStringConcat3() {
  stringConcat3("long string ", "over consmin");
}

function LongStringConcat5() {
  stringConcat5("long ", "string ", "over ", "consmin");
}

function LongStringConcat10() {
  stringConcat10("long ", "string ", "over ", "consmin ", "long ", "string ",
                 "over ", "consmin", "done");
}

function NumberStringConcat2() {
  stringConcat2(123.456);
}

function NumberStringConcat3() {
  stringConcat3(1, 0.2345);
}

function NumberStringConcat5() {
  stringConcat5(1, 2, 3, 4.5, 5.6);
}

function NumberStringConcat10() {
  stringConcat10(10, 2345, 5e10, 3.14, 987654321, 21.56789, 2, 3, 4);
}
