'use strict';

const common = require('../common');

// Regression tests for https://github.com/nodejs/node/issues/40693

const assert = require('assert');
const dgram = require('dgram');
const { AsyncLocalStorage } = require('async_hooks');

dgram.createSocket('udp4')
  .on('message', function(msg, rinfo) { this.send(msg, rinfo.port); })
  .on('listening', common.mustCall(function() {
    const asyncLocalStorage = new AsyncLocalStorage();
    const store = { val: 'abcd' };
    asyncLocalStorage.run(store, common.mustCall(() => {
      const client = dgram.createSocket('udp4');
      client.on('message', common.mustCall((msg, rinfo) => {
        assert.deepStrictEqual(asyncLocalStorage.getStore(), store);
        client.close();
        this.close();
      }));
      client.send('Hello, world!', this.address().port);
    }));
  }))
  .bind(0);
