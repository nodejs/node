'use strict';

// REALISTIC Node.js timer queue benchmark
// Tests actual production workload: small queue (3-15 items), frequent peek()

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  queueSize: [3, 5, 10, 15, 20],
  impl: ['original', 'adaptive'],
  workload: ['web-server', 'microservice', 'real-time'],
});

function createHeap(impl, comparator, setPosition) {
  if (impl === 'original') {
    const PriorityQueue = require('../../lib/internal/priority_queue');
    return new PriorityQueue(comparator, setPosition);
  } else if (impl === 'adaptive') {
    const PriorityQueueAdaptive = require('../../lib/internal/priority_queue_adaptive');
    return new PriorityQueueAdaptive(comparator, setPosition);
  } else {
    throw new Error(`Unknown implementation: ${impl}`);
  }
}

function benchmarkWebServer(queueSize, impl) {
  // Typical web server: mostly peek(), occasional insert/shift
  // Queue holds 3-5 TimersList objects
  const comparator = (a, b) => a.expiry - b.expiry;
  const setPosition = (node, pos) => { node.pos = pos; };
  const heap = createHeap(impl, comparator, setPosition);

  // Initial queue state
  const timers = [];
  for (let i = 0; i < queueSize; i++) {
    timers.push({
      expiry: 1000 + i * 10000,
      id: i,
      pos: null,
    });
    heap.insert(timers[i]);
  }

  const iterations = 100000;
  bench.start();

  for (let i = 0; i < iterations; i++) {
    const op = Math.random();

    if (op < 0.80) {
      // 80% peek() - ULTRA HOT PATH in processTimers()
      heap.peek();
    } else if (op < 0.90) {
      // 10% shift + insert (timer list expires, new one created)
      heap.shift();
      const newTimer = {
        expiry: 1000 + Math.floor(Math.random() * 100000),
        id: i,
        pos: null,
      };
      heap.insert(newTimer);
    } else {
      // 10% percolateDown (timer list rescheduled)
      const min = heap.peek();
      if (min) {
        min.expiry += 100;
        heap.percolateDown(1);
      }
    }
  }

  bench.end(iterations);
}

function benchmarkMicroservice(queueSize, impl) {
  // Microservice: balanced mix of operations
  const comparator = (a, b) => a.expiry - b.expiry;
  const setPosition = (node, pos) => { node.pos = pos; };
  const heap = createHeap(impl, comparator, setPosition);

  // Initial queue state
  for (let i = 0; i < queueSize; i++) {
    heap.insert({
      expiry: 1000 + i * 5000,
      id: i,
      pos: null,
    });
  }

  const iterations = 100000;
  bench.start();

  for (let i = 0; i < iterations; i++) {
    const op = Math.random();

    if (op < 0.70) {
      // 70% peek()
      heap.peek();
    } else if (op < 0.85) {
      // 15% insert
      heap.insert({
        expiry: 1000 + Math.floor(Math.random() * 60000),
        id: i,
        pos: null,
      });
    } else if (op < 0.95) {
      // 10% shift
      if (heap.size > 0) heap.shift();
    } else {
      // 5% percolateDown
      const min = heap.peek();
      if (min) {
        min.expiry += 50;
        heap.percolateDown(1);
      }
    }
  }

  bench.end(iterations);
}

function benchmarkRealTime(queueSize, impl) {
  // Real-time app: frequent updates, many peek()
  const comparator = (a, b) => a.expiry - b.expiry;
  const setPosition = (node, pos) => { node.pos = pos; };
  const heap = createHeap(impl, comparator, setPosition);

  // Initial queue state
  for (let i = 0; i < queueSize; i++) {
    heap.insert({
      expiry: 50 + i * 1000,
      id: i,
      pos: null,
    });
  }

  const iterations = 100000;
  bench.start();

  for (let i = 0; i < iterations; i++) {
    const op = Math.random();

    if (op < 0.85) {
      // 85% peek() - event loop checking next timer constantly
      heap.peek();
    } else if (op < 0.92) {
      // 7% percolateDown (timer reschedule)
      const min = heap.peek();
      if (min) {
        min.expiry += 10;
        heap.percolateDown(1);
      }
    } else {
      // 8% shift + insert
      if (heap.size > 0) heap.shift();
      heap.insert({
        expiry: 50 + Math.floor(Math.random() * 10000),
        id: i,
        pos: null,
      });
    }
  }

  bench.end(iterations);
}

function main({ queueSize, impl, workload }) {
  switch (workload) {
    case 'web-server':
      benchmarkWebServer(queueSize, impl);
      break;
    case 'microservice':
      benchmarkMicroservice(queueSize, impl);
      break;
    case 'real-time':
      benchmarkRealTime(queueSize, impl);
      break;
    default:
      throw new Error(`Unknown workload: ${workload}`);
  }
}
