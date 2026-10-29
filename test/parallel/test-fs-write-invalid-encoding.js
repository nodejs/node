'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

async function main() {
  const file = path.join(tmpdir.path, 'write-invalid-encoding');
  const handle = await fs.promises.open(file, 'w+');
  try {
    for (const encoding of ['bad', 16]) {
      const error = {
        code: 'ERR_UNKNOWN_ENCODING',
        message: `Unknown encoding: ${encoding}`,
      };
      assert.throws(() => fs.writeSync(handle.fd, '6162', 0, encoding), error);
      assert.throws(
        () => fs.write(handle.fd, '6162', 0, encoding, common.mustNotCall()),
        error);
      await assert.rejects(handle.write('6162', 0, encoding), error);
    }
    // Rejected writes must not change the file.
    assert.strictEqual((await handle.stat()).size, 0);

    for (const encoding of [undefined, null, '', 'utf8', 'UTF-8', 'hex']) {
      const expected = encoding === 'hex' ? 'ab' : '6162';
      await handle.truncate(0);
      assert.strictEqual(fs.writeSync(handle.fd, '6162', 0, encoding),
                         expected.length);
      assert.strictEqual(fs.readFileSync(file, 'utf8'), expected);

      await handle.truncate(0);
      await new Promise((resolve, reject) => {
        fs.write(handle.fd, '6162', 0, encoding, common.mustCall((err, written) => {
          if (err) return reject(err);
          assert.strictEqual(written, expected.length);
          resolve();
        }));
      });
      assert.strictEqual(fs.readFileSync(file, 'utf8'), expected);

      await handle.truncate(0);
      const { bytesWritten } = await handle.write('6162', 0, encoding);
      assert.strictEqual(bytesWritten, expected.length);
      assert.strictEqual(fs.readFileSync(file, 'utf8'), expected);
    }
  } finally {
    await handle.close();
  }
}

main().then(common.mustCall());
