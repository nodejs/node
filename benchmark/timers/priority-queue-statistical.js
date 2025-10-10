'use strict';

// Statistical benchmark with variance/stddev reporting
// Runs multiple trials to ensure reproducibility

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  queueSize: [5, 10, 20, 100, 1000, 10000],
  impl: ['original', 'adaptive'],
  trials: [10],
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

function runTrial(queueSize, impl) {
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
  const startTime = process.hrtime.bigint();

  for (let i = 0; i < iterations; i++) {
    const op = (i % 100) / 100; // Deterministic pattern

    if (op < 0.80) {
      // 80% peek()
      heap.peek();
    } else if (op < 0.90) {
      // 10% shift + insert
      heap.shift();
      const newTimer = {
        expiry: 1000 + ((i * 12345) % 100000), // Deterministic random
        id: i,
        pos: null,
      };
      heap.insert(newTimer);
    } else {
      // 10% percolateDown
      const min = heap.peek();
      if (min) {
        min.expiry += 100;
        heap.percolateDown(1);
      }
    }
  }

  const endTime = process.hrtime.bigint();
  const durationNs = Number(endTime - startTime);
  const opsPerSec = (iterations / durationNs) * 1e9;

  return opsPerSec;
}

function mean(values) {
  return values.reduce((a, b) => a + b, 0) / values.length;
}

function stddev(values) {
  const avg = mean(values);
  const squareDiffs = values.map((value) => Math.pow(value - avg, 2));
  const avgSquareDiff = mean(squareDiffs);
  return Math.sqrt(avgSquareDiff);
}

function main({ queueSize, impl, trials }) {
  const results = [];

  // Run multiple trials
  for (let i = 0; i < trials; i++) {
    results.push(runTrial(queueSize, impl));
  }

  const avg = mean(results);
  const std = stddev(results);
  const cv = (std / avg) * 100; // Coefficient of variation

  bench.start();
  // Report mean as the primary metric
  bench.end(avg);

  // Log statistical info (will appear in benchmark output)
  console.error(`n=${queueSize} impl=${impl}: ${avg.toFixed(0)} ops/sec ` +
                `(stddev=${std.toFixed(0)}, cv=${cv.toFixed(2)}%)`);
}
