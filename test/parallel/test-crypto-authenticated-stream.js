'use strict';
// Refs: https://github.com/nodejs/node/issues/31733
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const stream = require('stream');
const tmpdir = require('../common/tmpdir');

class Sink extends stream.Writable {
  constructor() {
    super();
    this.chunks = [];
  }

  _write(chunk, encoding, cb) {
    this.chunks.push(chunk);
    cb();
  }
}

function direct(config) {
  const { cipher, key, iv, aad, authTagLength, plaintextLength } = config;
  const expected = Buffer.alloc(plaintextLength);

  const c = crypto.createCipheriv(cipher, key, iv, { authTagLength });
  c.setAAD(aad, { plaintextLength });
  const ciphertext = Buffer.concat([c.update(expected), c.final()]);

  const d = crypto.createDecipheriv(cipher, key, iv, { authTagLength });
  d.setAAD(aad, { plaintextLength });
  d.setAuthTag(c.getAuthTag());
  const actual = Buffer.concat([d.update(ciphertext), d.final()]);

  assert.deepStrictEqual(expected, actual);
}

function mstream(config) {
  const { cipher, key, iv, aad, authTagLength, plaintextLength } = config;
  const expected = Buffer.alloc(plaintextLength);

  const c = crypto.createCipheriv(cipher, key, iv, { authTagLength });
  c.setAAD(aad, { plaintextLength });

  const plain = new stream.PassThrough();
  const crypt = new Sink();
  const chunks = crypt.chunks;
  plain.pipe(c).pipe(crypt);
  plain.end(expected);

  crypt.on('close', common.mustCall(() => {
    const d = crypto.createDecipheriv(cipher, key, iv, { authTagLength });
    d.setAAD(aad, { plaintextLength });
    d.setAuthTag(c.getAuthTag());

    const crypt = new stream.PassThrough();
    const plain = new Sink();
    crypt.pipe(d).pipe(plain);
    for (const chunk of chunks) crypt.write(chunk);
    crypt.end();

    plain.on('close', common.mustCall(() => {
      const actual = Buffer.concat(plain.chunks);
      assert.deepStrictEqual(expected, actual);
    }));
  }));
}

function fstream(config) {
  const count = fstream.count++;
  const filename = (name) => tmpdir.resolve(`${name}${count}`);

  const { cipher, key, iv, aad, authTagLength, plaintextLength } = config;
  const expected = Buffer.alloc(plaintextLength);
  fs.writeFileSync(filename('a'), expected);

  const c = crypto.createCipheriv(cipher, key, iv, { authTagLength });
  c.setAAD(aad, { plaintextLength });

  const plain = fs.createReadStream(filename('a'));
  const crypt = fs.createWriteStream(filename('b'));
  plain.pipe(c).pipe(crypt);

  // Observation: 'close' comes before 'end' on |c|, which definitely feels
  // wrong. Switching to `c.on('end', ...)` doesn't fix the test though.
  crypt.on('close', common.mustCall(() => {
    // Just to drive home the point that decryption does actually work:
    // reading the file synchronously, then decrypting it, works.
    {
      const ciphertext = fs.readFileSync(filename('b'));
      const d = crypto.createDecipheriv(cipher, key, iv, { authTagLength });
      d.setAAD(aad, { plaintextLength });
      d.setAuthTag(c.getAuthTag());
      const actual = Buffer.concat([d.update(ciphertext), d.final()]);
      assert.deepStrictEqual(expected, actual);
    }

    const d = crypto.createDecipheriv(cipher, key, iv, { authTagLength });
    d.setAAD(aad, { plaintextLength });
    d.setAuthTag(c.getAuthTag());

    const crypt = fs.createReadStream(filename('b'));
    const plain = fs.createWriteStream(filename('c'));
    crypt.pipe(d).pipe(plain);

    plain.on('close', common.mustCall(() => {
      const actual = fs.readFileSync(filename('c'));
      assert.deepStrictEqual(expected, actual);
    }));
  }));
}
fstream.count = 0;

function test(config) {
  direct(config);
  mstream(config);
  fstream(config);
}

tmpdir.refresh();

test({
  cipher: 'aes-128-ccm',
  aad: Buffer.alloc(1),
  iv: Buffer.alloc(8),
  key: Buffer.alloc(16),
  authTagLength: 16,
  plaintextLength: 32768,
});

test({
  cipher: 'aes-128-ccm',
  aad: Buffer.alloc(1),
  iv: Buffer.alloc(8),
  key: Buffer.alloc(16),
  authTagLength: 16,
  plaintextLength: 32769,
});
