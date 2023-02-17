'use strict';
const {
  ArrayFrom,
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSome,
  ArrayPrototypeSort,
  FunctionPrototypeCall,
  Number,
  ObjectAssign,
  ObjectKeys,
  PromisePrototypeThen,
  SafePromiseAllReturnVoid,
  SafePromiseAllSettledReturnVoid,
  SafeMap,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
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
const {
  kAborted,
  kCancelledByParent,
  kSubtestsFailed,
  kTestCodeFailure,
  kTestTimeoutFailure,
  Test,
} = require('internal/test_runner/test');
const { TapParser } = require('internal/test_runner/tap_parser');
const { YAMLToJs } = require('internal/test_runner/yaml_to_js');
const { TokenKind } = require('internal/test_runner/tap_lexer');

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

const kFilterArgs = ['--test', '--experimental-test-coverage', '--watch'];
const kFilterArgValues = ['--test-reporter', '--test-reporter-destination'];
const kDiagnosticsFilterArgs = ['tests', 'pass', 'fail', 'cancelled', 'skipped', 'todo', 'duration_ms'];

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

function getRunArgs({ path, inspectPort }) {
  const argv = ArrayPrototypeFilter(process.execArgv, filterExecArgv);
  if (isUsingInspector()) {
    ArrayPrototypePush(argv, `--inspect-port=${getInspectPort(inspectPort)}`);
  }
  ArrayPrototypePush(argv, path);
  return argv;
}

class FileTest extends Test {
  #buffer = [];
  #counters = { __proto__: null, all: 0, failed: 0, passed: 0, cancelled: 0, skipped: 0, todo: 0, totalFailed: 0 };
  failedSubtests = false;
  #skipReporting() {
    return this.#counters.all > 0 && (!this.error || this.error.failureType === kSubtestsFailed);
  }
  #checkNestedComment({ comment }) {
    const firstSpaceIndex = StringPrototypeIndexOf(comment, ' ');
    if (firstSpaceIndex === -1) return false;
    const secondSpaceIndex = StringPrototypeIndexOf(comment, ' ', firstSpaceIndex + 1);
    return secondSpaceIndex === -1 &&
          ArrayPrototypeIncludes(kDiagnosticsFilterArgs, StringPrototypeSlice(comment, 0, firstSpaceIndex));
  }
  #handleReportItem({ kind, node, comments, nesting = 0 }) {
    if (comments) {
      ArrayPrototypeForEach(comments, (comment) => this.reporter.diagnostic(nesting, this.name, comment));
    }
    switch (kind) {
      case TokenKind.TAP_VERSION:
        // TODO(manekinekko): handle TAP version coming from the parser.
        // this.reporter.version(node.version);
        break;

      case TokenKind.TAP_PLAN:
        if (nesting === 0 && this.#skipReporting()) {
          break;
        }
        this.reporter.plan(nesting, this.name, node.end - node.start + 1);
        break;

      case TokenKind.TAP_SUBTEST_POINT:
        this.reporter.start(nesting, this.name, node.name);
        break;

      case TokenKind.TAP_TEST_POINT: {

        const { todo, skip, pass } = node.status;

        let directive;

        if (skip) {
          directive = this.reporter.getSkip(node.reason);
        } else if (todo) {
          directive = this.reporter.getTodo(node.reason);
        } else {
          directive = kEmptyObject;
        }

        const diagnostics = YAMLToJs(node.diagnostics);
        const cancelled = kCanceledTests.has(diagnostics.error?.failureType);
        const testNumber = nesting === 0 ? (Number(node.id) + this.testNumber - 1) : node.id;
        const method = pass ? 'ok' : 'fail';
        this.reporter[method](nesting, this.name, testNumber, node.description, diagnostics, directive);
        if (nesting === 0) {
          FunctionPrototypeCall(super.countSubtest,
                                { finished: true, skipped: skip, isTodo: todo, passed: pass, cancelled },
                                this.#counters);
          this.failedSubtests ||= !pass;
        }
        break;

      }
      case TokenKind.COMMENT:
        if (nesting === 0 && this.#checkNestedComment(node)) {
          // Ignore file top level diagnostics
          break;
        }
        this.reporter.diagnostic(nesting, this.name, node.comment);
        break;

      case TokenKind.UNKNOWN:
        this.reporter.diagnostic(nesting, this.name, node.value);
        break;
    }
  }
  addToReport(ast) {
    if (!this.isClearToSend()) {
      ArrayPrototypePush(this.#buffer, ast);
      return;
    }
    this.reportStarted();
    this.#handleReportItem(ast);
  }
  countSubtest(counters) {
    if (this.#counters.all === 0) {
      return super.countSubtest(counters);
    }
    ArrayPrototypeForEach(ObjectKeys(counters), (key) => {
      counters[key] += this.#counters[key];
    });
  }
  reportStarted() {}
  report() {
    const skipReporting = this.#skipReporting();
    if (!skipReporting) {
      super.reportStarted();
    }
    ArrayPrototypeForEach(this.#buffer, (ast) => this.#handleReportItem(ast));
    if (!skipReporting) {
      super.report();
    }
  }
}

const runningProcesses = new SafeMap();
const runningSubtests = new SafeMap();

function runTestFile(path, root, inspectPort, filesWatcher) {
  const subtest = root.createSubtest(FileTest, path, async (t) => {
    const args = getRunArgs({ path, inspectPort });
    const stdio = ['pipe', 'pipe', 'pipe'];
    const env = { ...process.env };
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
      const node = {
        kind: TokenKind.UNKNOWN,
        node: {
          value: line,
        },
      };

      subtest.addToReport(node);
    });

    const parser = new TapParser();

    child.stdout.pipe(parser).on('data', (ast) => {
      subtest.addToReport(ast);
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
