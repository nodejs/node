'use strict';
const {
  ArrayFrom,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  ObjectAssign,
  PromisePrototypeThen,
  SafePromiseAll,
  SafePromiseAllReturnVoid,
  SafePromiseAllSettledReturnVoid,
  SafeMap,
  SafeSet,
} = primordials;

const { spawn } = require('child_process');
const { readdirSync, statSync } = require('fs');
// TODO(aduh95): switch to internal/readline/interface when backporting to Node.js 16.x is no longer a concern.
const { createInterface } = require('readline');
const { FilesWatcher } = require('internal/watch_mode/files_watcher');
const console = require('internal/console/global');
const {
  codes: {
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const { validateArray, validateBoolean } = require('internal/validators');
const { getInspectPort, isUsingInspector, isInspectorMessage } = require('internal/util/inspector');
const { kEmptyObject } = require('internal/util');
const { createTestTree } = require('internal/test_runner/harness');
const { kSubtestsFailed, Test } = require('internal/test_runner/test');
const {
  isSupportedFileType,
  doesPathMatchFilter,
} = require('internal/test_runner/utils');
const { basename, join, resolve } = require('path');
const { once } = require('events');
const {
  triggerUncaughtException,
  exitCodes: { kGenericUserError },
} = internalBinding('errors');

const kFilterArgs = ['--test', '--watch'];

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
      process.exit(kGenericUserError);
    }

    throw err;
  }

  return ArrayPrototypeSort(ArrayFrom(testFiles));
}

function filterExecArgv(arg) {
  return !ArrayPrototypeIncludes(kFilterArgs, arg);
}

function getRunArgs({ path, inspectPort }) {
  const argv = ArrayPrototypeFilter(process.execArgv, filterExecArgv);
  if (isUsingInspector()) {
    ArrayPrototypePush(argv, `--inspect-port=${getInspectPort(inspectPort)}`);
  }
  ArrayPrototypePush(argv, path);
  return argv;
}

const runningProcesses = new SafeMap();
const runningSubtests = new SafeMap();

function runTestFile(path, root, inspectPort, filesWatcher) {
  const subtest = root.createSubtest(Test, path, async (t) => {
    const args = getRunArgs({ path, inspectPort });
    const stdio = ['pipe', 'pipe', 'pipe'];
    const env = { ...process.env };
    if (filesWatcher) {
      stdio.push('ipc');
      env.WATCH_REPORT_DEPENDENCIES = '1';
    }

    const child = spawn(process.execPath, args, { signal: t.signal, encoding: 'utf8', env, stdio });
    runningProcesses.set(path, child);
    // TODO(cjihrig): Implement a TAP parser to read the child's stdout
    // instead of just displaying it all if the child fails.
    let err;
    let stderr = '';

    filesWatcher?.watchChildProcessModules(child, path);

    child.on('error', (error) => {
      err = error;
    });

    child.stderr.on('data', (data) => {
      stderr += data;
    });

    if (isUsingInspector()) {
      const rl = createInterface({ input: child.stderr });
      rl.on('line', (line) => {
        if (isInspectorMessage(line)) {
          process.stderr.write(line + '\n');
        }
      });
    }

    const { 0: { 0: code, 1: signal }, 1: stdout } = await SafePromiseAll([
      once(child, 'exit', { signal: t.signal }),
      child.stdout.toArray({ signal: t.signal }),
    ]);

    runningProcesses.delete(path);
    runningSubtests.delete(path);
    if (code !== 0 || signal !== null) {
      if (!err) {
        err = ObjectAssign(new ERR_TEST_FAILURE('test failed', kSubtestsFailed), {
          __proto__: null,
          exitCode: code,
          signal: signal,
          stdout: ArrayPrototypeJoin(stdout, ''),
          stderr,
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

function watchFiles(testFiles, root, inspectPort) {
  const filesWatcher = new FilesWatcher({ throttle: 500, mode: 'filter' });
  filesWatcher.on('changed', ({ owners }) => {
    filesWatcher.unfilterFilesOwnedBy(owners);
    PromisePrototypeThen(SafePromiseAllReturnVoid(testFiles, async (file) => {
      if (!owners.has(file)) {
        return;
      }
      const runningProcess = runningProcesses.get(file);
      if (runningProcess) {
        runningProcess.kill();
        await once(runningProcess, 'exit');
      }
      await runningSubtests.get(file);
      runningSubtests.set(file, runTestFile(file, root, inspectPort, filesWatcher));
    }, undefined, (error) => {
      triggerUncaughtException(error, true /* fromPromise */);
    }));
  });
  return filesWatcher;
}

function run(options) {
  if (options === null || typeof options !== 'object') {
    options = kEmptyObject;
  }
  const { concurrency, timeout, signal, files, inspectPort, watch } = options;

  if (files != null) {
    validateArray(files, 'options.files');
  }
  if (watch != null) {
    validateBoolean(watch, 'options.watch');
  }

  const root = createTestTree({ concurrency, timeout, signal });
  const testFiles = files ?? createTestFileList();

  let postRun = () => root.postRun();
  let filesWatcher;
  if (watch) {
    filesWatcher = watchFiles(testFiles, root, inspectPort);
    postRun = undefined;
  }

  PromisePrototypeThen(SafePromiseAllSettledReturnVoid(testFiles, (path) => {
    const subtest = runTestFile(path, root, inspectPort, filesWatcher);
    runningSubtests.set(path, subtest);
    return subtest;
  }), postRun);


  return root.reporter;
}

module.exports = { run };
