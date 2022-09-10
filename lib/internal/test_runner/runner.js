'use strict';
const {
  ArrayFrom,
  ArrayPrototypeConcat,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  ObjectAssign,
  PromisePrototypeThen,
  SafePromiseAll,
  SafeSet,
} = primordials;

const { spawn } = require('child_process');
const { readdirSync, statSync } = require('fs');
const console = require('internal/console/global');
const {
  codes: {
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const { validateArray } = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { createTestTree } = require('internal/test_runner/harness');
const { kSubtestsFailed, Test } = require('internal/test_runner/test');
const {
  isSupportedFileType,
  doesPathMatchFilter,
} = require('internal/test_runner/utils');
const { basename, join, resolve } = require('path');
const { once } = require('events');

const kFilterArgs = ['--test'];

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

function runTestFile(path, root) {
  const subtest = root.createSubtest(Test, path, async (t) => {
    const args = ArrayPrototypeConcat(
      ArrayPrototypeFilter(process.execArgv, filterExecArgv),
      path);

    const child = spawn(process.execPath, args, { signal: t.signal, encoding: 'utf8' });
    // TODO(cjihrig): Implement a TAP parser to read the child's stdout
    // instead of just displaying it all if the child fails.
    let err;

    child.on('error', (error) => {
      err = error;
    });

    const { 0: { 0: code, 1: signal }, 1: stdout, 2: stderr } = await SafePromiseAll([
      once(child, 'exit', { signal: t.signal }),
      child.stdout.toArray({ signal: t.signal }),
      child.stderr.toArray({ signal: t.signal }),
    ]);

    if (code !== 0 || signal !== null) {
      if (!err) {
        err = ObjectAssign(new ERR_TEST_FAILURE('test failed', kSubtestsFailed), {
          __proto__: null,
          exitCode: code,
          signal: signal,
          stdout: ArrayPrototypeJoin(stdout, ''),
          stderr: ArrayPrototypeJoin(stderr, ''),
          // The stack will not be useful since the failures came from tests
          // in a child process.
          stack: undefined,
        });
      }

      throw err;
    }
  });
  return subtest.start();
}

function run(options) {
  if (options === null || typeof options !== 'object') {
    options = kEmptyObject;
  }
  const { concurrency, timeout, signal, files } = options;

  if (files != null) {
    validateArray(files, 'options.files');
  }

  const root = createTestTree({ concurrency, timeout, signal });
  const testFiles = files ?? createTestFileList();

  PromisePrototypeThen(SafePromiseAll(testFiles, (path) => runTestFile(path, root)),
                       () => root.postRun());

  return root.reporter;
}

module.exports = { run };
