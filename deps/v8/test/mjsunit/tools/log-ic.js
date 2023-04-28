// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --log-ic --logfile='+' --allow-natives-syntax

// The idea behind this test is to make sure we do not crash when using the
// --log-ic flag.


(function testLoadIC() {
  function loadIC(obj) {
    return obj.field;
  }

  %EnsureFeedbackVectorForFunction(loadIC);

  var obj = {field: 'hello'};
  loadIC(obj);
  loadIC(obj);
  loadIC(obj);
})();

(function testStoreIC() {
  function storeIC(obj, value) {
    return obj.field = value;
  }

  %EnsureFeedbackVectorForFunction(storeIC);

  var obj = {field: 'hello'};
  storeIC(obj, 'world');
  storeIC(obj, 'world');
  storeIC(obj, 'world');
})();

(function testKeyedLoadIC() {
  function keyedLoadIC(obj, field) {
    return obj[field];
  }

  %EnsureFeedbackVectorForFunction(keyedLoadIC);

  var obj = {field: 'hello'};
  keyedLoadIC(obj, 'field');
  keyedLoadIC(obj, 'field');
  keyedLoadIC(obj, 'field');
})();

(function testKeyedStoreIC() {
  function keyedStoreIC(obj, field, value) {
    return obj[field] = value;
  }

  %EnsureFeedbackVectorForFunction(keyedStoreIC);

  var obj = {field: 'hello'};
  keyedStoreIC(obj, 'field', 'world');
  keyedStoreIC(obj, 'field', 'world');
  keyedStoreIC(obj, 'field', 'world');
})();
