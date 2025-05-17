import { workerData, wire } from 'node:worker_threads';

wire(workerData.data, {
  async echo(arg) {
    return arg;
  },
});

// Keep the event loop alive
setInterval(() => {}, 100000);