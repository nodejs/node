'use strict';
const { Buffer } = require('buffer');
const { ERR_INTERNAL_ASSERTION } = require('internal/errors').codes;

const child_process = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const https = require('https');
const zlib = require('zlib');

const { ObjectAssign } = primordials;

const EXE = (process.platform === 'win32') ? 'deno.exe' : 'deno';
const TRIPLET = triplet(process.arch, process.platform);
const VERSION = '1.4.3';

const DIGESTS = {
  'x86_64-apple-darwin':
    '94a33573327d41b68c7904e286755ede88747ed21c544d62314a60b23982c474',
  'x86_64-pc-windows-msvc':
    '7f691bd901ae90e1f902ed62543fcd021c35882e299bb55c27e44747f00a5313',
  'x86_64-unknown-linux-gnu':
    '3b8234a59dee9d8e7a150ddb7d1963ace991ff442b366ead954752f35081985f',
};

function deno(args, opts, cb) {
  if (typeof args === 'function') cb = args, args = [], opts = {};
  if (typeof opts === 'function') cb = opts, opts = {};
  if (typeof cb !== 'function') cb = () => {};

  const defaults = {
    download: './' + EXE,
    exe: EXE,
    stdio: 'inherit',
  };
  opts = ObjectAssign(defaults, opts);

  const onexit = (code, sig) => cb(null, code, sig);
  const proc = child_process.spawn(opts.exe, args, opts);
  proc.once('exit', onexit);

  proc.once('error', (err) => {
    if (err.code !== 'ENOENT') return cb(err);
    download(opts.download, (err) => {
      if (err) return cb(err);
      child_process.spawn(opts.download, args, opts).once('exit', onexit);
    });
  });
}

function download(filename, cb) {
  const url =
    'https://github.com/denoland/deno' +
    `/releases/download/v${VERSION}/deno-${TRIPLET}.zip`;

  https.get(url, (res) => {
    const url = res.headers.location || '';

    if (!url.startsWith('https://'))
      return cb(new ERR_INTERNAL_ASSERTION('redirect expected'));

    https.get(url, follow);
  });

  function follow(res) {
    const hasher = crypto.createHash('sha256');
    const writer = fs.createWriteStream(filename, { flags: 'wx', mode: 0o755 });
    writer.once('error', cb);

    res.pipe(unzipper()).pipe(writer).once('finish', next);
    res.pipe(hasher).once('finish', next);
    res.once('error', cb);

    function next() {
      if (++next.calls !== 2) return;
      const actual = hasher.digest('hex');
      const expected = DIGESTS[TRIPLET];
      let err;
      if (actual !== expected) {
        err = new ERR_INTERNAL_ASSERTION(
          `Checksum mismatch for ${url}. ` +
          `Expected ${expected}, actual ${actual}`);
        try {
          fs.unlinkSync(filename);
        } catch {
          // Ignore.
        }
      }
      cb(err);
    }
    next.calls = 0;
  }
}

// Deflate a single-file PKZIP archive.
function unzipper() {
  class Unzipper extends zlib.InflateRaw {
    buf = Buffer.alloc(0)

    _transform(chunk, encoding, cb) {
      if (this.buf) {
        const b = this.buf = Buffer.concat([this.buf, chunk]);

        if (b.length < 2) return cb();

        // ZIP files start with the bytes "PK".
        if (b[0] !== 0x50 && b[1] !== 0x4B)
          return cb(new ERR_INTERNAL_ASSERTION('bad magic'));

        if (b.length < 30) return cb();

        // Skip filename and extra data.
        let n = 30;
        n += b[26] + b[27] * 256;
        n += b[28] + b[29] * 256;

        if (b.length < n) return cb();

        chunk = b.slice(n);
        this.buf = null;  // Switch to passthrough mode.
      }
      return super._transform(chunk, encoding, cb);
    }
  }

  return new Unzipper();
}

function triplet(arch, os) {
  if (arch === 'x64') {
    if (os === 'darwin') return 'x86_64-apple-darwin';
    if (os === 'linux') return 'x86_64-unknown-linux-gnu';
    if (os === 'win32') return 'x86_64-pc-windows-msvc';
  }
  throw new ERR_INTERNAL_ASSERTION(`Unsupported arch/os: ${arch}/${os}`);
}

module.exports = deno;
