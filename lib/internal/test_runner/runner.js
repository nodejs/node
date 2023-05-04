'use strict';
const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypeSome,
  ArrayPrototypeSort,
  ObjectAssign,
  PromisePrototypeThen,
  SafePromiseAllReturnVoid,
  SafePromiseAllSettledReturnVoid,
  PromiseResolve,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  TypedArrayPrototypeSubarray,
} = primordials;

const { spawn } = require('child_process');
const { readdirSync, statSync } = require('fs');
const { DefaultDeserializer, DefaultSerializer } = require('v8');
// TODO(aduh95): switch to internal/readline/interface when backporting to Node.js 16.x is no longer a concern.
const { createInterface } = require('readline');
const { deserializeError } = require('internal/error_serdes');
const { Buffer } = require('buffer');
const { FilesWatcher } = require('internal/watch_mode/files_watcher');
const console = require('internal/console/global');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_TEST_FAILURE,
  },
} = require('internal/errors');
const { validateArray, validateBoolean, validateFunction } = require('internal/validators');
const { getInspectPort, isUsingInspector, isInspectorMessage } = require('internal/util/inspector');
const { isRegExp } = require('internal/util/types');
const { kEmptyObject } = require('internal/util');
const { kEmitMessage } = require('internal/test_runner/tests_stream');
const { createTestTree } = require('internal/test_runner/harness');
const {
  kAborted,
  kCancelledByParent,
  kSubtestsFailed,
  kTestCodeFailure,
  kTestTimeoutFailure,
  Test,
} = require('internal/test_runner/test');

const {
  convertStringToRegExp,
  countCompletedTest,
  doesPathMatchFilter,
  isSupportedFileType,
} = require('internal/test_runner/utils');
const { basename, join, resolve } = require('path');
const { once } = require('events');
const {
  triggerUncaughtException,
  exitCodes: { kGenericUserError },
} = internalBinding('errors');

const kFilterArgs = ['--test', '--experimental-test-coverage', '--watch'];
const kFilterArgValues = ['--test-reporter', '--test-reporter-destination'];
const kDiagnosticsFilterArgs = ['tests', 'suites', 'pass', 'fail', 'cancelled', 'skipped', 'todo', 'duration_ms'];

const kCanceledTests = new SafeSet()
  .add(kCancelledByParent).add(kAborted).add(kTestTimeoutFailure);

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

function filterExecArgv(arg, i, arr) {
  return !ArrayPrototypeIncludes(kFilterArgs, arg) &&
  !ArrayPrototypeSome(kFilterArgValues, (p) => arg === p || (i > 0 && arr[i - 1] === p) || StringPrototypeStartsWith(arg, `${p}=`));
}

function getRunArgs({ path, inspectPort, testNamePatterns }) {
  const argv = ArrayPrototypeFilter(process.execArgv, filterExecArgv);
  if (isUsingInspector()) {
    ArrayPrototypePush(argv, `--inspect-port=${getInspectPort(inspectPort)}`);
  }
  if (testNamePatterns) {
    ArrayPrototypeForEach(testNamePatterns, (pattern) => ArrayPrototypePush(argv, `--test-name-pattern=${pattern}`));
  }
  ArrayPrototypePush(argv, path);

  return argv;
}

const serializer = new DefaultSerializer();
serializer.writeHeader();
const v8Header = serializer.releaseBuffer();
const kSerializedSizeHeader = 4;

