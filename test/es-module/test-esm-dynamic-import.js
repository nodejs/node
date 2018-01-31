// Flags: --experimental-modules --harmony-dynamic-import
'use strict';
const common = require('../common');
const assert = require('assert');
const { URL } = require('url');
const vm = require('vm');

common.crashOnUnhandledRejection();

const relativePath = './test-esm-ok.mjs';
const absolutePath = require.resolve('./test-esm-ok.mjs');
const targetURL = new URL('file:///');
targetURL.pathname = absolutePath;

function expectErrorProperty(result, propertyKey, value) {
  Promise.resolve(result)
    .catch(common.mustCall(error => {
      assert.strictEqual(error[propertyKey], value);
    }));
}

function expectMissingModuleError(result) {
  expectErrorProperty(result, 'code', 'MODULE_NOT_FOUND');
}

function expectInvalidUrlError(result) {
  expectErrorProperty(result, 'code', 'ERR_INVALID_URL');
}

function expectInvalidReferrerError(result) {
  expectErrorProperty(result, 'code', 'ERR_INVALID_URL');
}

function expectInvalidProtocolError(result) {
  expectErrorProperty(result, 'code', 'ERR_INVALID_PROTOCOL');
}

function expectInvalidContextError(result) {
  expectErrorProperty(result,
    'message', 'import() called outside of main context');
}

function expectOkNamespace(result) {
  Promise.resolve(result)
    .then(common.mustCall(ns => {
      // Can't deepStrictEqual because ns isn't a normal object
      assert.deepEqual(ns, { default: true });
    }));
}

function expectFsNamespace(result) {
  Promise.resolve(result)
    .then(common.mustCall(ns => {
      assert.strictEqual(typeof ns.default.writeFile, 'function');
    }));
}

// For direct use of import expressions inside of CJS or ES modules, including
// via eval, all kinds of specifiers should work without issue.
(function testScriptOrModuleImport() {
  // Importing another file, both direct & via eval
  // expectOkNamespace(import(relativePath));
  expectOkNamespace(eval.call(null, `import("${relativePath}")`));
  expectOkNamespace(eval(`import("${relativePath}")`));
  expectOkNamespace(eval.call(null, `import("${targetURL}")`));

  // Importing a built-in, both direct & via eval
  expectFsNamespace(import("fs"));
  expectFsNamespace(eval('import("fs")'));
  expectFsNamespace(eval.call(null, 'import("fs")'));

  expectMissingModuleError(import("./not-an-existing-module.mjs"));
  // TODO(jkrems): Right now this doesn't hit a protocol error because the
  // module resolution step already rejects it. These arguably should be
  // protocol errors.
  expectMissingModuleError(import("node:fs"));
  expectMissingModuleError(import('http://example.com/foo.js'));
})();

// vm.runInThisContext:
// * Supports built-ins, always
// * Supports imports if the script has a known defined origin
(function testRunInThisContext() {
  // Succeeds because it's got an valid base url
  expectFsNamespace(vm.runInThisContext(`import("fs")`, {
    filename: __filename,
  }));
  expectOkNamespace(vm.runInThisContext(`import("${relativePath}")`, {
    filename: __filename,
  }));
  // Rejects because it's got an invalid referrer URL.
  // TODO(jkrems): Arguably the first two (built-in + absolute URL) could work
  // with some additional effort.
  expectInvalidReferrerError(vm.runInThisContext('import("fs")'));
  expectInvalidReferrerError(vm.runInThisContext(`import("${targetURL}")`));
  expectInvalidReferrerError(vm.runInThisContext(`import("${relativePath}")`));
})();

// vm.runInNewContext is currently completely unsupported, pending well-defined
// semantics for per-context/realm module maps in node.
(function testRunInNewContext() {
  // Rejects because it's running in the wrong context
  expectInvalidContextError(
    vm.runInNewContext(`import("${targetURL}")`, undefined, {
      filename: __filename,
    })
  );

  // Rejects because it's running in the wrong context
  expectInvalidContextError(vm.runInNewContext(`import("fs")`, undefined, {
    filename: __filename,
  }));
})();
