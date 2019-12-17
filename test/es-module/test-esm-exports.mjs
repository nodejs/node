// Flags: --experimental-modules
import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok, deepStrictEqual, strictEqual } from 'assert';
import { spawn } from 'child_process';

import { requireFixture, importFixture } from '../fixtures/pkgexports.mjs';
import fromInside from '../fixtures/node_modules/pkgexports/lib/hole.js';

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
    // Fallbacks
    ['pkgexports/fallbackdir/asdf.js', { default: 'asdf' }],
    ['pkgexports/fallbackfile', { default: 'asdf' }],
    // Conditional split for require
    ['pkgexports/condition', isRequire ? { default: 'encoded path' } :
      { default: 'asdf' }],
    // String exports sugar
    ['pkgexports-sugar', { default: 'main' }],
    // Conditional object exports sugar
    ['pkgexports-sugar2', isRequire ? { default: 'not-exported' } :
      { default: 'main' }],
    // Resolve self
    ['pkgexports/resolve-self', isRequire ?
      { default: 'self-cjs' } : { default: 'self-mjs' }],
    // Resolve self sugar
    ['pkgexports-sugar', { default: 'main' }]
  ]);

  for (const [validSpecifier, expected] of validSpecifiers) {
    if (validSpecifier === null) continue;

    loadFixture(validSpecifier)
      .then(mustCall((actual) => {
        deepStrictEqual({ ...actual }, expected);
      }));
  }

  const undefinedExports = new Map([
    // There's no such export - so there's nothing to do.
    ['pkgexports/missing', './missing'],
    // The file exists but isn't exported. The exports is a number which counts
    // as a non-null value without any properties, just like `{}`.
    ['pkgexports-number/hidden.js', './hidden.js'],
    // Sugar cases still encapsulate
    ['pkgexports-sugar/not-exported.js', './not-exported.js'],
    ['pkgexports-sugar2/not-exported.js', './not-exported.js'],
  ]);

  const invalidExports = new Map([
    // Even though 'pkgexports/sub/asdf.js' works, alternate "path-like"
    // variants do not to prevent confusion and accidental loopholes.
    ['pkgexports/sub/./../asdf.js', './sub/./../asdf.js'],
    // This path steps back inside the package but goes through an exports
    // target that escapes the package, so we still catch that as invalid
    ['pkgexports/belowdir/pkgexports/asdf.js', './belowdir/pkgexports/asdf.js'],
    // This target file steps below the package
    ['pkgexports/belowfile', './belowfile'],
    // Directory mappings require a trailing / to work
    ['pkgexports/missingtrailer/x', './missingtrailer/x'],
    // Invalid target handling
    ['pkgexports/null', './null'],
    ['pkgexports/invalid1', './invalid1'],
    ['pkgexports/invalid2', './invalid2'],
    ['pkgexports/invalid3', './invalid3'],
    ['pkgexports/invalid4', './invalid4'],
    // Missing / invalid fallbacks
    ['pkgexports/nofallback1', './nofallback1'],
    ['pkgexports/nofallback2', './nofallback2'],
    // Reaching into nested node_modules
    ['pkgexports/nodemodules', './nodemodules'],
  ]);

  for (const [specifier, subpath] of undefinedExports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, (isRequire ? '' : 'ERR_') + 'MODULE_NOT_FOUND');
      assertStartsWith(err.message, 'Package exports');
      assertIncludes(err.message, `do not define a '${subpath}' subpath`);
    }));
  }

  for (const [specifier, subpath] of invalidExports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, (isRequire ? '' : 'ERR_') + 'MODULE_NOT_FOUND');
      assertStartsWith(err.message, (isRequire ? 'Package exports' :
        'Cannot resolve'));
      assertIncludes(err.message, isRequire ?
        `do not define a valid '${subpath}' target` :
        `matched for '${subpath}'`);
    }));
  }

  // Conditional export, even with no match, should still be used instead
  // of falling back to main
  if (isRequire) {
    loadFixture('pkgexports-main').catch(mustCall((err) => {
      strictEqual(err.code, 'MODULE_NOT_FOUND');
      assertStartsWith(err.message, 'No valid export');
    }));
  }

  // Covering out bases - not a file is still not a file after dir mapping.
  loadFixture('pkgexports/sub/not-a-file.js').catch(mustCall((err) => {
    strictEqual(err.code, (isRequire ? '' : 'ERR_') + 'MODULE_NOT_FOUND');
    // ESM returns a full file path
    assertStartsWith(err.message, isRequire ?
      'Cannot find module \'pkgexports/sub/not-a-file.js\'' :
      'Cannot find module');
  }));

  // The use of %2F escapes in paths fails loading
  loadFixture('pkgexports/sub/..%2F..%2Fbar.js').catch(mustCall((err) => {
    strictEqual(err.code, isRequire ? 'ERR_INVALID_FILE_URL_PATH' :
      'ERR_MODULE_NOT_FOUND');
  }));

  // Sugar conditional exports main mixed failure case
  loadFixture('pkgexports-sugar-fail').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_INVALID_PACKAGE_CONFIG');
    assertStartsWith(err.message, (isRequire ? 'Invalid package' :
      'Cannot resolve'));
    assertIncludes(err.message, '"exports" cannot contain some keys starting ' +
    'with \'.\' and some not. The exports object must either be an object of ' +
    'package subpath keys or an object of main entry condition name keys ' +
    'only.');
  }));
});

const { requireFromInside, importFromInside } = fromInside;
[importFromInside, requireFromInside].forEach((loadFromInside) => {
  const validSpecifiers = new Map([
    // A file not visible from outside of the package
    ['../not-exported.js', { default: 'not-exported' }],
    // Part of the public interface
    ['pkgexports/valid-cjs', { default: 'asdf' }],
  ]);
  for (const [validSpecifier, expected] of validSpecifiers) {
    if (validSpecifier === null) continue;

    loadFromInside(validSpecifier)
      .then(mustCall((actual) => {
        deepStrictEqual({ ...actual }, expected);
      }));
  }
});

function assertStartsWith(actual, expected) {
  const start = actual.toString().substr(0, expected.length);
  strictEqual(start, expected);
}

function assertIncludes(actual, expected) {
  ok(actual.toString().indexOf(expected) !== -1,
     `${JSON.stringify(actual)} includes ${JSON.stringify(expected)}`);
}

// Test warning message
[
  [
    '--experimental-conditional-exports',
    '/es-modules/conditional-exports.js',
    'Conditional exports',
  ],
  [
    '--experimental-resolve-self',
    '/node_modules/pkgexports/resolve-self.js',
    'Package name self resolution',
  ],
].forEach(([flag, file, message]) => {
  const child = spawn(process.execPath, [flag, path(file)]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', (code, signal) => {
    strictEqual(code, 0);
    strictEqual(signal, null);
    ok(stderr.toString().includes(
      `ExperimentalWarning: ${message} is an experimental feature. ` +
      'This feature could change at any time'
    ));
  });
});
