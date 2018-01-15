'use strict';

// Flags: --vm-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');

const { Module } = require('vm');

(async function main() {
  const foo = new Module('export default 5;');
  await foo.link(() => {});

  const bar = new Module('import five from "foo"; five');

  assert.deepStrictEqual(bar.dependencySpecifiers, ['foo']);

  await bar.link((url) => {
    assert.strictEqual(url, 'foo');
    return foo;
  });

  bar.instantiate();

  assert.strictEqual((await bar.evaluate()).result, 5);
}());
