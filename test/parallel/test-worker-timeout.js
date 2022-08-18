'use strict';
const common = require('../common');
const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {

  {
    const w = new Worker(__filename, { workerData: { type: '1' }, timeout: 3000 });
    w.on('timeout', common.mustCall());
    w.on('exit', common.mustCall());
  }
  {
    const w = new Worker(__filename, { workerData: { type: '2' }, timeout: 60 * 1000 });
    w.on('timeout', common.mustNotCall());
    w.on('exit', common.mustCall());
  }
  {
    const w = new Worker(__filename, { workerData: { type: '3' } });
    w.on('timeout', common.mustNotCall());
    w.on('exit', common.mustCall());
  }

} else {
  const { type } = workerData;
  if (type === '1') {
    setInterval(() => {}, 1000);
  } else if (type === '2') {
    // Nothing
  } else if (type === '3') {
    setTimeout(() => {}, 1000);
  }
}
