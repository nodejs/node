'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const path = require('path');
const assert = require('assert');
const exec = require('child_process').execFile;
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const tmpDir = tmpdir.path;

// Check that running the symlink executes the target as the correct type
const symlinks = [
  {
    source: 'extensionless-symlink-to-mjs-file',
    target: fixtures.path('es-modules/mjs-file.mjs'),
    prints: '.mjs file',
    errorsWithPreserveSymlinksMain: false
  }, {
    source: 'extensionless-symlink-to-cjs-file',
    target: fixtures.path('es-modules/cjs-file.cjs'),
    prints: '.cjs file',
    errorsWithPreserveSymlinksMain: false
  }, {
    source: 'extensionless-symlink-to-file-in-module-scope',
    target: fixtures.path('es-modules/package-type-module/index.js'),
    prints: 'package-type-module',
    // The package scope of the symlinks' sources is commonjs, and this
    // symlink's target is a .js file in a module scope, so when the scope
    // is evaluated based on the source (commonjs) this esm file should error
    errorsWithPreserveSymlinksMain: true
  }, {
    source: 'extensionless-symlink-to-file-in-explicit-commonjs-scope',
    target: fixtures.path('es-modules/package-type-commonjs/index.js'),
    prints: 'package-type-commonjs',
    errorsWithPreserveSymlinksMain: false
  }, {
    source: 'extensionless-symlink-to-file-in-implicit-commonjs-scope',
    target: fixtures.path('es-modules/package-without-type/index.js'),
    prints: 'package-without-type',
    errorsWithPreserveSymlinksMain: false
  },
];

symlinks.forEach((symlink) => {
  const mainPath = path.join(tmpDir, symlink.source);
  fs.symlinkSync(symlink.target, mainPath);

  const flags = [
    '',
    '--preserve-symlinks-main',
  ];
  flags.forEach((nodeOptions) => {
    const opts = {
      env: Object.assign({}, process.env, { NODE_OPTIONS: nodeOptions })
    };
    exec(process.execPath, [mainPath], opts, common.mustCall(
      (err, stdout) => {
        if (nodeOptions.includes('--preserve-symlinks-main')) {
          if (symlink.errorsWithPreserveSymlinksMain &&
              err.toString().includes('Error')) return;
          else if (!symlink.errorsWithPreserveSymlinksMain &&
                    stdout.includes(symlink.prints)) return;
          assert.fail(`For ${JSON.stringify(symlink)}, ${
            (symlink.errorsWithPreserveSymlinksMain) ?
              'failed to error' : 'errored unexpectedly'
          } with --preserve-symlinks-main`);
        } else {
          if (stdout.includes(symlink.prints)) return;
          assert.fail(`For ${JSON.stringify(symlink)}, failed to find ` +
            `${symlink.prints} in: <\n${stdout}\n>`);
        }
      }
    ));
  });
});
