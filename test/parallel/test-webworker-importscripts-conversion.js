// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
const assert = require('node:assert');

for (const type of ['classic', 'module']) {
  const source = `
    const results = [];
    const conversions = [];
    const sentinel = new Error('conversion');
    try {
      importScripts('https://[', {
        toString() { conversions.push('converted'); throw sentinel; }
      });
    } catch (error) {
      results.push([conversions, error === sentinel]);
    }
    const order = [];
    try {
      importScripts(
        { toString() { order.push(1); return 'https://['; } },
        { toString() { order.push(2); return 'data:text/javascript,'; } }
      );
    } catch (error) {
      results.push([order, error.name]);
    }
    postMessage(results);
  `;
  const worker = new Worker(`data:text/javascript,${encodeURIComponent(source)}`, { type });
  worker.onerror = common.mustNotCall('worker failed');
  worker.onmessage = common.mustCall(({ data }) => {
    worker.terminate();
    assert.deepStrictEqual(data, [
      [['converted'], true],
      [[1, 2], type === 'module' ? 'TypeError' : 'SyntaxError'],
    ]);
  });
}

if (common.hasCrypto) {
  // URL parsing must capture blob entries after all arguments are converted.
  const source = `
    self.ran = false;
    const url = URL.createObjectURL(new Blob(['self.ran = true'], {
      type: 'text/javascript'
    }));
    let errorName;
    try {
      importScripts(url, {
        toString() {
          URL.revokeObjectURL(url);
          return 'data:text/javascript,';
        }
      });
    } catch (error) {
      errorName = error.name;
    }
    postMessage([self.ran, errorName]);
  `;
  const worker = new Worker(`data:text/javascript,${encodeURIComponent(source)}`);
  worker.onerror = common.mustNotCall('worker failed');
  worker.onmessage = common.mustCall(({ data }) => {
    worker.terminate();
    assert.deepStrictEqual(data, [false, 'NetworkError']);
  });
}
