'use strict';

const common = require('../common.js');
const assert = require('assert');
const {
  generateKeyPair,
  generateKeyPairSync,
} = require('crypto');

const bench = common.createBenchmark(main, {
  method: ['rsaSync', 'rsaAsync', 'dsaSync', 'dsaAsync'],
  n: [1e2],
});

const methods = {
  rsaSync(n) {
    bench.start();
    for (let i = 0; i < n; ++i) {
      generateKeyPairSync('rsa', {
        modulusLength: 1024,
        publicExponent: 0x10001,
      });
    }
    bench.end(n);
  },

  rsaAsync(n) {
    let remaining = n;
    function done(err) {
      assert.ifError(err);
      if (--remaining === 0)
        bench.end(n);
    }
    bench.start();
    for (let i = 0; i < n; ++i)
      generateKeyPair('rsa', {
        modulusLength: 512,
        publicExponent: 0x10001,
      }, done);
  },

  dsaSync(n) {
    bench.start();
    for (let i = 0; i < n; ++i) {
      generateKeyPairSync('dsa', {
        modulusLength: 1024,
        divisorLength: 160,
      });
    }
    bench.end(n);
  },

  dsaAsync(n) {
    let remaining = n;
    function done(err) {
      assert.ifError(err);
      if (--remaining === 0)
        bench.end(n);
    }
    bench.start();
    for (let i = 0; i < n; ++i)
      generateKeyPair('dsa', {
        modulusLength: 1024,
        divisorLength: 160,
      }, done);
  },
};

function main({ n, method }) {
  methods[method](n);
}
