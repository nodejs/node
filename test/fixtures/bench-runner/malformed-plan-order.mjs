import { bench } from 'node:bench';
import common from '../../common/index.js';

await new Promise((resolve, reject) => {
  const timeout = setTimeout(() => {
    reject(new Error('benchmark summary was not acknowledged'));
  }, common.platformTimeout(10_000));
  process.on('message', (message) => {
    if (message?.type === 'node:bench:ack' && message.id === 0) {
      clearTimeout(timeout);
      resolve();
    }
  });
  process.send?.({
    id: 0,
    type: 'node:bench:record',
    record: {
      type: 'bench:summary',
      data: {
        counts: { completed: 0, failed: 0, skipped: 0, total: 0 },
        duration_ns: 1n,
        entryFile: import.meta.filename,
        fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
        file: import.meta.filename,
        runId: process.env.NODE_BENCH_RUN_ID,
        success: true,
      },
    },
  });
});

bench('late plan', { samples: 1 }, () => {
  throw new Error('late plan benchmark ran');
});
