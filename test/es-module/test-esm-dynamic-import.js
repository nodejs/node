// Flags: --experimental-modules
'use strict';
const common = require('../common');
const assert = require('assert');
const { URL } = require('url');

const relativePath = '../fixtures/es-modules/test-esm-ok.mjs';
const absolutePath = require.resolve('../fixtures/es-modules/test-esm-ok.mjs');
const targetURL = new URL('file:///');
targetURL.pathname = absolutePath;

function expectErrorProperty(result, propertyKey, value) {
  Promise.resolve(result)
    .catch(common.mustCall((error) => {
      assert.strictEqual(error[propertyKey], value);
    }));
}

function expectMissingModuleError(result) {
  expectErrorProperty(result, 'code', 'MODULE_NOT_FOUND');
}

function expectOkNamespace(result) {
  Promise.resolve(result)
    .then(common.mustCall((ns) => {
      const expected = { default: true };
      Object.defineProperty(expected, Symbol.toStringTag, {
        value: 'Module'
      });
      Object.setPrototypeOf(expected, Object.getPrototypeOf(ns));
      assert.deepStrictEqual(ns, expected);
    }));
}

function expectFsNamespace(result) {
  Promise.resolve(result)
    .then(common.mustCall((ns) => {
      assert.strictEqual(typeof ns.default.writeFile, 'function');
      assert.strictEqual(typeof ns.writeFile, 'function');
    }));
}

// For direct use of import expressions inside of CJS or ES modules, including
// via eval, all kinds of specifiers should work without issue.
(function testScriptOrModuleImport() {
  // Importing another file, both direct & via eval
  // expectOkNamespace(import(relativePath));
  expectOkNamespace(eval(`import("${relativePath}")`));
  expectOkNamespace(eval(`import("${relativePath}")`));
  expectOkNamespace(eval(`import("${targetURL}")`));

  // Importing a built-in, both direct & via eval
  expectFsNamespace(import('fs'));
  expectFsNamespace(eval('import("fs")'));
  expectFsNamespace(eval('import("fs")'));

  expectMissingModuleError(import('./not-an-existing-module.mjs'));
  // TODO(jkrems): Right now this doesn't hit a protocol error because the
  // module resolution step already rejects it. These arguably should be
  // protocol errors.
  expectMissingModuleError(import('node:fs'));
  expectMissingModuleError(import('http://example.com/foo.js'));
})();
