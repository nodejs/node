'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const child_process = require('child_process');
const pkgName = 'foo';

if (process.argv[2] === 'child') {
  console.log(require(pkgName).string);
} else {
  common.refreshTmpDir();

  // Copy node binary into a test $PREFIX directory.
  const prefixPath = path.join(common.tmpDir, 'install');
  fs.mkdirSync(prefixPath);
  let testExecPath;
  if (common.isWindows) {
    testExecPath = path.join(prefixPath, path.basename(process.execPath));
  } else {
    const prefixBinPath = path.join(prefixPath, 'bin');
    fs.mkdirSync(prefixBinPath);
    testExecPath = path.join(prefixBinPath, path.basename(process.execPath));
  }
  const mode = fs.statSync(process.execPath).mode;
  fs.writeFileSync(testExecPath, fs.readFileSync(process.execPath));
  fs.chmodSync(testExecPath, mode);

  const runTest = (expectedString, env) => {
    const child = child_process.execFileSync(testExecPath,
                                             [ __filename, 'child' ],
                                             { encoding: 'utf8', env: env });
    assert.strictEqual(child.trim(), expectedString);
  };

  const testFixturesDir = path.join(common.fixturesDir,
                                    path.basename(__filename, '.js'));

  const env = Object.assign({}, process.env);
  // Turn on module debug to aid diagnosing failures.
  env['NODE_DEBUG'] = 'module';
  // Unset NODE_PATH.
  delete env['NODE_PATH'];

  // Test empty global path.
  const noPkgHomeDir = path.join(common.tmpDir, 'home-no-pkg');
  fs.mkdirSync(noPkgHomeDir);
  env['HOME'] = env['USERPROFILE'] = noPkgHomeDir;
  assert.throws(
      () => {
        child_process.execFileSync(testExecPath, [ __filename, 'child' ],
                                   { encoding: 'utf8', env: env });
      },
      new RegExp('Cannot find module \'' + pkgName + '\''));

  // Test module in $HOME/.node_modules.
  const modHomeDir = path.join(testFixturesDir, 'home-pkg-in-node_modules');
  env['HOME'] = env['USERPROFILE'] = modHomeDir;
  runTest('$HOME/.node_modules', env);

  // Test module in $HOME/.node_libraries.
  const libHomeDir = path.join(testFixturesDir, 'home-pkg-in-node_libraries');
  env['HOME'] = env['USERPROFILE'] = libHomeDir;
  runTest('$HOME/.node_libraries', env);

  // Test module both $HOME/.node_modules and $HOME/.node_libraries.
  const bothHomeDir = path.join(testFixturesDir, 'home-pkg-in-both');
  env['HOME'] = env['USERPROFILE'] = bothHomeDir;
  runTest('$HOME/.node_modules', env);

  // Test module in $PREFIX/lib/node.
  // Write module into $PREFIX/lib/node.
  const expectedString = '$PREFIX/lib/node';
  const prefixLibPath = path.join(prefixPath, 'lib');
  fs.mkdirSync(prefixLibPath);
  const prefixLibNodePath = path.join(prefixLibPath, 'node');
  fs.mkdirSync(prefixLibNodePath);
  const pkgPath = path.join(prefixLibNodePath, pkgName + '.js');
  fs.writeFileSync(pkgPath, 'exports.string = \'' + expectedString + '\';');

  env['HOME'] = env['USERPROFILE'] = noPkgHomeDir;
  runTest(expectedString, env);

  // Test module in all global folders.
  env['HOME'] = env['USERPROFILE'] = bothHomeDir;
  runTest('$HOME/.node_modules', env);

  // Test module in NODE_PATH is loaded ahead of global folders.
  env['HOME'] = env['USERPROFILE'] = bothHomeDir;
  env['NODE_PATH'] = path.join(testFixturesDir, 'node_path');
  runTest('$NODE_PATH', env);

  // Test module in local folder is loaded ahead of global folders.
  const localDir = path.join(testFixturesDir, 'local-pkg');
  env['HOME'] = env['USERPROFILE'] = bothHomeDir;
  env['NODE_PATH'] = path.join(testFixturesDir, 'node_path');
  const child = child_process.execFileSync(testExecPath,
                                           [ path.join(localDir, 'test.js') ],
                                           { encoding: 'utf8', env: env });
  assert.strictEqual(child.trim(), 'local');
}
