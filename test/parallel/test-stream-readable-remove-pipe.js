'use strict';

const common = require('../common');

// This test ensures that if we remove last 'readable' listener
// on Readable instance that is piped it will not disrupt the pipe.

const assert = require('assert');
const { Readable, Writable } = require('stream');

let receivedData = '';
const w = new Writable({
  write: (chunk, env, callback) => {
    receivedData += chunk;
    callback();
  },
});

const data = ['foo', 'bar', 'baz'];
const r = new Readable({
  read: () => {},
});

const listener = common.mustNotCall();
r.on('readable', listener);

r.pipe(w);
r.push(data[0]);

r.removeListener('readable', listener);

process.nextTick(() => {
  r.push(data[1]);
  r.push(data[2]);
  r.push(null);
});

w.on('finish', common.mustCall(() => {
  assert.strictEqual(receivedData, data.join(''));
}));
