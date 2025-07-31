'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: [
    'depth',        // Recursive nextTick (tests latency)
    'breadth',      // Many nextTicks at once (tests throughput)
    'mixed-args',   // Different argument counts (tests pooling)
    'async-await',  // Real-world async/await pattern
    'promise-chain', // Promise chain with nextTick
  ],
});

function main({ n, type }) {
  switch (type) {
    case 'depth':
      benchDepth(n);
      break;
    case 'breadth':
      benchBreadth(n);
      break;
    case 'mixed-args':
      benchMixedArgs(n);
      break;
    case 'async-await':
      benchAsyncAwait(n);
      break;
    case 'promise-chain':
      benchPromiseChain(n);
      break;
  }
}

// Test recursive nextTick (latency focused)
function benchDepth(n) {
  let count = 0;
  bench.start();
  
  function nextTickCallback() {
    count++;
    if (count === n) {
      bench.end(n);
    } else {
      process.nextTick(nextTickCallback);
    }
  }
  
  process.nextTick(nextTickCallback);
}

// Test many concurrent nextTicks (throughput focused)
function benchBreadth(n) {
  let count = 0;
  bench.start();
  
  function callback() {
    count++;
    if (count === n) {
      bench.end(n);
    }
  }
  
  for (let i = 0; i < n; i++) {
    process.nextTick(callback);
  }
}

// Test mixed argument patterns (tests object pooling)
function benchMixedArgs(n) {
  let count = 0;
  bench.start();
  
  function callback0() { 
    count++;
    if (count === n) bench.end(n);
  }
  function callback1(a) { 
    count++;
    if (count === n) bench.end(n);
  }
  function callback2(a, b) { 
    count++;
    if (count === n) bench.end(n);
  }
  function callback4(a, b, c, d) { 
    count++;
    if (count === n) bench.end(n);
  }
  function callback8(a, b, c, d, e, f, g, h) { 
    count++;
    if (count === n) bench.end(n);
  }
  
  const callbacks = [
    () => process.nextTick(callback0),
    () => process.nextTick(callback1, 1),
    () => process.nextTick(callback2, 1, 2),
    () => process.nextTick(callback4, 1, 2, 3, 4),
    () => process.nextTick(callback8, 1, 2, 3, 4, 5, 6, 7, 8),
  ];
  
  for (let i = 0; i < n; i++) {
    callbacks[i % callbacks.length]();
  }
}

// Test real-world async/await pattern
function benchAsyncAwait(n) {
  let count = 0;
  bench.start();
  
  async function asyncTask() {
    return new Promise(resolve => {
      process.nextTick(() => {
        count++;
        if (count === n) {
          bench.end(n);
        }
        resolve(count);
      });
    });
  }
  
  // Start many async tasks
  for (let i = 0; i < n; i++) {
    asyncTask();
  }
}

// Test Promise chain with nextTick (common Node.js pattern)
function benchPromiseChain(n) {
  let count = 0;
  bench.start();
  
  function createPromiseChain() {
    return Promise.resolve()
      .then(() => {
        return new Promise(resolve => {
          process.nextTick(() => {
            count++;
            if (count === n) {
              bench.end(n);
            }
            resolve();
          });
        });
      });
  }
  
  for (let i = 0; i < n; i++) {
    createPromiseChain();
  }
}