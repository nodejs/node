#!/usr/bin/env node
/**
 * Usage:
 *   test-npm-package.js [--install] [--rebuild] <source> <test-arg>+
 *
 * Everything after the <source> directory gets passed to `npm run` to build
 * the test command.
 *
 * If `--install` is passed, we'll run a full `npm install` before running the
 * test suite. Same for `--rebuild` and `npm rebuild`.
 *
 * We always use the node used to spawn this script and the `npm` version
 * bundled in `deps/npm`.
 *
 * If an additional `--logfile=<filename>` option is passed before `<source>`,
 * the stdout output of the test script will be written to that file.
 */
'use strict';
const { spawn, spawnSync } = require('child_process');
const { createHash } = require('crypto');
const { createWriteStream, mkdirSync, rmdirSync } = require('fs');
const path = require('path');

const common = require('../test/common');

const projectDir = path.resolve(__dirname, '..');
const npmBin = path.join(projectDir, 'deps', 'npm', 'bin', 'npm-cli.js');
const nodePath = path.dirname(process.execPath);

function spawnCopyDeepSync(source, destination) {
  if (common.isWindows) {
    mkdirSync(destination); // prevent interactive prompt
    return spawnSync('xcopy.exe', ['/E', source, destination]);
  } else {
    return spawnSync('cp', ['-r', `${source}/`, destination]);
  }
}

function runNPMPackageTests({ srcDir, install, rebuild, testArgs, logfile }) {
  // Make sure we don't conflict with concurrent test runs
  const srcHash = createHash('md5').update(srcDir).digest('hex');
  common.tmpDir = common.tmpDir + '.npm.' + srcHash;
  common.refreshTmpDir();

  const tmpDir = common.tmpDir;
  const npmCache = path.join(tmpDir, 'npm-cache');
  const npmPrefix = path.join(tmpDir, 'npm-prefix');
  const npmTmp = path.join(tmpDir, 'npm-tmp');
  const npmUserconfig = path.join(tmpDir, 'npm-userconfig');
  const pkgDir = path.join(tmpDir, 'pkg');

  spawnCopyDeepSync(srcDir, pkgDir);

  const npmOptions = {
    cwd: pkgDir,
    env: Object.assign({}, process.env, {
      'npm_config_cache': npmCache,
      'npm_config_prefix': npmPrefix,
      'npm_config_tmp': npmTmp,
      'npm_config_userconfig': npmUserconfig,
    }),
    stdio: 'inherit',
  };

  if (common.isWindows) {
    npmOptions.env.home = tmpDir;
    npmOptions.env.Path = `${nodePath};${process.env.Path}`;
  } else {
    npmOptions.env.HOME = tmpDir;
    npmOptions.env.PATH = `${nodePath}:${process.env.PATH}`;
  }

  if (rebuild) {
    spawnSync(process.execPath, [
      npmBin,
      'rebuild',
    ], npmOptions);
  }

  if (install) {
    spawnSync(process.execPath, [
      npmBin,
      'install',
      '--ignore-scripts',
      '--no-save',
    ], npmOptions);
  }

  const testChild = spawn(process.execPath, [
    npmBin,
    '--silent',
    'run',
    ...testArgs,
  ], Object.assign({}, npmOptions, { stdio: 'pipe' }));

  testChild.stdout.pipe(process.stdout);
  testChild.stderr.pipe(process.stderr);

  if (logfile) {
    const logStream = createWriteStream(logfile);
    testChild.stdout.pipe(logStream);
  }

  testChild.on('exit', () => {
    common.refreshTmpDir();
    rmdirSync(tmpDir);
  });
}

function parseArgs(args) {
  let srcDir;
  let rebuild = false;
  let install = false;
  let logfile = null;
  const testArgs = [];
  args.forEach((arg) => {
    if (srcDir) {
      testArgs.push(arg);
      return;
    }

    if (arg === '--install') {
      install = true;
    } else if (arg === '--rebuild') {
      rebuild = true;
    } else if (arg[0] !== '-') {
      srcDir = path.resolve(projectDir, arg);
    } else if (arg.startsWith('--logfile=')) {
      logfile = path.resolve(projectDir, arg.slice('--logfile='.length));
    } else {
      throw new Error(`Unrecognized option ${arg}`);
    }
  });
  if (!srcDir) {
    throw new Error('Expected a source directory');
  }
  return { srcDir, install, rebuild, testArgs, logfile };
}

runNPMPackageTests(parseArgs(process.argv.slice(2)));
