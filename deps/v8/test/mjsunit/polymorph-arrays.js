// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax
function init_array(a) {
  for (var i = 0; i < 10; ++i ){
    a[i] = i;
  }
}

function init_sparse_array(a) {
  for (var i = 0; i < 10; ++i ){
    a[i] = i;
  }
  a[200000] = 256;
  return %NormalizeElements(a);
}

function testPolymorphicLoads() {
  function make_polymorphic_load_function() {
    function load(a, i) {
      return a[i];
    }
    %PrepareFunctionForOptimization(load);

    var object_array = new Object;
    var sparse_object_array = new Object;
    var js_array = new Array(10);
    var sparse_js_array = %NormalizeElements([]);

    init_array(object_array);
    init_array(js_array);
    init_sparse_array(sparse_object_array);
    init_sparse_array(sparse_js_array);

    assertEquals(1, load(object_array, 1));
    assertEquals(1, load(js_array, 1));
    assertEquals(1, load(sparse_object_array, 1));
    assertEquals(1, load(sparse_js_array, 1));

    return load;
  }

  var object_array = new Object;
  var sparse_object_array = new Object;
  var js_array = new Array(10);
  var sparse_js_array = %NormalizeElements([]);

  init_array(object_array);
  init_array(js_array);
  init_sparse_array(sparse_object_array);
  init_sparse_array(sparse_js_array);

  load = make_polymorphic_load_function();
  assertEquals(undefined, load(js_array, new Object()));
  load = make_polymorphic_load_function();
  assertEquals(undefined, load(object_array, new Object()));
  load = make_polymorphic_load_function();
  assertEquals(undefined, load(sparse_js_array, new Object()));
  load = make_polymorphic_load_function();
  assertEquals(undefined, load(sparse_object_array, new Object()));

  // Try with optimizing compiler.
  load = make_polymorphic_load_function();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(1, load(object_array, 1));
  assertEquals(1, load(js_array, 1));
  assertEquals(1, load(sparse_object_array, 1));
  assertEquals(1, load(sparse_js_array, 1));

  load = make_polymorphic_load_function();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(undefined, load(js_array, new Object()));
  load = make_polymorphic_load_function();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(undefined, load(object_array, new Object()));
  load = make_polymorphic_load_function();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(undefined, load(sparse_js_array, new Object()));
  load = make_polymorphic_load_function();
  %OptimizeFunctionOnNextCall(load);
  assertEquals(undefined, load(sparse_object_array, new Object()));
}

function testPolymorphicStores() {
  function make_polymorphic_store_function() {
    function store(a, i, val) {
      a[i] = val;
    }
    %PrepareFunctionForOptimization(store);

    var object_array = new Object;
    var sparse_object_array = new Object;
    var js_array = new Array(10);
    var sparse_js_array = [];
    sparse_js_array.length = 200001;

    init_array(object_array);
    init_array(js_array);
    init_sparse_array(sparse_object_array);
    init_sparse_array(sparse_js_array);

    store(object_array, 1, 256);
    store(js_array, 1, 256);
    store(sparse_object_array, 1, 256);
    store(sparse_js_array, 1, 256);

    return store;
  }

  var object_array = new Object;
  var sparse_object_array = new Object;
  var js_array = new Array(10);
  var sparse_js_array = %NormalizeElements([]);
  sparse_js_array.length = 200001;
  assertTrue(%HasDictionaryElements(sparse_js_array));

  init_array(object_array);
  init_array(js_array);
  init_sparse_array(sparse_object_array);
  init_sparse_array(sparse_js_array);

  store = make_polymorphic_store_function();
  store(object_array, 2, 257);
  store = make_polymorphic_store_function();
  store(js_array, 2, 257);
  store = make_polymorphic_store_function();
  store(sparse_object_array, 2, 257);
  store = make_polymorphic_store_function();
  store(sparse_js_array, 2, 257);

  assertEquals(257, object_array[2]);
  assertEquals(257, js_array[2]);
  assertEquals(257, sparse_js_array[2]);
  assertEquals(257, sparse_object_array[2]);

  // Now try Crankshaft optimized polymorphic stores
  store = make_polymorphic_store_function();
  %OptimizeFunctionOnNextCall(store);
  store(object_array, 3, 258);
  store = make_polymorphic_store_function();
  %OptimizeFunctionOnNextCall(store);
  store(js_array, 3, 258);
  store = make_polymorphic_store_function();
  %OptimizeFunctionOnNextCall(store);
  store(sparse_object_array, 3, 258);
  store = make_polymorphic_store_function();
  %OptimizeFunctionOnNextCall(store);
  store(sparse_js_array, 3, 258);

  assertEquals(258, object_array[3]);
  assertEquals(258, js_array[3]);
  assertEquals(258, sparse_js_array[3]);
  assertEquals(258, sparse_object_array[3]);
}

testPolymorphicLoads();
testPolymorphicStores();
