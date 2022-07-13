'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');

const { types } = require('util');
const { SourceTextModule } = require('vm');

(async () => {
  const m = new SourceTextModule('globalThis.importResult = import("");', {
    importModuleDynamically: common.mustCall(async (specifier, wrap) => {
      const m = new SourceTextModule('');
      await m.link(() => 0);
      await m.evaluate();
      return m.namespace;
    }),
  });
  await m.link(() => 0);
  await m.evaluate();
  const ns = await globalThis.importResult;
  delete globalThis.importResult;
  assert.ok(types.isModuleNamespaceObject(ns));
})().then(common.mustCall());
