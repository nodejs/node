import { workerData, wire } from 'node:worker_threads';

wire(workerData.data, {
  fail(arg) {
    return new Promise((resolve, reject) => {
      // nothing to do here, we will fail
    });
  },
});