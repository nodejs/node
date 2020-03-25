// Flags: --experimental-modules
import { mustCall } from '../common/index.mjs';
import { ok, deepStrictEqual, strictEqual } from 'assert';

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
    ['pkgexports-sugar', { default: 'main' }],
  ]);

  if (isRequire) {
    validSpecifiers.set('pkgexports/subpath/file', { default: 'file' });
    validSpecifiers.set('pkgexports/subpath/dir1', { default: 'main' });
    validSpecifiers.set('pkgexports/subpath/dir1/', { default: 'main' });
    validSpecifiers.set('pkgexports/subpath/dir2', { default: 'index' });
    validSpecifiers.set('pkgexports/subpath/dir2/', { default: 'index' });
  }

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
    // Conditional exports with no match are "not exported" errors
    ['pkgexports/invalid1', './invalid1'],
    ['pkgexports/invalid4', './invalid4'],
  ]);

  const invalidExports = new Map([
    // Directory mappings require a trailing / to work
    ['pkgexports/missingtrailer/x', './missingtrailer/'],
    // This path steps back inside the package but goes through an exports
    // target that escapes the package, so we still catch that as invalid
    ['pkgexports/belowdir/pkgexports/asdf.js', './belowdir/'],
    // This target file steps below the package
    ['pkgexports/belowfile', './belowfile'],
    // Invalid target handling
    ['pkgexports/null', './null'],
    ['pkgexports/invalid2', './invalid2'],
    ['pkgexports/invalid3', './invalid3'],
    // Missing / invalid fallbacks
    ['pkgexports/nofallback1', './nofallback1'],
    ['pkgexports/nofallback2', './nofallback2'],
    // Reaching into nested node_modules
    ['pkgexports/nodemodules', './nodemodules'],
    // Self resolve invalid
    ['pkgexports/resolve-self-invalid', './invalid2'],
  ]);

  const invalidSpecifiers = new Map([
    // Even though 'pkgexports/sub/asdf.js' works, alternate "path-like"
    // variants do not to prevent confusion and accidental loopholes.
    ['pkgexports/sub/./../asdf.js', './sub/./../asdf.js'],
  ]);

  for (const [specifier, subpath] of undefinedExports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_PACKAGE_PATH_NOT_EXPORTED');
      assertStartsWith(err.message, 'Package subpath ');
      assertIncludes(err.message, subpath);
    }));
  }

  for (const [specifier, subpath] of invalidExports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_INVALID_PACKAGE_TARGET');
      assertStartsWith(err.message, 'Invalid "exports"');
      assertIncludes(err.message, subpath);
    }));
  }

  for (const [specifier, subpath] of invalidSpecifiers) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
      assertStartsWith(err.message, 'Package subpath ');
      assertIncludes(err.message, subpath);
    }));
  }

  // Conditional export, even with no match, should still be used instead
  // of falling back to main
  if (isRequire) {
    loadFixture('pkgexports-main').catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_PACKAGE_PATH_NOT_EXPORTED');
      assertStartsWith(err.message, 'No "exports" main ');
    }));
  }

  const notFoundExports = new Map([
    // Non-existing file
    ['pkgexports/sub/not-a-file.js', 'pkgexports/sub/not-a-file.js'],
    // No extension lookups
    ['pkgexports/no-ext', 'pkgexports/no-ext'],
  ]);

  if (!isRequire) {
    notFoundExports.set('pkgexports/subpath/file', 'pkgexports/subpath/file');
    notFoundExports.set('pkgexports/subpath/dir1', 'pkgexports/subpath/dir1');
    notFoundExports.set('pkgexports/subpath/dir2', 'pkgexports/subpath/dir2');
  }

  for (const [specifier, request] of notFoundExports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, (isRequire ? '' : 'ERR_') + 'MODULE_NOT_FOUND');
      // ESM returns a full file path
      assertStartsWith(err.message, isRequire ?
        `Cannot find module '${request}'` :
        'Cannot find module');
    }));
  }

  // The use of %2F escapes in paths fails loading
  loadFixture('pkgexports/sub/..%2F..%2Fbar.js').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_INVALID_FILE_URL_PATH');
  }));

  // Package export with numeric index properties must throw a validation error
  loadFixture('pkgexports-numeric').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_INVALID_PACKAGE_CONFIG');
  }));

  // Sugar conditional exports main mixed failure case
  loadFixture('pkgexports-sugar-fail').catch(mustCall((err) => {
    strictEqual(err.code, 'ERR_INVALID_PACKAGE_CONFIG');
    assertStartsWith(err.message, 'Invalid package');
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
