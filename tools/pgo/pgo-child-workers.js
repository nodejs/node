'use strict';

// PGO Training Script: Worker Threads
//
// Node.js Worker threads are used for:
// - CPU-intensive parallel work (image processing, parsing, compilation)
// - Thread pool patterns for offloading blocking work
// - SharedArrayBuffer-based parallel computation
// - Inline eval workers (used by some frameworks)
//
// This exercises: Worker thread messaging, structured clone serialization,
// SharedArrayBuffer, Atomics, V8 isolate creation, module loading in workers.
//
// Note: child_process.spawn/fork are excluded from PGO training because each
// spawned process generates its own .profraw file, creating hundreds of
// startup-heavy profiles that dilute the steady-state profile data.

const { Worker, isMainThread } = require('worker_threads');
const path = require('path');
const os = require('os');
const fs = require('fs');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;
const TEMP_DIR = path.join(
  os.tmpdir(),
  `node-pgo-workers-${process.pid}-${Date.now()}`,
);

function setup() {
  fs.mkdirSync(TEMP_DIR, { recursive: true });

  // Write worker script files
  fs.writeFileSync(
    path.join(TEMP_DIR, 'worker-cpu.js'),
    `
    'use strict';
    const { parentPort, workerData } = require('worker_threads');

    // CPU-intensive work: hash computation, JSON processing, regex
    function doWork(data) {
      const crypto = require('crypto');
      const results = [];

      for (let i = 0; i < data.iterations; i++) {
        // Hash computation
        const hash = crypto.createHash('sha256')
          .update(JSON.stringify({ index: i, data: data.payload }))
          .digest('hex');

        // JSON round-trip
        const obj = JSON.parse(JSON.stringify({
          id: i,
          hash,
          timestamp: Date.now(),
          nested: { a: { b: { c: i } } },
        }));

        // Regex processing
        const text = 'The quick brown fox jumps over the lazy dog '.repeat(10);
        const matches = text.match(/\\b\\w{4,}\\b/g) || [];

        results.push({ hash: hash.slice(0, 8), matches: matches.length });
      }

      return { count: results.length, sample: results[0] };
    }

    parentPort.on('message', (msg) => {
      if (msg.type === 'work') {
        const result = doWork(msg.data);
        parentPort.postMessage({ type: 'result', data: result });
      }
      if (msg.type === 'exit') {
        process.exit(0);
      }
    });

    // Also handle direct workerData
    if (workerData && workerData.autoStart) {
      const result = doWork(workerData);
      parentPort.postMessage({ type: 'result', data: result });
    }
  `,
  );

  fs.writeFileSync(
    path.join(TEMP_DIR, 'worker-shared.js'),
    `
    'use strict';
    const { parentPort, workerData } = require('worker_threads');

    // Shared memory worker: operates on SharedArrayBuffer
    const { buffer, offset, length } = workerData;
    const view = new Int32Array(buffer);

    // Process assigned segment
    for (let i = offset; i < offset + length; i++) {
      // Atomic operations
      Atomics.add(view, i, 1);
      Atomics.load(view, i);
    }

    parentPort.postMessage({ done: true, processed: length });
  `,
  );
}

function cleanup() {
  try {
    fs.rmSync(TEMP_DIR, { recursive: true, force: true });
  } catch {
    // best effort
  }
}

// Workload 1: Worker threads — message passing (the dominant pattern)
async function workloadWorkerMessages(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const worker = new Worker(path.join(TEMP_DIR, 'worker-cpu.js'));

    await new Promise((resolve, reject) => {
      worker.on('message', (msg) => {
        if (msg.type === 'result') {
          ops++;
          worker.postMessage({ type: 'exit' });
        }
      });
      worker.on('exit', resolve);
      worker.on('error', reject);

      // Send work
      worker.postMessage({
        type: 'work',
        data: { iterations: 50, payload: `batch_${i}` },
      });
    });
  }
  return ops;
}

// Workload 2: Worker threads — workerData initialization
async function workloadWorkerData(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const worker = new Worker(path.join(TEMP_DIR, 'worker-cpu.js'), {
      workerData: { autoStart: true, iterations: 30, payload: `init_${i}` },
    });

    await new Promise((resolve, reject) => {
      worker.on('message', (msg) => {
        if (msg.type === 'result') {
          ops++;
          worker.terminate();
        }
      });
      worker.on('exit', resolve);
      worker.on('error', reject);
    });
  }
  return ops;
}

// Workload 3: Worker threads — SharedArrayBuffer (parallel computation)
async function workloadSharedMemory(iterations) {
  let ops = 0;
  const ARRAY_SIZE = 1024;
  const NUM_WORKERS = Math.min(4, os.cpus().length);

  for (let i = 0; i < iterations; i++) {
    const sharedBuffer = new SharedArrayBuffer(ARRAY_SIZE * 4); // Int32Array
    const segmentSize = Math.floor(ARRAY_SIZE / NUM_WORKERS);

    const workers = [];
    const promises = [];

    for (let w = 0; w < NUM_WORKERS; w++) {
      const worker = new Worker(path.join(TEMP_DIR, 'worker-shared.js'), {
        workerData: {
          buffer: sharedBuffer,
          offset: w * segmentSize,
          length: segmentSize,
        },
      });

      const promise = new Promise((resolve, reject) => {
        worker.on('message', (msg) => {
          ops++;
          resolve(msg);
        });
        worker.on('error', reject);
        worker.on('exit', () => resolve());
      });

      workers.push(worker);
      promises.push(promise);
    }

    await Promise.all(promises);
  }
  return ops;
}

// Workload 4: Worker from inline code (eval pattern — used by some frameworks)
async function workloadInlineWorker(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const code = `
      const { parentPort } = require('worker_threads');
      const crypto = require('crypto');
      const result = crypto.createHash('sha256').update('data-${i}').digest('hex');
      parentPort.postMessage({ hash: result });
    `;

    const worker = new Worker(code, { eval: true });

    await new Promise((resolve, reject) => {
      worker.on('message', (msg) => {
        ops++;
        resolve(msg);
      });
      worker.on('error', reject);
      worker.on('exit', () => resolve());
    });
  }
  return ops;
}

async function main() {
  if (!isMainThread) return; // Guard against being loaded as worker

  console.log('[pgo-child-workers] Starting worker thread workload...');

  setup();
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  try {
    while (remaining() > 0) {
      round++;
      const scale = Math.max(0.1, remaining() / DURATION_MS);
      const iterScale = (base) => Math.max(1, Math.floor(base * scale));

      // Worker threads (most impactful for PGO)
      if (round === 1)
        console.log('[pgo-child-workers] Running worker messages...');
      totalOps += await workloadWorkerMessages(iterScale(10));
      if (remaining() <= 0) break;

      if (round === 1)
        console.log('[pgo-child-workers] Running workerData init...');
      totalOps += await workloadWorkerData(iterScale(10));
      if (remaining() <= 0) break;

      if (round === 1)
        console.log('[pgo-child-workers] Running shared memory workers...');
      totalOps += await workloadSharedMemory(iterScale(5));
      if (remaining() <= 0) break;

      if (round === 1)
        console.log('[pgo-child-workers] Running inline workers...');
      totalOps += await workloadInlineWorker(iterScale(10));
    }

    const elapsed = (Date.now() - startTime) / 1000;
    console.log(
      `[pgo-child-workers] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
    );
  } finally {
    cleanup();
  }
}

main().catch((err) => {
  console.error('[pgo-child-workers] Error:', err);
  cleanup();
  process.exit(1);
});
