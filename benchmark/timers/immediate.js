'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  thousands: [5000],
  type: ['depth', 'depth1', 'breadth', 'breadth1', 'breadth4', 'clear']
});

function main({ thousands, type }) {
  const N = thousands * 1e3;
  switch (type) {
    case 'depth':
      depth(N);
      break;
    case 'depth1':
      depth1(N);
      break;
    case 'breadth':
      breadth(N);
      break;
    case 'breadth1':
      breadth1(N);
      break;
    case 'breadth4':
      breadth4(N);
      break;
    case 'clear':
      clear(N);
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
      bench.end(N / 1e3);
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
      bench.end(N / 1e3);
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
      bench.end(N / 1e3);
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
      bench.end(N / 1e3);
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
      bench.end(N / 1e3);
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
      bench.end(N / 1e3);
  }
  for (var i = 0; i < N; i++) {
    clearImmediate(setImmediate(cb, 1));
  }
  setImmediate(cb, 2);
}
