'use strict';
const {
  ArrayFrom,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Promise,
  SafeSet,
} = primordials;
const {
  prepareMainThreadExecution,
} = require('internal/bootstrap/pre_execution');
const { spawn } = require('child_process');
const { readdirSync, statSync } = require('fs');
const console = require('internal/console/global');
const {
  codes: {
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const test = require('internal/test_runner/harness');
const { kSubtestsFailed } = require('internal/test_runner/test');
const {
  isSupportedFileType,
  doesPathMatchFilter,
} = require('internal/test_runner/utils');
const { basename, join, resolve } = require('path');
const kFilterArgs = ['--test'];

prepareMainThreadExecution(false);
markBootstrapComplete();

// TODO(cjihrig): Replace this with recursive readdir once it lands.
function processPath(path, testFiles, options) {
  const stats = statSync(path);

  if (stats.isFile()) {
    if (options.userSupplied ||
        (options.underTestDir && isSupportedFileType(path)) ||
        doesPathMatchFilter(path)) {
      testFiles.add(path);
    }
  } else if (stats.isDirectory()) {
    const name = basename(path);

    if (!options.userSupplied && name === 'node_modules') {
      return;
    }

    // 'test' directories get special treatment. Recursively add all .js,
    // .cjs, and .mjs files in the 'test' directory.
    const isTestDir = name === 'test';
    const { underTestDir } = options;
    const entries = readdirSync(path);

    if (isTestDir) {
      options.underTestDir = true;
    }

    options.userSupplied = false;

    for (let i = 0; i < entries.length; i++) {
      processPath(join(path, entries[i]), testFiles, options);
    }

    options.underTestDir = underTestDir;
  }
}

function createTestFileList() {
  const cwd = process.cwd();
  const hasUserSuppliedPaths = process.argv.length > 1;
  const testPaths = hasUserSuppliedPaths ?
    ArrayPrototypeSlice(process.argv, 1) : [cwd];
  const testFiles = new SafeSet();

  try {
    for (let i = 0; i < testPaths.length; i++) {
      const absolutePath = resolve(testPaths[i]);

      processPath(absolutePath, testFiles, { userSupplied: true });
    }
  } catch (err) {
    if (err?.code === 'ENOENT') {
      console.error(`Could not find '${err.path}'`);
      process.exit(1);
    }

    throw err;
  }

  return ArrayPrototypeSort(ArrayFrom(testFiles));
}

function filterExecArgv(arg) {
  return !ArrayPrototypeIncludes(kFilterArgs, arg);
}

function runTestFile(path) {
  return test(path, () => {
    return new Promise((resolve, reject) => {
      const args = ArrayPrototypeFilter(process.execArgv, filterExecArgv);
      ArrayPrototypePush(args, path);

      const child = spawn(process.execPath, args);
      // TODO(cjihrig): Implement a TAP parser to read the child's stdout
      // instead of just displaying it all if the child fails.
      let stdout = '';
      let stderr = '';
      let err;

      child.on('error', (error) => {
        err = error;
      });

      child.stdout.setEncoding('utf8');
      child.stderr.setEncoding('utf8');

      child.stdout.on('data', (chunk) => {
        stdout += chunk;
      });

      child.stderr.on('data', (chunk) => {
        stderr += chunk;
      });

      child.once('exit', (code, signal) => {
        if (code !== 0 || signal !== null) {
          if (!err) {
            err = new ERR_TEST_FAILURE('test failed', kSubtestsFailed);
            err.exitCode = code;
            err.signal = signal;
            err.stdout = stdout;
            err.stderr = stderr;
            // The stack will not be useful since the failures came from tests
            // in a child process.
            err.stack = undefined;
          }

          return reject(err);
        }

        resolve();
      });
    });
  });
}

(async function main() {
  const testFiles = createTestFileList();

  for (let i = 0; i < testFiles.length; i++) {
    runTestFile(testFiles[i]);
  }
})();
