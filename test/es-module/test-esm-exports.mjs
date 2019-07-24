// Flags: --experimental-modules

import { mustCall } from '../common/index.mjs';
import { ok, deepStrictEqual, strictEqual } from 'assert';

import { requireFixture, importFixture } from '../fixtures/pkgexports.mjs';

[requireFixture, importFixture].forEach((loadFixture) => {
  const isRequire = loadFixture === requireFixture;

  const validSpecifiers = new Map([
    // A simple mapping of a path.
    ['pkgexports/valid-cjs', { default: 'asdf' }],
    // A directory mapping, pointing to the package root.
    ['pkgexports/sub/asdf.js', { default: 'asdf' }],
    // A mapping pointing to a file that needs special encoding (%20) in URLs.
    ['pkgexports/space', { default: 'encoded path' }],
    // Verifying that normal packages still work with exports turned on.
    isRequire ? ['baz/index', { default: 'eye catcher' }] : [null],
  ]);
  for (const [validSpecifier, expected] of validSpecifiers) {
    if (validSpecifier === null) continue;

    loadFixture(validSpecifier)
      .then(mustCall((actual) => {
        deepStrictEqual({ ...actual }, expected);
      }));
  }

  // There's no such export - so there's nothing to do.
  loadFixture('pkgexports/missing').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_PATH_NOT_EXPORTED');
    assertStartsWith(err.message, 'Package exports');
    assertIncludes(err.message, 'do not define a \'./missing\' subpath');
  }));

  // The file exists but isn't exported. The exports is a number which counts
  // as a non-null value without any properties, just like `{}`.
  loadFixture('pkgexports-number/hidden.js').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_PATH_NOT_EXPORTED');
    assertStartsWith(err.message, 'Package exports');
    assertIncludes(err.message, 'do not define a \'./hidden.js\' subpath');
  }));

  // There's no main field so we won't find anything when importing the name.
  // The fact that "." is mapped is ignored, it's not a valid main config.
  loadFixture('pkgexports').catch(mustCall((err) => {
    if (isRequire) {
      strictEqual(err.code, 'MODULE_NOT_FOUND');
      assertStartsWith(err.message, 'Cannot find module \'pkgexports\'');
    } else {
      strictEqual(err.code, 'ERR_MODULE_NOT_FOUND');
      assertStartsWith(err.message, 'Cannot find main entry point');
    }
  }));

  // Even though 'pkgexports/sub/asdf.js' works, alternate "path-like" variants
  // do not to prevent confusion and accidental loopholes.
  loadFixture('pkgexports/sub/./../asdf.js').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_PATH_NOT_EXPORTED');
    assertStartsWith(err.message, 'Package exports');
    assertIncludes(err.message,
                   'do not define a \'./sub/./../asdf.js\' subpath');
  }));

  // Covering out bases - not a file is still not a file after dir mapping.
  loadFixture('pkgexports/sub/not-a-file.js').catch(mustCall((err) => {
    if (isRequire) {
      strictEqual(err.code, 'MODULE_NOT_FOUND');
      assertStartsWith(err.message,
                       'Cannot find module \'pkgexports/sub/not-a-file.js\'');
    } else {
      strictEqual(err.code, 'ERR_MODULE_NOT_FOUND');
      // ESM currently returns a full file path
      assertStartsWith(err.message, 'Cannot find module');
    }
  }));
});

function assertStartsWith(actual, expected) {
  const start = actual.toString().substr(0, expected.length);
  strictEqual(start, expected);
}

function assertIncludes(actual, expected) {
  ok(actual.toString().indexOf(expected),
     `${JSON.stringify(actual)} includes ${JSON.stringify(expected)}`);
}
