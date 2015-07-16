// Clone repo and run tests for popular npm packages

'use strict';
var os = require('os');
var fs = require('fs');
var path = require('path');
var spawnSync = require('child_process').spawnSync;
var common = require('../common');

const iojs = path.resolve(__dirname, '../../iojs');
const npm = path.resolve(__dirname, '../../deps/npm/cli.js');

// .eslintrc in iojs will be used if we do our work in iojs/test/tmpdir.
// So.....override with something in os.tmpdir().
common.tmpDir = path.resolve(os.tmpdir(), 'node-smoke-test');

common.refreshTmpDir();
process.chdir(common.tmpDir);
fs.mkdirSync('.bin');
process.chdir('.bin');
fs.symlinkSync(iojs, 'node');
fs.symlinkSync(iojs, 'iojs');
fs.symlinkSync(npm, 'npm');
const pathPrepend = path.resolve(common.tmpDir, '.bin');

process.envPATH = pathPrepend + path.delimiter + process.env.PATH;

const checkResult = function(result) {
  if (result.status !== 0) {
    if (result.stdout) {
      console.log(result.stdout.toString());
    }
    if (result.stderr) {
      console.error(result.stderr.toString());
    }
    const error = result.error || new Error('spawned command failed');
    throw error;
  }
};

exports.npmTest = function(module) {
  console.error('Running smoke test for module "' + module + '"...');
  const modulePath = path.resolve(common.tmpDir, module);
  fs.mkdirSync(modulePath);
  process.chdir(modulePath);

  console.error('...installing...');
  var result = spawnSync('npm', ['install', module, '--cache-min=Infinity']);
  checkResult(result);

  const packageJsonPath = path.resolve(modulePath, 'node_modules', module,
    'package.json');
  const metadata = require(packageJsonPath);
  const packageUrl = metadata.repository.url.replace(/^git\+/, '');

  console.error('...cloning repo...');
  result = spawnSync('git', ['clone', packageUrl, '--depth=1']);
  checkResult(result);

  process.chdir(path.basename(packageUrl).replace(/\.git$/, ''));

  console.error('...installing devDependencies...');
  result = spawnSync('npm', ['install', '--cache-min=Infinity']);
  checkResult(result);

  console.error('...running tests...');
  result = spawnSync('npm', ['test']);
  checkResult(result);

  console.error('Tests succeeded for module "' + module + '".\n');
};
