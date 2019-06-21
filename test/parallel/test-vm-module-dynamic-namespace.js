'use strict';

// Flags: --experimental-vm-modules --expose-internals
//
const common = require('../common');

const assert = require('assert');

const { types } = require('util');
const { SourceTextModule, wrapMap } = require('internal/vm/source_text_module');

const { importModuleDynamicallyCallback } =
  require('internal/process/esm_loader');

async function getNamespace() {
  const m = new SourceTextModule('');
  await m.link(() => 0);
  m.instantiate();
  await m.evaluate();
  return m.namespace;
}

(async () => {
  const namespace = await getNamespace();
  const m = new SourceTextModule('export const A = "A";', {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      return namespace;
    })
  });
  await m.link(() => 0);
  m.instantiate();
  await m.evaluate();
  const ns = await importModuleDynamicallyCallback(wrapMap.get(m));
  assert.ok(types.isModuleNamespaceObject(ns));
})().then(common.mustCall());