class FileTest extends Test {
  #buffer = [];
  #messageBuffer = [];
  #messageBufferSize = 0;
  #reportedChildren = 0;
  failedSubtests = false;
  #skipReporting() {
    return this.#reportedChildren > 0 && (!this.error || this.error.failureType === kSubtestsFailed);
  }
  #checkNestedComment(comment) {
    const firstSpaceIndex = StringPrototypeIndexOf(comment, ' ');
    if (firstSpaceIndex === -1) return false;
    const secondSpaceIndex = StringPrototypeIndexOf(comment, ' ', firstSpaceIndex + 1);
    return secondSpaceIndex === -1 &&
          ArrayPrototypeIncludes(kDiagnosticsFilterArgs, StringPrototypeSlice(comment, 0, firstSpaceIndex));
  }
  #handleReportItem(item) {
    const isTopLevel = item.data.nesting === 0;
    if (isTopLevel) {
      if (item.type === 'test:plan' && this.#skipReporting()) {
        return;
      }
      if (item.type === 'test:diagnostic' && this.#checkNestedComment(item.data.message)) {
        return;
      }
    }
    if (item.data.details?.error) {
      item.data.details.error = deserializeError(item.data.details.error);
    }
    if (item.type === 'test:pass' || item.type === 'test:fail') {
      item.data.testNumber = isTopLevel ? (this.root.harness.counters.topLevel + 1) : item.data.testNumber;
      countCompletedTest({
        __proto__: null,
        name: item.data.name,
        finished: true,
        skipped: item.data.skip !== undefined,
        isTodo: item.data.todo !== undefined,
        passed: item.type === 'test:pass',
        cancelled: kCanceledTests.has(item.data.details?.error?.failureType),
        nesting: item.data.nesting,
        reportedType: item.data.details?.type,
      }, this.root.harness);
    }
    this.reporter[kEmitMessage](item.type, item.data);
  }
  #accumulateReportItem(item) {
    if (item.type !== 'test:pass' && item.type !== 'test:fail') {
      return;
    }
    this.#reportedChildren++;
    if (item.data.nesting === 0 && item.type === 'test:fail') {
      this.failedSubtests = true;
    }
  }
  #drainBuffer() {
    if (this.#buffer.length > 0) {
      ArrayPrototypeForEach(this.#buffer, (ast) => this.#handleReportItem(ast));
      this.#buffer = [];
    }
  }
  addToReport(item) {
    this.#accumulateReportItem(item);
    if (!this.isClearToSend()) {
      ArrayPrototypePush(this.#buffer, item);
      return;
    }
    this.#drainBuffer();
    this.#handleReportItem(item);
  }
  parseMessage(readData) {
    if (readData.length === 0) return;

    ArrayPrototypePush(this.#messageBuffer, readData);
    this.#messageBufferSize += readData.length;

    // Index 0 should always be present because we just pushed data into it.
    let messageBufferHead = this.#messageBuffer[0];
    let headerIndex = messageBufferHead.indexOf(v8Header);
    let nonSerialized = Buffer.alloc(0);

    while (messageBufferHead && headerIndex !== kSerializedSizeHeader) {
      const nonSerializedData = headerIndex === -1 ?
        messageBufferHead :
        messageBufferHead.slice(0, headerIndex - kSerializedSizeHeader);
      nonSerialized = Buffer.concat([nonSerialized, nonSerializedData]);
      this.#messageBufferSize -= nonSerializedData.length;
      if (headerIndex === -1) {
        ArrayPrototypeShift(this.#messageBuffer);
      } else {
        this.#messageBuffer[0] = messageBufferHead.subarray(headerIndex - kSerializedSizeHeader);
      }
      messageBufferHead = this.#messageBuffer[0];
      headerIndex = messageBufferHead?.indexOf(v8Header);
    }

    if (nonSerialized.length > 0) {
      const messages = RegExpPrototypeSymbolSplit(/\r?\n/, nonSerialized.toString('utf-8'));
      messages.forEach((message) => {
        this.addToReport({
          __proto__: null,
          type: 'test:diagnostic',
          data: { __proto__: null, nesting: 0, file: this.name, message },
        });
      });
    }

    while (messageBufferHead?.length >= kSerializedSizeHeader) {
      // We call `readUInt32BE` manually here, because this is faster than first converting
      // it to a buffer and using `readUInt32BE` on that.
      const fullMessageSize = (
        messageBufferHead[0] << 24 |
        messageBufferHead[1] << 16 |
        messageBufferHead[2] << 8 |
        messageBufferHead[3]
      ) + kSerializedSizeHeader;

      if (this.#messageBufferSize < fullMessageSize) break;

      const concatenatedBuffer = this.#messageBuffer.length === 1 ?
        this.#messageBuffer[0] : Buffer.concat(this.#messageBuffer, this.#messageBufferSize);

      const deserializer = new DefaultDeserializer(
        TypedArrayPrototypeSubarray(concatenatedBuffer, kSerializedSizeHeader, fullMessageSize),
      );

      messageBufferHead = TypedArrayPrototypeSubarray(concatenatedBuffer, fullMessageSize);
      this.#messageBufferSize = messageBufferHead.length;
      this.#messageBuffer = this.#messageBufferSize !== 0 ? [messageBufferHead] : [];

      deserializer.readHeader();
      const item = deserializer.readValue();
      this.addToReport(item);
    }
  }
  reportStarted() {}
  report() {
    this.#drainBuffer();
    const skipReporting = this.#skipReporting();
    if (!skipReporting) {
      super.reportStarted();
      super.report();
    }
  }
}

const runningProcesses = new SafeMap();
const runningSubtests = new SafeMap();

function runTestFile(path, root, inspectPort, filesWatcher, testNamePatterns) {
  const subtest = root.createSubtest(FileTest, path, async (t) => {
    const args = getRunArgs({ path, inspectPort, testNamePatterns });
    const stdio = ['pipe', 'pipe', 'pipe'];
    const env = { ...process.env, NODE_TEST_CONTEXT: 'child-v8' };
    if (filesWatcher) {
      stdio.push('ipc');
      env.WATCH_REPORT_DEPENDENCIES = '1';
    }

    const child = spawn(process.execPath, args, { signal: t.signal, encoding: 'utf8', env, stdio });
    runningProcesses.set(path, child);

    let err;

    filesWatcher?.watchChildProcessModules(child, path);

    child.on('error', (error) => {
      err = error;
    });

    child.stdout.on('data', (data) => {
      subtest.parseMessage(data);
    });

    const rl = createInterface({ input: child.stderr });
    rl.on('line', (line) => {
      if (isInspectorMessage(line)) {
        process.stderr.write(line + '\n');
        return;
      }

      // stderr cannot be treated as TAP, per the spec. However, we want to
      // surface stderr lines as TAP diagnostics to improve the DX. Inject
      // each line into the test output as an unknown token as if it came
      // from the TAP parser.
      subtest.addToReport({
        __proto__: null,
        type: 'test:diagnostic',
        data: { __proto__: null, nesting: 0, file: path, message: line },
      });
    });

    const { 0: code, 1: signal } = await once(child, 'exit', { signal: t.signal });

    runningProcesses.delete(path);
    runningSubtests.delete(path);
    if (code !== 0 || signal !== null) {
      if (!err) {
        const failureType = subtest.failedSubtests ? kSubtestsFailed : kTestCodeFailure;
        err = ObjectAssign(new ERR_TEST_FAILURE('test failed', failureType), {
          __proto__: null,
          exitCode: code,
          signal: signal,
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

function watchFiles(testFiles, root, inspectPort, testNamePatterns) {
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
      if (!runningSubtests.size) {
        // Reset the topLevel counter
        root.harness.counters.topLevel = 0;
      }
      await runningSubtests.get(file);
      runningSubtests.set(file, runTestFile(file, root, inspectPort, filesWatcher, testNamePatterns));
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
  let { testNamePatterns } = options;
  const { concurrency, timeout, signal, files, inspectPort, watch, setup } = options;

  if (files != null) {
    validateArray(files, 'options.files');
  }
  if (watch != null) {
    validateBoolean(watch, 'options.watch');
  }
  if (setup != null) {
    validateFunction(setup, 'options.setup');
  }
  if (testNamePatterns != null) {
    if (!ArrayIsArray(testNamePatterns)) {
      testNamePatterns = [testNamePatterns];
    }

    testNamePatterns = ArrayPrototypeMap(testNamePatterns, (value, i) => {
      if (isRegExp(value)) {
        return value;
      }
      const name = `options.testNamePatterns[${i}]`;
      if (typeof value === 'string') {
        return convertStringToRegExp(value, name);
      }
      throw new ERR_INVALID_ARG_TYPE(name, ['string', 'RegExp'], value);
    });
  }

  const root = createTestTree({ concurrency, timeout, signal });
  const testFiles = files ?? createTestFileList();

  let postRun = () => root.postRun();
  let filesWatcher;
  if (watch) {
    filesWatcher = watchFiles(testFiles, root, inspectPort, testNamePatterns);
    postRun = undefined;
  }
  const runFiles = () => {
    root.harness.bootstrapComplete = true;
    return SafePromiseAllSettledReturnVoid(testFiles, (path) => {
      const subtest = runTestFile(path, root, inspectPort, filesWatcher, testNamePatterns);
      runningSubtests.set(path, subtest);
      return subtest;
    });
  };

  PromisePrototypeThen(PromisePrototypeThen(PromiseResolve(setup?.(root)), runFiles), postRun);

  return root.reporter;
}

module.exports = { run };
