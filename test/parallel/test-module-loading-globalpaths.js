'use strict';
const common = require('../common');
const pkgName = 'foo';
const { addLibraryPath } = require('../common/shared-lib-util');

common.crashOnUnhandledRejection();

addLibraryPath(process.env);
(async () => {
  if (process.argv[2] === 'child') {
    console.log(require(pkgName).string);
  } else {
    const fixtures = require('../common/fixtures');
    const assert = require('assert');
    const path = require('path');
    const fs = require('fs');
    const { chmod, copyFile, mkdir, stat, writeFile } = fs.promises;
    const { COPYFILE_FICLONE } = fs.constants;
    const child_process = require('child_process');
    const promisify = require('util').promisify;
    const execFile = promisify(child_process.execFile);

    const tmpdir = require('../common/tmpdir');
    tmpdir.refresh();

    // test directories
    const noPkgHomeDir = path.join(tmpdir.path, 'home-no-pkg');
    const prefixPath = path.join(tmpdir.path, 'install');
    const prefixLibPath = path.join(prefixPath, 'lib');

    // Copy node binary into a test $PREFIX directory.
    await mkdir(prefixPath);
    let testExecPath;
    if (common.isWindows) {
      testExecPath = path.join(prefixPath, path.basename(process.execPath));
    } else {
      const prefixBinPath = path.join(prefixPath, 'bin');
      await mkdir(prefixBinPath);
      testExecPath = path.join(prefixBinPath, path.basename(process.execPath));
    }

    // prepare test directories
    const [{ mode }] = await Promise.all([
      stat(process.execPath),
      copyFile(process.execPath, testExecPath, COPYFILE_FICLONE),
      mkdir(noPkgHomeDir),
      mkdir(prefixLibPath)
    ]);
    const prefixLibNodePath = path.join(prefixLibPath, 'node');
    await Promise.all([
      mkdir(prefixLibNodePath),
      chmod(testExecPath, mode)
    ]);

    const runTest = (expectedString, env) => {
      child_process.execFile(
        testExecPath,
        [ __filename, 'child' ],
        { encoding: 'utf8', env: env },
        (err, stdout, stderr) => {
          assert.strictEqual(stdout.trim(), expectedString);
        }
      );
    };
    const testFixturesDir = fixtures.path(path.basename(__filename, '.js'));

    const env = Object.assign({}, process.env);
    // Unset NODE_PATH.
    delete env.NODE_PATH;

    // Test empty global path.
    env.HOME = env.USERPROFILE = noPkgHomeDir;
    await assert.rejects(
      async () => {
        await execFile(
          testExecPath,
          [ __filename, 'child' ],
          { encoding: 'utf8', env: env });
      },
      new RegExp(`Cannot find module '${pkgName}'`));

    // Test module in $HOME/.node_modules.
    const modHomeDir = path.join(testFixturesDir, 'home-pkg-in-node_modules');
    env.HOME = env.USERPROFILE = modHomeDir;
    runTest('$HOME/.node_modules', env);

    // Test module in $HOME/.node_libraries.
    const libHomeDir = path.join(testFixturesDir, 'home-pkg-in-node_libraries');
    env.HOME = env.USERPROFILE = libHomeDir;
    runTest('$HOME/.node_libraries', env);

    // Test module both $HOME/.node_modules and $HOME/.node_libraries.
    const bothHomeDir = path.join(testFixturesDir, 'home-pkg-in-both');
    env.HOME = env.USERPROFILE = bothHomeDir;
    runTest('$HOME/.node_modules', env);

    // Test module in $PREFIX/lib/node.
    // Write module into $PREFIX/lib/node.
    const expectedString = '$PREFIX/lib/node';
    const pkgPath = path.join(prefixLibNodePath, `${pkgName}.js`);
    await writeFile(pkgPath, `exports.string = '${expectedString}';`);

    env.HOME = env.USERPROFILE = noPkgHomeDir;
    runTest(expectedString, env);

    // Test module in all global folders.
    env.HOME = env.USERPROFILE = bothHomeDir;
    runTest('$HOME/.node_modules', env);

    // Test module in NODE_PATH is loaded ahead of global folders.
    env.HOME = env.USERPROFILE = bothHomeDir;
    env.NODE_PATH = path.join(testFixturesDir, 'node_path');
    runTest('$NODE_PATH', env);

    // Test module in local folder is loaded ahead of global folders.
    const localDir = path.join(testFixturesDir, 'local-pkg');
    env.HOME = env.USERPROFILE = bothHomeDir;
    env.NODE_PATH = path.join(testFixturesDir, 'node_path');
    const { stdout } = await execFile(
      testExecPath,
      [ path.join(localDir, 'test.js') ],
      { encoding: 'utf8', env: env }
    );
    assert.strictEqual(stdout.trim(), 'local');
  }
})();
