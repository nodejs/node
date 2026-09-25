// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('StringNormalize', [5], [
  new Benchmark('StringNormalize', false, false, 0,
  StringNormalize),
]);
new BenchmarkSuite('StringNormalizeNFD', [5], [
  new Benchmark('StringNormalizeNFD', false, false, 0,
  StringNormalizeNFD),
]);
new BenchmarkSuite('StringNormalizeNFKC', [5], [
  new Benchmark('StringNormalizeNFKC', false, false, 0,
  StringNormalizeNFKC),
]);
new BenchmarkSuite('StringNormalizeNFKD', [5], [
  new Benchmark('StringNormalizeNFKD', false, false, 0,
  StringNormalizeNFKD),
]);

const shortString = "àèìòùáéíóúäëïöüÿâêîôûãõñ";

function StringNormalize() {
  return shortString.normalize();
}

function StringNormalizeNFD() {
  return shortString.normalize("NFD");
}

function StringNormalizeNFKC() {
  return shortString.normalize("NFKC");
}

function StringNormalizeNFKD() {
  return shortString.normalize("NFKD");
}
