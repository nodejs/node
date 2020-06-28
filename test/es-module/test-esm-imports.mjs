import { mustCall } from '../common/index.mjs';
import { ok, deepStrictEqual, strictEqual } from 'assert';

import importer from '../fixtures/es-modules/pkgimports/importer.js';
import { requireFixture } from '../fixtures/pkgexports.mjs';

const { requireImport, importImport } = importer;

[requireImport, importImport].forEach((loadFixture) => {
  const isRequire = loadFixture === requireImport;

  const internalImports = new Map([
    // Base case
    ['#test', { default: 'test' }],
    // import / require conditions
    ['#branch', { default: isRequire ? 'requirebranch' : 'importbranch' }],
    // Subpath imports
    ['#subpath/x.js', { default: 'xsubpath' }],
    // External imports
    ['#external', { default: 'asdf' }],
    // External subpath imports
    ['#external/subpath/asdf.js', { default: 'asdf' }],
  ]);

  for (const [validSpecifier, expected] of internalImports) {
    if (validSpecifier === null) continue;

    loadFixture(validSpecifier)
      .then(mustCall((actual) => {
        deepStrictEqual({ ...actual }, expected);
      }));
  }

  const invalidImportTargets = new Set([
    // External subpath import without trailing slash
    ['#external/invalidsubpath/x', '#external/invalidsubpath/'],
    // Target steps below the package base
    ['#belowbase', '#belowbase'],
    // Target is a URL
    ['#url', '#url'],
  ]);

  for (const [specifier, subpath] of invalidImportTargets) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_INVALID_PACKAGE_TARGET');
      assertStartsWith(err.message, 'Invalid "imports"');
      assertIncludes(err.message, subpath);
      assertNotIncludes(err.message, 'targets must start with');
    }));
  }

  const invalidImportSpecifiers = new Map([
    // Backtracking below the package base
    ['#subpath/sub/../../../belowbase', 'request is not a valid subpath'],
    // Percent-encoded slash errors
    ['#external/subpath/x%2Fy', 'must not include encoded "/"'],
    // Target must have a name
    ['#', '#'],
    // Initial slash target must have a leading name
    ['#/initialslash', '#/initialslash'],
    // Percent-encoded target paths
    ['#percent', 'must not include encoded "/"'],
  ]);

  for (const [specifier, expected] of invalidImportSpecifiers) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
      assertStartsWith(err.message, 'Invalid module');
      assertIncludes(err.message, expected);
    }));
  }

  const undefinedImports = new Set([
    // Missing import
    '#missing',
    // Explicit null import
    '#null',
    // No condition match import
    '#nullcondition',
    // Null subpath shadowing
    '#subpath/nullshadow/x',
  ]);

  for (const specifier of undefinedImports) {
    loadFixture(specifier).catch(mustCall((err) => {
      strictEqual(err.code, 'ERR_PACKAGE_IMPORT_NOT_DEFINED');
      assertStartsWith(err.message, 'Package import ');
      assertIncludes(err.message, specifier);
    }));
  }

  // Handle not found for the defined imports target not existing
  loadFixture('#notfound').catch(mustCall((err) => {
    strictEqual(err.code,
                isRequire ? 'MODULE_NOT_FOUND' : 'ERR_MODULE_NOT_FOUND');
  }));
});

// CJS resolver must still support #package packages in node_modules
requireFixture('#cjs').then(mustCall((actual) => {
  strictEqual(actual.default, 'cjs backcompat');
}));

function assertStartsWith(actual, expected) {
  const start = actual.toString().substr(0, expected.length);
  strictEqual(start, expected);
}

function assertIncludes(actual, expected) {
  ok(actual.toString().indexOf(expected) !== -1,
     `${JSON.stringify(actual)} includes ${JSON.stringify(expected)}`);
}

function assertNotIncludes(actual, expected) {
  ok(actual.toString().indexOf(expected) === -1,
     `${JSON.stringify(actual)} doesn't include ${JSON.stringify(expected)}`);
}
