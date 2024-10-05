'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');
const { getPackageJSON } = require('module');
const { pathToFileURL } = require('url');


{ // Throws when no arguments are provided
  assert.throws(
    () => getPackageJSON(),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}

{ // Throw when `everything` is not a boolean
  for (const invalid of ['', {}, [], Symbol(), 1, 0]) assert.throws(
    () => getPackageJSON('', invalid),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}

const pkgName = 'package-with-unrecognised-fields';
const pkgType = 'module'; // a non-default value

{ // Accepts a file URL (string), like from `import.meta.resolve()`
  const pathToDir = fixtures.path('packages/root-types-field/');
  const pkg = getPackageJSON(`${pathToFileURL(pathToDir)}`);

  assert.deepStrictEqual(pkg, {
    path: path.join(pathToDir, 'package.json'),
    data: {
      __proto__: null,
      name: pkgName,
      type: pkgType,
    }
  });
}

{ // Exclude unrecognised fields when `everything` is not `true`
  const pathToDir = fixtures.path('packages/root-types-field/');
  const pkg = getPackageJSON(pathToDir);

  assert.deepStrictEqual(pkg, {
    path: path.join(pathToDir, 'package.json'),
    data: {
      __proto__: null,
      name: pkgName,
      type: pkgType,
    }
  });
}

{ // Include unrecognised fields when `everything` is `true`
  const pathToDir = fixtures.path('packages/root-types-field/');
  const pkg = getPackageJSON(pathToDir, true);

  assert.deepStrictEqual(pkg, {
    path: path.join(pathToDir, 'package.json'),
    data: {
      __proto__: null,
      name: pkgName,
      type: pkgType,
      types: './index.d.ts',
    }
  });
}

{ // Exclude unrecognised fields when `everything` is not `true`
  const pathToDir = fixtures.path('packages/nested-types-field/');
  const pkg = getPackageJSON(pathToDir);

  assert.deepStrictEqual(pkg, {
    path: path.join(pathToDir, 'package.json'),
    data: {
      __proto__: null,
      name: pkgName,
      type: pkgType,
      exports: {
        default: './index.js',
        types: './index.d.ts', // I think this is unexpected?
      },
    },
  });
}

{ // Include unrecognised fields when `everything` is not `true`
  const pathToDir = fixtures.path('packages/nested-types-field/');
  const pkg = getPackageJSON(pathToDir, true);

  assert.deepStrictEqual(pkg, {
    path: path.join(pathToDir, 'package.json'),
    data: {
      __proto__: null,
      name: pkgName,
      type: pkgType,
      exports: {
        default: './index.js',
        types: './index.d.ts', // I think this is unexpected?
      },
      unrecognised: true,
    },
  });
}
