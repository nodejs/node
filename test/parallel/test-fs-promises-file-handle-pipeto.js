// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const {
  pipeTo,
} = require('stream/iter');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const tmpDir = tmpdir.path;


// =============================================================================
// pipeTo() with transforms - uppercase through writer
// =============================================================================

async function testPipeToWithTransform() {
  const srcPath = path.join(tmpDir, 'writer-transform-src.txt');
  const dstPath = path.join(tmpDir, 'writer-transform-dst.txt');
  const data = 'hello world from transforms test\n'.repeat(200);
  fs.writeFileSync(srcPath, data);

  function uppercase(chunks) {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let i = 0; i < chunks.length; i++) {
      const src = chunks[i];
      const buf = Buffer.allocUnsafe(src.length);
      for (let j = 0; j < src.length; j++) {
        const b = src[j];
        buf[j] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[i] = buf;
    }
    return out;
  }

  const rfh = await open(srcPath, 'r');
  const wfh = await open(dstPath, 'w');
  const w = wfh.writer();

  await pipeTo(rfh.pull(), uppercase, w);

  await rfh.close();
  await wfh.close();

  assert.strictEqual(fs.readFileSync(dstPath, 'utf8'), data.toUpperCase());
}


// =============================================================================
// pipeTo() integration - pipe source through writer
// =============================================================================

async function testPipeToIntegration() {
  const srcPath = path.join(tmpDir, 'writer-pipeto-src.txt');
  const dstPath = path.join(tmpDir, 'writer-pipeto-dst.txt');
  const data = 'The quick brown fox jumps over the lazy dog.\n'.repeat(500);
  fs.writeFileSync(srcPath, data);

  const rfh = await open(srcPath, 'r');
  const wfh = await open(dstPath, 'w');
  const w = wfh.writer();

  const totalBytes = await pipeTo(rfh.pull(), w);

  await rfh.close();
  await wfh.close();

  assert.strictEqual(totalBytes, Buffer.byteLength(data));
  assert.strictEqual(fs.readFileSync(dstPath, 'utf8'), data);
}


Promise.all([
  testPipeToIntegration(),
  testPipeToWithTransform(),
]).then(common.mustCall());
