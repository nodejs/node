'use strict';

require('../common');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function writeFile(name, buf) {
  const p = path.join(tmpdir.path, name);
  fs.writeFileSync(p, buf);
  return p;
}

function expectMatches(filePath, rawBuf) {
  assert.strictEqual(
    fs.readFileSync(filePath, 'utf8'),
    rawBuf.toString('utf8'),
  );
}

describe('fs.readFileSync utf8 simdutf dispatch', () => {
  it('empty file', () => {
    const p = writeFile('empty.txt', Buffer.alloc(0));
    assert.strictEqual(fs.readFileSync(p, 'utf8'), '');
  });

  it('ascii small', () => {
    const buf = Buffer.from('hello');
    expectMatches(writeFile('tiny-ascii.txt', buf), buf);
  });

  it('ascii 20KB', () => {
    const buf = Buffer.alloc(20 * 1024, 0x41);
    expectMatches(writeFile('medium-ascii.txt', buf), buf);
  });

  it('ascii 1MB', () => {
    const buf = Buffer.alloc(1024 * 1024, 0x61);
    expectMatches(writeFile('large-ascii.txt', buf), buf);
  });

  it('fd input', () => {
    const buf = Buffer.alloc(50 * 1024, 0x62);
    const p = writeFile('fd-ascii.txt', buf);
    const fd = fs.openSync(p, 'r');
    try {
      assert.strictEqual(fs.readFileSync(fd, 'utf8'), buf.toString('utf8'));
    } finally {
      fs.closeSync(fd);
    }
  });

  it('multibyte UTF-8', () => {
    const buf = Buffer.from('中文测试 — café — 🚀'.repeat(500), 'utf8');
    expectMatches(writeFile('multibyte.txt', buf), buf);
  });

  it('latin1-fits utf8', () => {
    const buf = Buffer.from('naïve café résumé — niño Köln '.repeat(500), 'utf8');
    expectMatches(writeFile('latin1-fits.txt', buf), buf);
  });

  it('invalid: lone continuation byte', () => {
    const buf = Buffer.from([0x68, 0x69, 0x80, 0x21]);
    expectMatches(writeFile('invalid-cont.txt', buf), buf);
  });

  it('invalid: overlong', () => {
    const buf = Buffer.from([0x41, 0xC0, 0xAF, 0x42]);
    expectMatches(writeFile('invalid-overlong.txt', buf), buf);
  });

  it('invalid: surrogate', () => {
    const buf = Buffer.from([0x41, 0xED, 0xA0, 0x80, 0x42]);
    expectMatches(writeFile('invalid-surrogate.txt', buf), buf);
  });

  it('latin1 boundary U+00FF', () => {
    const buf = Buffer.from('ÿ'.repeat(2048), 'utf8');
    expectMatches(writeFile('latin1-boundary.txt', buf), buf);
  });

  it('above latin1 U+0100', () => {
    const buf = Buffer.from('ĀāĂ'.repeat(1024), 'utf8');
    expectMatches(writeFile('above-latin1.txt', buf), buf);
  });

  it('single codepoint each UTF-8 length', () => {
    for (const cp of [0x41, 0x00E9, 0x4E2D, 0x1F600]) {
      const buf = Buffer.from(String.fromCodePoint(cp), 'utf8');
      expectMatches(writeFile(`single-cp-${cp.toString(16)}.txt`, buf), buf);
    }
  });

  it('truncated multibyte at EOF', () => {
    const buf = Buffer.from([0x41, 0xE4, 0xB8]);
    expectMatches(writeFile('truncated-multibyte.txt', buf), buf);
  });
});
