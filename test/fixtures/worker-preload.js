const {
  Worker,
  workerData,
  threadId
} = require('worker_threads');

if (threadId < 2) {
  new Worker('1 + 1', { eval: true });
}
