'use strict';

const common = require('../common');
const { registerHooks } = require('module');

const schemelessBlockList = new Set([
  'sea',
  'sqlite',
  'test',
  'test/reporters',
]);

const testModules = [];
for (const mod of schemelessBlockList) {
  testModules.push(`node:${mod}`);
}

const hook = registerHooks({
  resolve: common.mustCall((specifier, context, nextResolve) => nextResolve(specifier, context), testModules.length),
  load: common.mustCall((url, context, nextLoad) => nextLoad(url, context), testModules.length),
});

for (const mod of testModules) {
  require(mod);
}

hook.deregister();
