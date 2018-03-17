'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e6],
  type: ['depth', 'depth1', 'breadth', 'breadth1', 'breadth4', 'clear']
});

function main({ n, type }) {
  switch (type) {
    case 'depth':
      depth(n);
      break;
    case 'depth1':
      depth1(n);
      break;
    case 'breadth':
      breadth(n);
      break;
    case 'breadth1':
      breadth1(n);
      break;
    case 'breadth4':
      breadth4(n);
      break;
    case 'clear':
      clear(n);
      break;
  }
}

// setImmediate tail recursion, 0 arguments
function depth(N) {
  var n = 0;
  bench.start();
  setImmediate(cb);
  function cb() {
    n++;
    if (n === N)
      bench.end(n);
    else
      setImmediate(cb);
  }
}

// setImmediate tail recursion, 1 argument
function depth1(N) {
  var n = 0;
  bench.start();
  setImmediate(cb, 1);
  function cb(a1) {
    n++;
    if (n === N)
      bench.end(N);
    else
      setImmediate(cb, 1);
  }
}

// concurrent setImmediate, 0 arguments
function breadth(N) {
  var n = 0;
  bench.start();
  function cb() {
    n++;
    if (n === N)
      bench.end(N);
  }
  for (var i = 0; i < N; i++) {
    setImmediate(cb);
  }
}

// concurrent setImmediate, 1 argument
function breadth1(N) {
  var n = 0;
  bench.start();
  function cb(a1) {
    n++;
    if (n === N)
      bench.end(n);
  }
  for (var i = 0; i < N; i++) {
    setImmediate(cb, 1);
  }
}

// concurrent setImmediate, 4 arguments
function breadth4(N) {
  N /= 2;
  var n = 0;
  bench.start();
  function cb(a1, a2, a3, a4) {
    n++;
    if (n === N)
      bench.end(n);
  }
  for (var i = 0; i < N; i++) {
    setImmediate(cb, 1, 2, 3, 4);
  }
}

function clear(N) {
  N *= 4;
  bench.start();
  function cb(a1) {
    if (a1 === 2)
      bench.end(N);
  }
  for (var i = 0; i < N; i++) {
    clearImmediate(setImmediate(cb, 1));
  }
  setImmediate(cb, 2);
}
