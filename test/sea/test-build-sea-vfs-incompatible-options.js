// This tests that --build-sea rejects "useVfs" combined with options it
// does not support.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { skipIfBuildSEAIsNotSupported } = require('../common/sea');
const { writeFileSync } = require('fs');
const { spawnSyncAndAssert } = require('../common/child_process');

skipIfBuildSEAIsNotSupported();

// Test: "useVfs" is not a Boolean
{
  tmpdir.refresh();
  const config = tmpdir.resolve('invalid-useVfs.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useVfs": "true"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"useVfs" field of .*invalid-useVfs\.json is not a Boolean/,
    });
}

// Test: "useVfs" with "useSnapshot"
{
  tmpdir.refresh();
  const config = tmpdir.resolve('vfs-snapshot.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useVfs": true,
  "useSnapshot": true
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"useVfs" is not supported when "useSnapshot" is true/,
    });
}

// Test: "useVfs" with "useCodeCache"
{
  tmpdir.refresh();
  const config = tmpdir.resolve('vfs-code-cache.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useVfs": true,
  "useCodeCache": true
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"useVfs" is not supported when "useCodeCache" is true/,
    });
}

// Test: "vfsArchive" without "useVfs"
{
  tmpdir.refresh();
  const config = tmpdir.resolve('vfs-archive-no-vfs.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "vfsArchive": "assets.zip"
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"vfsArchive" requires "useVfs" to be true/,
    });
}

// Test: "vfsArchive" combined with "assets"
{
  tmpdir.refresh();
  const config = tmpdir.resolve('vfs-archive-and-assets.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useVfs": true,
  "vfsArchive": "assets.zip",
  "assets": { "a.txt": "a.txt" }
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"vfsArchive" cannot be used together with "assets"/,
    });
}

// Test: "vfsArchive" is not a string
{
  tmpdir.refresh();
  const config = tmpdir.resolve('vfs-archive-not-string.json');
  writeFileSync(config, `
{
  "main": "bundle.js",
  "output": "sea",
  "useVfs": true,
  "vfsArchive": true
}
  `, 'utf8');
  spawnSyncAndAssert(
    process.execPath,
    ['--build-sea', config], {
      cwd: tmpdir.path,
    }, {
      status: 1,
      stderr: /"vfsArchive" field of .*vfs-archive-not-string\.json is not a string/,
    });
}
