'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');

const { types } = require('util');
const { SourceTextModule } = require('vm');

async function getNamespace() {
  const m = new SourceTextModule('');
  await m.link(() => 0);
  await m.evaluate();
  return m.namespace;
}

(async () => {
  const namespace = await getNamespace();
  const m = new SourceTextModule('export const A = "A"; import("");', {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      return namespace;
    })
  });
  await m.link(() => 0);
  const { result } = await m.evaluate();
  const ns = await result;
  assert.ok(types.isModuleNamespaceObject(ns));
})().then(common.mustCall());
