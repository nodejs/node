'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.env.isChild === '1') {
  const {
    checkPrimeSync,
    generatePrimeSync,
    generateKeySync,
    generateKeyPairSync,
    hkdfSync,
    pbkdf2Sync,
    randomFillSync,
    scryptSync,
    randomInt
  } = require('crypto');
  // Call some sync APIs
  checkPrimeSync(generatePrimeSync(32));
  generateKeySync('aes', { length: 128 });
  generateKeyPairSync('x25519');
  hkdfSync('sha512', 'key', 'salt', 'info', 64);
  pbkdf2Sync('secret', 'salt', 100000, 64, 'sha512');
  randomFillSync(Buffer.alloc(8));
  scryptSync('pass', 'salt', 1);
  // Call a async API which will not introduce trace event data
  randomInt(3, () => {});
  return;
}

tmpdir.refresh();
const FILE_NAME = path.join(tmpdir.path, 'node_trace.1.log');

const proc = cp.spawn(process.execPath,
                      [
                        '--trace-events-enabled',
                        '--trace-event-categories',
                        'node.crypto.sync',
                        __filename,
                      ],
                      {
                        cwd: tmpdir.path,
                        env: {
                          ...process.env,
                          isChild: '1',
                        }
                      });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    assert(!err);
    const traces = JSON.parse(data.toString()).traceEvents;
    assert(traces.length > 0);
    let count = 0;
    traces.forEach((trace) => {
      if (trace.cat === 'node,node.crypto,node.crypto.sync') {
        count++;
      }
    });
    // Each API introduces two events.
    assert.strictEqual(count, 8 * 2);
  }));
}));
