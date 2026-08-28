import common from '../../common/index.js';

const pending = new Map();
const onMessage = (message) => {
  if (message?.type !== 'node:bench:ack') return;
  pending.get(message.id)?.();
  pending.delete(message.id);
};
process.on('message', onMessage);

const timeout = setTimeout(() => {
  throw new Error('benchmark record was not acknowledged');
}, common.platformTimeout(10_000));

function sendDiagnostic(id) {
  return new Promise((resolve, reject) => {
    pending.set(id, resolve);
    process.send({
      id,
      type: 'node:bench:record',
      record: {
        type: 'bench:diagnostic',
        data: {
          runId: process.env.NODE_BENCH_RUN_ID,
          fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
          entryFile: process.argv[1],
          message: `acknowledged ${id}`,
          level: 'info',
          file: process.argv[1],
        },
      },
    }, (error) => {
      if (error) reject(error);
    });
  });
}

for (let i = 0; i < 32; i++) await sendDiagnostic(10_000 + i);

clearTimeout(timeout);
process.off('message', onMessage);
