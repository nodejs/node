// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringToLocaleUpperCaseTR', [5], [
  new Benchmark('StringToLocaleUpperCaseTR', false, false, 0,
  StringToLocaleUpperCaseTR)
]);
new BenchmarkSuite('StringToLocaleLowerCaseTR', [5], [
  new Benchmark('StringToLocaleLowerCaseTR', false, false, 0,
  StringToLocaleLowerCaseTR),
]);
new BenchmarkSuite('StringToLocaleUpperCase', [5], [
  new Benchmark('StringToLocaleUpperCase', false, false, 0,
  StringToLocaleUpperCase)
]);
new BenchmarkSuite('StringToLocaleLowerCase', [5], [
  new Benchmark('StringToLocaleLowerCase', false, false, 0,
  StringToLocaleLowerCase),
]);

var shortString = "Îñţérñåţîöñåļîžåţîöñ Ļöçåļîžåţîöñ החןןם שםוןמ Γρεεκ ισ φθν 一二三";

function StringToLocaleUpperCase() {
  return shortString.toLocaleUpperCase();
}
function StringToLocaleLowerCase() {
  return shortString.toLocaleLowerCase();
}
function StringToLocaleUpperCaseTR() {
  return shortString.toLocaleUpperCase(["tr"]);
}
function StringToLocaleLowerCaseTR() {
  return shortString.toLocaleLowerCase(["tr"]);
}
