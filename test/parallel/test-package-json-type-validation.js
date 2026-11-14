'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const { describe, it } = require('node:test');


describe('package.json type field validation', () => {
  it('should warn for invalid type field values', async () => {
    tmpdir.refresh();

    fs.mkdirSync(tmpdir.resolve('invalid-type-pkg'), { recursive: true });
    fs.writeFileSync(
      tmpdir.resolve('invalid-type-pkg/package.json'),
      JSON.stringify({ type: 'CommonJS' })
    );
    fs.writeFileSync(
      tmpdir.resolve('invalid-type-pkg/index.js'),
      'module.exports = 42;'
    );

    const { stderr } = await common.spawnPromisified(process.execPath, [
      tmpdir.resolve('invalid-type-pkg/index.js'),
    ]);

    assert.ok(stderr.includes('MODULE_INVALID_TYPE'));
    assert.ok(stderr.includes('CommonJS'));
  });

  it('should not warn for valid type values', async () => {
    tmpdir.refresh();

    fs.mkdirSync(tmpdir.resolve('valid-type-pkg'), { recursive: true });
    fs.writeFileSync(
      tmpdir.resolve('valid-type-pkg/package.json'),
      JSON.stringify({ type: 'commonjs' })
    );
    fs.writeFileSync(
      tmpdir.resolve('valid-type-pkg/index.js'),
      'module.exports = 42;'
    );

    const { stderr } = await common.spawnPromisified(process.execPath, [
      tmpdir.resolve('valid-type-pkg/index.js'),
    ]);

    assert.ok(!stderr.includes('MODULE_INVALID_TYPE'));
  });

  it('should not warn when type field is missing', async () => {
    tmpdir.refresh();
    fs.mkdirSync(tmpdir.resolve('no-type-pkg'), { recursive: true });
    fs.writeFileSync(
      tmpdir.resolve('no-type-pkg/package.json'),
      JSON.stringify({ name: 'test' })
    );
    fs.writeFileSync(
      tmpdir.resolve('no-type-pkg/index.js'),
      'module.exports = 42;'
    );

    const { stderr } = await common.spawnPromisified(process.execPath, [
      tmpdir.resolve('no-type-pkg/index.js'),
    ]);

    assert.ok(!stderr.includes('MODULE_INVALID_TYPE'));
  });
});