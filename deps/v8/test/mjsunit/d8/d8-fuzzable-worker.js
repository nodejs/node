// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for a more fuzzable way for creating workers based on functions.

(function TestWorker() {
  function workerCode1() {
    onmessage = function() {
      postMessage('hi');
    }
  }

  let w = new Worker(workerCode1, {type: 'function'});
  w.postMessage('');
  assertEquals('hi', w.getMessage());
})();

(function TestPassingNumberParam() {
  function workerCode2(n) {
    onmessage = function() {
        postMessage('worker ' + (n + 1));
    }
  }

  w = new Worker(workerCode2, {type: 'function', arguments: [2021]});
  w.postMessage('');
  assertEquals('worker 2022', w.getMessage());
})();

(function TestPassingStringParam() {
  function workerCode3(s) {
    onmessage = function() {
        postMessage('worker ' + s);
    }
  }

  w = new Worker(workerCode3, {type: 'function', arguments: ['hello']});
  w.postMessage('');
  assertEquals('worker hello', w.getMessage());
})();

(function TestPassingObjectParam() {
  function workerCode4(o) {
    onmessage = function() {
        postMessage('worker ' + (o.x + 1));
    }
  }

  w = new Worker(workerCode4, {type: 'function', arguments: [{x: 1}]});
  w.postMessage('');
  assertEquals('worker 2', w.getMessage());
})();

(function TestPassingFunctionParam() {
  function workerCode5(f) {
    eval(f);
    onmessage = function() {
        postMessage('worker ' + func());
    }
  }

  let config = {'func': function func() { return 'hi';} };

  w = new Worker(workerCode5, {type: 'function',
                               arguments: [config.func.toString()]});
  w.postMessage('');
  assertEquals('worker hi', w.getMessage());
})();
