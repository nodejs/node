'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').execFile;

const mjsFile = require.resolve('../fixtures/es-modules/mjs-file.mjs');
const cjsFile = require.resolve('../fixtures/es-modules/cjs-file.cjs');
const packageWithoutTypeMain =
  require.resolve('../fixtures/es-modules/package-without-type/index.js');
const packageTypeCommonJsMain =
  require.resolve('../fixtures/es-modules/package-type-commonjs/index.js');
const packageTypeModuleMain =
  require.resolve('../fixtures/es-modules/package-type-module/index.js');

// Check that running `node` without options works
expect('', mjsFile, '.mjs file');
expect('', cjsFile, '.cjs file');
expect('', packageTypeModuleMain, 'package-type-module');
expect('', packageTypeCommonJsMain, 'package-type-commonjs');
expect('', packageWithoutTypeMain, 'package-without-type');

// Check that running with --type and no package.json "type" works
expect('--type=commonjs', packageWithoutTypeMain, 'package-without-type');
expect('--type=module', packageWithoutTypeMain, 'package-without-type');
expect('-m', packageWithoutTypeMain, 'package-without-type');

// Check that running with conflicting --type flags throws errors
expect('--type=commonjs', mjsFile, 'ERR_REQUIRE_ESM', true);
expect('--type=module', cjsFile, 'ERR_TYPE_MISMATCH', true);
expect('-m', cjsFile, 'ERR_TYPE_MISMATCH', true);
expect('--type=commonjs', packageTypeModuleMain,
       'SyntaxError', true);
expect('--type=module', packageTypeCommonJsMain,
       'ERR_TYPE_MISMATCH', true);
expect('-m', packageTypeCommonJsMain,
       'ERR_TYPE_MISMATCH', true);

function expect(opt = '', inputFile, want, wantsError = false) {
  // TODO: Remove when --experimental-modules is unflagged
  opt = `--experimental-modules ${opt}`;
  const argv = [inputFile];
  const opts = {
    env: Object.assign({}, process.env, { NODE_OPTIONS: opt }),
    maxBuffer: 1e6,
  };
  exec(process.execPath, argv, opts, common.mustCall((err, stdout, stderr) => {
    if (wantsError) {
      stdout = stderr;
    } else {
      assert.ifError(err);
    }
    if (stdout.includes(want)) return;

    const o = JSON.stringify(opt);
    assert.fail(`For ${o}, failed to find ${want} in: <\n${stdout}\n>`);
  }));
}
