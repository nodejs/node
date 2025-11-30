'use strict';
const common = require('.');
const path = require('node:path');
const test = require('node:test');
const fs = require('node:fs/promises');
const assert = require('node:assert/strict');
const { hostname } = require('node:os');

const stackFramesRegexp = /(?<=\n)(\s+)((.+?)\s+\()?(?:\(?(.+?):(\d+)(?::(\d+))?)\)?(\s+\{)?(\[\d+m)?(\n|$)/g;
const windowNewlineRegexp = /\r/g;

function replaceNodeVersion(str) {
  return str.replaceAll(process.version, '*');
}

function replaceStackTrace(str, replacement = '$1*$7$8\n') {
  return str.replace(stackFramesRegexp, replacement);
}

function replaceInternalStackTrace(str) {
  // Replace non-internal frame `at TracingChannel.traceSync (node:diagnostics_channel:328:14)`
  // as well as `at node:internal/main/run_main_module:33:47` with `*`.
  return str.replaceAll(/(\W+).*[(\s]node:.*/g, '$1*');
}

function replaceWindowsLineEndings(str) {
  return str.replace(windowNewlineRegexp, '');
}

function replaceWindowsPaths(str) {
  return common.isWindows ? str.replaceAll(path.win32.sep, path.posix.sep) : str;
}

function transformProjectRoot(replacement = '') {
  const projectRoot = path.resolve(__dirname, '../..');
  return (str) => {
    return str.replaceAll('\\\'', "'").replaceAll(projectRoot, replacement);
  };
}

function transform(...args) {
  return (str) => args.reduce((acc, fn) => fn(acc), str);
}

function getSnapshotPath(filename) {
  const { name, dir } = path.parse(filename);
  return path.resolve(dir, `${name}.snapshot`);
}

async function assertSnapshot(actual, filename = process.argv[1]) {
  const snapshot = getSnapshotPath(filename);
  if (process.env.NODE_REGENERATE_SNAPSHOTS) {
    await fs.writeFile(snapshot, actual);
  } else {
    let expected;
    try {
      expected = await fs.readFile(snapshot, 'utf8');
    } catch (e) {
      if (e.code === 'ENOENT') {
        console.log(
          'Snapshot file does not exist. You can create a new one by running the test with NODE_REGENERATE_SNAPSHOTS=1',
        );
      }
      throw e;
    }
    assert.strictEqual(actual, replaceWindowsLineEndings(expected));
  }
}

/**
 * Spawn a process and assert its output against a snapshot.
 * if you want to automatically update the snapshot, run tests with NODE_REGENERATE_SNAPSHOTS=1
 * transform is a function that takes the output and returns a string that will be compared against the snapshot
 * this is useful for normalizing output such as stack traces
 * there are some predefined transforms in this file such as replaceStackTrace and replaceWindowsLineEndings
 * both of which can be used as an example for writing your own
 * compose multiple transforms by passing them as arguments to the transform function:
 * assertSnapshot.transform(assertSnapshot.replaceStackTrace, assertSnapshot.replaceWindowsLineEndings)
 * @param {string} filename
 * @param {function(string): string} [transform]
 * @param {object} [options] - control how the child process is spawned
 * @param {boolean} [options.tty] - whether to spawn the process in a pseudo-tty
 * @returns {Promise<void>}
 */
async function spawnAndAssert(filename, transform = (x) => x, { tty = false, ...options } = {}) {
  if (tty && common.isWindows) {
    test({ skip: 'Skipping pseudo-tty tests, as pseudo terminals are not available on Windows.' });
    return;
  }
  let { flags } = common.parseTestMetadata(filename);
  if (options.flags) {
    flags = [...options.flags, ...flags];
  }

  const executable = tty ? (process.env.PYTHON || 'python3') : process.execPath;
  const args =
    tty ?
      [path.join(__dirname, '../..', 'tools/pseudo-tty.py'), process.execPath, ...flags, filename] :
      [...flags, filename];
  const { stdout, stderr } = await common.spawnPromisified(executable, args, options);
  await assertSnapshot(transform(`${stdout}${stderr}`), filename);
}

function replaceTestDuration(str) {
  return str
    .replaceAll(/duration_ms: [0-9.]+/g, 'duration_ms: *')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *');
}

const root = path.resolve(__dirname, '..', '..');
const color = '(\\[\\d+m)';
const stackTraceBasePath = new RegExp(`${color}\\(${RegExp.escape(root)}/?${color}(.*)${color}\\)`, 'g');

function replaceSpecDuration(str) {
  return str
    .replaceAll(/[0-9.]+ms/g, '*ms')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replace(stackTraceBasePath, '$3');
}

function replaceJunitDuration(str) {
  return str
    .replaceAll(/time="[0-9.]+"/g, 'time="*"')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replaceAll(`hostname="${hostname()}"`, 'hostname="HOSTNAME"')
    .replaceAll(/file="[^"]*"/g, 'file="*"')
    .replace(stackTraceBasePath, '$3');
}

function removeWindowsPathEscaping(str) {
  return common.isWindows ? str.replaceAll(/\\\\/g, '\\') : str;
}

function replaceTestLocationLine(str) {
  return str.replaceAll(/(js:)(\d+)(:\d+)/g, '$1(LINE)$3');
}

// The Node test coverage returns results for all files called by the test. This
// will make the output file change if files like test/common/index.js change.
// This transform picks only the first line and then the lines from the test
// file.
function pickTestFileFromLcov(str) {
  const lines = str.split(/\n/);
  const firstLineOfTestFile = lines.findIndex(
    (line) => line.startsWith('SF:') && line.trim().endsWith('output.js'),
  );
  const lastLineOfTestFile = lines.findIndex(
    (line, index) => index > firstLineOfTestFile && line.trim() === 'end_of_record',
  );
  return (
    lines[0] + '\n' + lines.slice(firstLineOfTestFile, lastLineOfTestFile + 1).join('\n') + '\n'
  );
}

const defaultTransform = transform(
  replaceWindowsLineEndings,
  replaceStackTrace,
  removeWindowsPathEscaping,
  transformProjectRoot(),
  replaceWindowsPaths,
  replaceTestDuration,
  replaceTestLocationLine,
);
const specTransform = transform(
  replaceSpecDuration,
  replaceWindowsLineEndings,
  replaceStackTrace,
  replaceWindowsPaths,
);
const junitTransform = transform(
  replaceJunitDuration,
  replaceWindowsLineEndings,
  replaceStackTrace,
  replaceWindowsPaths,
);
const lcovTransform = transform(
  replaceWindowsLineEndings,
  replaceStackTrace,
  transformProjectRoot(),
  replaceWindowsPaths,
  pickTestFileFromLcov,
);

function ensureCwdIsProjectRoot() {
  if (process.cwd() !== root) {
    process.chdir(root);
  }
}

function canColorize() {
  // Loading it lazily to avoid breaking `NODE_REGENERATE_SNAPSHOTS`.
  return require('internal/tty').getColorDepth() > 2;
}

module.exports = {
  assertSnapshot,
  getSnapshotPath,
  replaceNodeVersion,
  replaceStackTrace,
  replaceInternalStackTrace,
  replaceWindowsLineEndings,
  replaceWindowsPaths,
  spawnAndAssert,
  transform,
  transformProjectRoot,
  replaceTestDuration,
  defaultTransform,
  specTransform,
  junitTransform,
  lcovTransform,
  ensureCwdIsProjectRoot,
  canColorize,
};
