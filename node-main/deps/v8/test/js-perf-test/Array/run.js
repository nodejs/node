// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('../base.js');

let array;
// Initialize func variable to ensure the first test doesn't benefit from
// global object property tracking.
let func = 0;
let this_arg;
let result;
const array_size = 100;
const max_index = array_size - 1;
// Matches what {FastSetup} below produces.
const max_index_value = `value ${max_index}`;

// newClosure is a handy function to get a fresh
// closure unpolluted by IC feedback for a 2nd-order array builtin
// test.

let cache_break = 0;

var __sequence = 0;
// Allocate arrays without AllocationSites.
function make_array_string(literal) {
  __sequence = __sequence + 1;
  return "/* " + __sequence + " */  " + literal;
}
function make_array(literal) {
  return eval(make_array_string(literal));
}

function newClosure(name, generic = false) {
  if (generic) {
    return new Function(
      `
      // ${cache_break++}
      result = Array.prototype.${name}.call(array, func, this_arg);
      `);
  }
  return new Function(
      `
      // ${cache_break++}
      result = array.${name}(func, this_arg);
      `);
}

function MakeHoley(array) {
  for (let i =0; i < array.length; i+=2) {
    delete array[i];
  }
  assert(%HasHoleyElements(array));
}

function SmiSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(i);
  // TODO(v8:10105): May still create holey arrays (allocation sites?).
  // assert(%HasFastPackedElements(array));
  assert(%HasSmiElements(array));
}

function HoleySmiSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(i);
  MakeHoley(array);
  assert(%HasSmiElements(array));
}

function DoubleSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(i + 0.5);
  // TODO(v8:10105): May still create holey arrays (allocation sites?).
  // assert(%HasFastPackedElements(array));
  assert(%HasDoubleElements(array));
}

function HoleyDoubleSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(i + 0.5);
  MakeHoley(array);
  assert(%HasDoubleElements(array));
}

function FastSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(`value ${i}`);
  // TODO(v8:10105): May still create holey arrays (allocation sites?).
  // assert(%HasFastPackedElements(array));
  assert(%HasObjectElements(array));
}

function HoleyFastSetup() {
  array = make_array('[]');
  for (let i = 0; i < array_size; i++) array.push(`value ${i}`);
  MakeHoley(array);
  assert(%HasObjectElements(array));
}

function DictionarySetup() {
  array = make_array('[]');
  // Add a large index to force dictionary elements.
  array[2**30] = 10;
  // Spread out {array_size} elements.
  for (var i = 0; i < array_size-1; i++) {
    array[i*101] = i;
  }
  assert(%HasDictionaryElements(array));
}

function ObjectSetup() {
  array = { length: array_size };
  for (var i = 0; i < array_size; i++) {
    array[i] = i;
  }
  assert(%HasObjectElements(array));
  assert(%HasHoleyElements(array));
}


const ARRAY_SETUP = {
  PACKED_SMI: SmiSetup,
  HOLEY_SMI: HoleySmiSetup,
  PACKED_DOUBLE: DoubleSetup,
  HOLEY_DOUBLE: HoleyDoubleSetup,
  PACKED: FastSetup,
  HOLEY: HoleyFastSetup,
  DICTIONARY: DictionarySetup,
}

function DefineHigherOrderTests(tests) {
  let i = 0;
  while (i < tests.length) {
     const [name, testFunc, setupFunc, callback] = tests[i++];

     let setupFuncWrapper = () => {
       func = callback;
       this_arg = undefined;
       setupFunc();
     };
     createSuite(name, 1000, testFunc, setupFuncWrapper);
  }
}

// Higher-order Array builtins.
d8.file.execute('filter.js');
d8.file.execute('map.js');
d8.file.execute('every.js');
d8.file.execute('some.js');
d8.file.execute('for-each.js');
d8.file.execute('reduce.js');
d8.file.execute('reduce-right.js');
d8.file.execute('find.js');
d8.file.execute('find-index.js');

// Other Array builtins.
d8.file.execute('from.js');
d8.file.execute('of.js');
d8.file.execute('join.js');
d8.file.execute('to-string.js');
d8.file.execute('slice.js');
d8.file.execute('copy-within.js');
d8.file.execute('at.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-Array(Score): ' + result);
}

function PrintStep(name) {}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError,
                           NotifyStep: PrintStep });
