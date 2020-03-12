'use strict';
const common = require('../common');
const Countdown = require('../common/countdown');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const http = require('http');
const cls = new AsyncLocalStorage();
const NUM_CLIENTS = 10;

// Run multiple clients that receive data from a server
// in multiple chunks, in a single non-closure function.
// Use the AsyncLocalStorage (ALS) APIs to maintain the context
// and data download. Make sure that individual clients
// receive their respective data, with no conflicts.

// Set up a server that sends large buffers of data, filled
// with cardinal numbers, increasing per request
let index = 0;
const server = http.createServer((q, r) => {
  // Send a large chunk as response, otherwise the data
  // may be sent in a single chunk, and the callback in the
  // client may be called only once, defeating the purpose of test
  r.end((index++ % 10).toString().repeat(1024 * 1024));
});

const countdown = new Countdown(NUM_CLIENTS, () => {
  server.close();
});

server.listen(0, common.mustCall(() => {
  for (let i = 0; i < NUM_CLIENTS; i++) {
    cls.run(new Map(), common.mustCall(() => {
      const options = { port: server.address().port };
      const req = http.get(options, common.mustCall((res) => {
        const store = cls.getStore();
        store.set('data', '');

        // Make ondata and onend non-closure
        // functions and fully dependent on ALS
        res.setEncoding('utf8');
        res.on('data', ondata);
        res.on('end', common.mustCall(onend));
      }));
      req.end();
    }));
  }
}));

// Accumulate the current data chunk with the store data
function ondata(d) {
  const store = cls.getStore();
  assert.notStrictEqual(store, undefined);
  let chunk = store.get('data');
  chunk += d;
  store.set('data', chunk);
}

// Retrieve the store data, and test for homogeneity
function onend() {
  const store = cls.getStore();
  assert.notStrictEqual(store, undefined);
  const data = store.get('data');
  assert.strictEqual(data, data[0].repeat(data.length));
  countdown.dec();
}
