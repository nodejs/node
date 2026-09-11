'use strict';

const assert = require('node:assert');
const crypto = require('node:crypto');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const zlib = require('node:zlib');
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  n: [1e3, 1e4],
  mode: ['subscribed', 'unsubscribed'],
  operation: ['crypto', 'zlib', 'readFile', 'writeFile'],
});

function main({ n, mode, operation }) {
  const subscriber = () => {};
  if (mode === 'subscribed') {
    dc.subscribe('threadpool.work', subscriber);
  } else if (mode === 'unsubscribed') {
    dc.subscribe('threadpool.work', subscriber);
    dc.unsubscribe('threadpool.work', subscriber);
  }

  tmpdir.refresh();
  const file = tmpdir.resolve('file');
  const data = Buffer.alloc(1024, 'x');
  fs.writeFileSync(file, data);

  const operations = {
    crypto(callback) {
      crypto.pbkdf2('secret', 'salt', 1, 32, 'sha256', callback);
    },
    zlib(callback) {
      zlib.gzip(data, callback);
    },
    readFile(callback) {
      fs.readFile(file, callback);
    },
    writeFile(callback) {
      fs.writeFile(file, data, callback);
    },
  };

  let completed = 0;
  bench.start();
  run();

  function run() {
    operations[operation]((err) => {
      assert.ifError(err);
      if (++completed < n) return run();

      bench.end(n);
      if (mode === 'subscribed') {
        dc.unsubscribe('threadpool.work', subscriber);
      }
    });
  }
}
