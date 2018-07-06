'use strict';

const common = require('../common');

// This test ensures that if we have both 'readable' and 'data'
// listeners on Readable instance once the 'readable' listeners
// are gone and there are still 'data' listeners stream will try
// to flow to satisfy the 'data' listeners.

const assert = require('assert');
const { Readable } = require('stream');

const r = new Readable({
  read: () => {},
});

const data = ['foo', 'bar', 'baz'];

let receivedData = '';
r.once('readable', common.mustCall());
r.on('data', (chunk) => receivedData += chunk);
r.once('end', common.mustCall(() => {
  assert.strictEqual(receivedData, data.join(''));
}));

r.push(data[0]);
r.push(data[1]);
r.push(data[2]);
r.push(null);
