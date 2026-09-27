// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const source = `
  import value from 'data:text/javascript,export default 42';
  postMessage([import.meta.url, location.href, value]);
`;
const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
const worker = new Worker(url, { type: 'module' });
URL.revokeObjectURL(url);
worker.onerror = common.mustNotCall('worker failed');
worker.onmessage = common.mustCall(({ data }) => {
  worker.terminate();
  assert.deepStrictEqual(data, [url, url, 42]);
});
