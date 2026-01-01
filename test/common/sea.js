'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { inspect } = require('util');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const { readFileSync, copyFileSync, statSync } = require('fs');
const {
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

function skipIfSingleExecutableIsNotSupported() {
  if (!process.config.variables.single_executable_application)
    common.skip('Single Executable Application support has been disabled.');

  if (!['darwin', 'win32', 'linux'].includes(process.platform))
    common.skip(`Unsupported platform ${process.platform}.`);

  if (process.platform === 'linux' && process.config.variables.is_debug === 1)
    common.skip('Running the resultant binary fails with `Couldn\'t read target executable"`.');

  if (process.config.variables.node_shared)
    common.skip('Running the resultant binary fails with ' +
      '`/home/iojs/node-tmp/.tmp.2366/sea: error while loading shared libraries: ' +
      'libnode.so.112: cannot open shared object file: No such file or directory`.');

  if (process.config.variables.icu_gyp_path === 'tools/icu/icu-system.gyp')
    common.skip('Running the resultant binary fails with ' +
      '`/home/iojs/node-tmp/.tmp.2379/sea: error while loading shared libraries: ' +
      'libicui18n.so.71: cannot open shared object file: No such file or directory`.');

  if (!process.config.variables.node_use_openssl || process.config.variables.node_shared_openssl)
    common.skip('Running the resultant binary fails with `Node.js is not compiled with OpenSSL crypto support`.');

  if (process.config.variables.want_separate_host_toolset !== 0)
    common.skip('Running the resultant binary fails with `Segmentation fault (core dumped)`.');

  if (process.platform === 'linux') {
    const osReleaseText = readFileSync('/etc/os-release', { encoding: 'utf-8' });
    const isAlpine = /^NAME="Alpine Linux"/m.test(osReleaseText);
    if (isAlpine) common.skip('Alpine Linux is not supported.');

    if (process.arch === 's390x') {
      common.skip('On s390x, postject fails with `memory access out of bounds`.');
    }
  }

  if (process.config.variables.ubsan) {
    common.skip('UndefinedBehavior Sanitizer is not supported');
  }

  try {
    readFileSync(process.execPath);
  } catch (e) {
    if (e.code === 'ERR_FS_FILE_TOO_LARGE') {
      common.skip('The Node.js binary is too large to be supported by postject');
    }
  }

  tmpdir.refresh();

  // The SEA tests involve making a copy of the executable and writing some fixtures
  // to the tmpdir. To be safe, ensure that the disk space has at least a copy of the
  // executable and some extra space for blobs and configs is available.
  const stat = statSync(process.execPath);
  const expectedSpace = stat.size + 10 * 1024 * 1024;
  if (!tmpdir.hasEnoughSpace(expectedSpace)) {
    common.skip(`Available disk space < ${Math.floor(expectedSpace / 1024 / 1024)} MB`);
  }
}

function generateSEA(fixtureDir, options = {}) {
  const {
    workingDir = tmpdir.path,
    configPath = 'sea-config.json',
    verifyWorkflow = false,
  } = options;
  // Copy fixture files to working directory if they are different.
  if (fixtureDir !== workingDir) {
    fs.cpSync(fixtureDir, workingDir, { recursive: true });
  }

  // Determine the output executable path.
  const outputFile = path.resolve(workingDir, process.platform === 'win32' ? 'sea.exe' : 'sea');

  try {
    // Copy the executable.
    copyFileSync(process.execPath, outputFile);
    console.log(`Copied ${process.execPath} to ${outputFile}`);
  } catch (e) {
    const message = `Cannot copy ${process.execPath} to ${outputFile}: ${inspect(e)}`;
    if (verifyWorkflow) {
      throw new Error(message);
    }
    common.skip(message);
  }

  // Generate the blob using --experimental-sea-config.
  spawnSyncAndExitWithoutError(
    process.execPath,
    ['--experimental-sea-config', configPath],
    {
      cwd: workingDir,
      env: {
        NODE_DEBUG_NATIVE: 'SEA',
        ...process.env,
      },
    },
  );

  // Parse the config to get the output file path.
  const config = JSON.parse(fs.readFileSync(path.resolve(workingDir, configPath)));
  assert.strictEqual(typeof config.output, 'string');
  const seaPrepBlob = path.resolve(workingDir, config.output);
  assert(fs.existsSync(seaPrepBlob), `Expected SEA blob ${seaPrepBlob} to exist`);

  // Use postject to inject the blob.
  const postjectFile = fixtures.path('postject-copy', 'node_modules', 'postject', 'dist', 'cli.js');
  try {
    spawnSyncAndExitWithoutError(process.execPath, [
      postjectFile,
      outputFile,
      'NODE_SEA_BLOB',
      seaPrepBlob,
      '--sentinel-fuse', 'NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2',
      ...process.platform === 'darwin' ? [ '--macho-segment-name', 'NODE_SEA' ] : [],
    ]);
  } catch (e) {
    const message = `Cannot inject ${seaPrepBlob} into ${outputFile}: ${inspect(e)}`;
    if (verifyWorkflow) {
      throw new Error(message);
    }
    common.skip(message);
  }
  console.log(`Injected ${seaPrepBlob} into ${outputFile}`);

  signSEA(outputFile, verifyWorkflow);
  return outputFile;
}

function signSEA(targetExecutable, verifyWorkflow = false) {
  if (process.platform === 'darwin') {
    try {
      spawnSyncAndExitWithoutError('codesign', [ '--sign', '-', targetExecutable ]);
      spawnSyncAndExitWithoutError('codesign', [ '--verify', targetExecutable ]);
    } catch (e) {
      const message = `Cannot sign ${targetExecutable}: ${inspect(e)}`;
      if (verifyWorkflow) {
        throw new Error(message);
      }
      common.skip(message);
    }
    console.log(`Signed ${targetExecutable}`);
  } else if (process.platform === 'win32') {
    try {
      spawnSyncAndExitWithoutError('where', [ 'signtool' ]);
    } catch (e) {
      const message = `Cannot find signtool: ${inspect(e)}`;
      if (verifyWorkflow) {
        throw new Error(message);
      }
      common.skip(message);
    }
    let stderr;
    try {
      ({ stderr } = spawnSyncAndExitWithoutError('signtool', [ 'sign', '/fd', 'SHA256', targetExecutable ]));
      spawnSyncAndExitWithoutError('signtool', ['verify', '/pa', 'SHA256', targetExecutable]);
    } catch (e) {
      const message = `Cannot sign ${targetExecutable}: ${inspect(e)}\n${stderr}`;
      if (verifyWorkflow) {
        throw new Error(message);
      }
      common.skip(message);
    }
    console.log(`Signed ${targetExecutable}`);
  }
}

module.exports = {
  skipIfSingleExecutableIsNotSupported,
  generateSEA,
};
