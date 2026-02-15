'use strict';
const common = require('.');
const path = require('node:path');
const test = require('node:test');
const fs = require('node:fs/promises');
const { realpathSync } = require('node:fs');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const { hostname } = require('node:os');

/* eslint-disable @stylistic/js/max-len,no-control-regex */
/**
 * Group 1: Line start (including color codes and escapes)
 * Group 2: Function name
 * Group 3: Filename
 * Group 4: Line number
 * Group 5: Column number
 * Group 6: Line end (including color codes and `{` which indicates the start of an error object details)
 */
// Mappings:                              (g1                             )   (g2 )          (g3      ) (g4 )    (g5 )      (g6                       )
const internalStackFramesRegexp = /(?<=\n)(\s*(?:\x1b?\[\d+m\s+)?(?:at\s+)?)(?:async\s+)?(?:(.+?)\s+\()?(?:(node:.+?):(\d+)(?::(\d+))?)\)?((?:\x1b?\[\d+m)?\s*{?\n|$)/g;
/**
 * Group 1: Filename
 * Group 2: Line number
 * Group 3: Line end and source code line
 */
const internalErrorSourceLines = /(?<=\n|^)(node:.+?):(\d+)(\n.*\n\s*\^(?:\n|$))/g;
/* eslint-enable @stylistic/js/max-len,no-control-regex */

const windowNewlineRegexp = /\r/g;

// Replaces the current Node.js executable version strings with a
// placeholder. This could commonly present in an unhandled exception
// output.
function replaceNodeVersion(str) {
  return str.replaceAll(`Node.js ${process.version}`, 'Node.js <node-version>');
}

// Collapse consecutive identical lines containing the keyword into
// one single line. The `str` should have been processed by `replaceWindowsLineEndings`.
function foldIdenticalLines(str, keyword) {
  const lines = str.split('\n');
  const folded = lines.filter((line, idx) => {
    if (idx === 0) {
      return true;
    }
    if (line.includes(keyword) && line === lines[idx - 1]) {
      return false;
    }
    return true;
  });
  return folded.join('\n');
}

const kInternalFrame = '<node-internal-frames>';
// Replace non-internal frame `at TracingChannel.traceSync (node:diagnostics_channel:328:14)`
// as well as `at node:internal/main/run_main_module:33:47` with `at <node-internal-frames>`.
// Also replaces error source line like:
//   node:internal/mod.js:44
//     throw err;
//     ^
function replaceInternalStackTrace(str) {
  const result = str.replaceAll(internalErrorSourceLines, `$1:<line>$3`)
    .replaceAll(internalStackFramesRegexp, `$1${kInternalFrame}$6`);
  return foldIdenticalLines(result, kInternalFrame);
}

// Replaces Windows line endings with posix line endings for unified snapshots
// across platforms.
function replaceWindowsLineEndings(str) {
  return str.replace(windowNewlineRegexp, '');
}

// Replaces all Windows path separators with posix separators for unified snapshots
// across platforms.
function replaceWindowsPaths(str) {
  if (!common.isWindows) {
    return str;
  }
  // Only replace `\` and `\\` with a leading letter, colon, or a `.`.
  // Avoid replacing escaping patterns like ` \#`, `\ `, or `\\`.
  return str.replaceAll(/(?<=(\w:|\.|\w+)(?:\S|\\ )*)\\\\?/g, '/');
}

// Removes line trailing white spaces.
function replaceTrailingSpaces(str) {
  return str.replaceAll(/[\t ]+\n/g, '\n');
}

// Replaces customized or platform specific executable names to be `<node-exe>`.
function generalizeExeName(str) {
  const baseName = path.basename(process.argv0 || 'node', '.exe');
  return str.replaceAll(`${baseName} --`, '<node-exe> --');
}

// Replaces the pids in warning messages with a placeholder.
function replaceWarningPid(str) {
  return str.replaceAll(/\(node:\d+\)/g, '(node:<pid>)');
}

// Replaces a path with a placeholder. The path can be a platform specific path
// or a file URL.
function transformPath(dirname, replacement) {
  // Handles output already processed by `replaceWindowsPaths`.
  const winPath = replaceWindowsPaths(dirname);
  // Handles URL encoded path in file URL strings as well.
  const urlEncoded = pathToFileURL(dirname).pathname;
  // On Windows, paths are case-insensitive, so we need to use case-insensitive
  // regex replacement to handle cases where the drive letter case differs.
  const flags = common.isWindows ? 'gi' : 'g';
  const urlEncodedRegex = new RegExp(RegExp.escape(urlEncoded), flags);
  const dirnameRegex = new RegExp(RegExp.escape(dirname), flags);
  const winPathRegex = new RegExp(RegExp.escape(winPath), flags);
  return (str) => {
    return str.replaceAll('\\\'', "'")
      // Replace fileUrl first as `winPath` could be a substring of the fileUrl.
      .replaceAll(urlEncodedRegex, replacement)
      .replaceAll(dirnameRegex, replacement)
      .replaceAll(winPathRegex, replacement);
  };
}

// Replaces path strings representing the nodejs/node repo full project root with
// `<project-root>`. Also replaces file URLs containing the full project root path.
// The project root path may contain unicode characters.
const kProjectRoot = '<project-root>';
function transformProjectRoot() {
  const projectRoot = path.resolve(__dirname, '../..');
  if (process.env.NODE_TEST_DIR) {
    const testDir = realpathSync(process.env.NODE_TEST_DIR);
    // On Jenkins CI, the test dir may be overridden by `NODE_TEST_DIR`.
    return transform(
      transformPath(projectRoot, kProjectRoot),
      transformPath(testDir, `${kProjectRoot}/test`),
      // TODO(legendecas): test-runner may print relative paths to the test relative to cwd.
      // It will be better if we could distinguish them from the project root.
      transformPath(path.relative(projectRoot, testDir), 'test'),
    );
  }
  return transformPath(projectRoot, kProjectRoot);
}

// Replaces tmpdirs created by `test/common/tmpdir.js`.
function transformTmpDir(str) {
  return str.replaceAll(/\/\.tmp\.\d+\//g, '/<tmpdir>/');
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
function replaceSpecDuration(str) {
  return str
    .replaceAll(/[0-9.]+ms/g, '*ms')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *');
}

function replaceJunitDuration(str) {
  return str
    .replaceAll(/time="[0-9.]+"/g, 'time="*"')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replaceAll(`hostname="${hostname()}"`, 'hostname="HOSTNAME"')
    .replaceAll(/file="[^"]*"/g, 'file="*"');
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

// Transforms basic patterns like:
// - platform specific path and line endings
// - line trailing spaces
// - executable specific path and versions
// - project root path and tmpdir
// - node internal stack frames
const basicTransform = transform(
  replaceWindowsLineEndings,
  replaceTrailingSpaces,
  replaceWindowsPaths,
  replaceNodeVersion,
  generalizeExeName,
  replaceWarningPid,
  transformProjectRoot(),
  transformTmpDir,
  replaceInternalStackTrace,
);

const defaultTransform = transform(
  basicTransform,
  replaceTestDuration,
);
const specTransform = transform(
  replaceSpecDuration,
  basicTransform,
);
const junitTransform = transform(
  replaceJunitDuration,
  basicTransform,
);
const lcovTransform = transform(
  basicTransform,
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
  replaceInternalStackTrace,
  replaceWindowsLineEndings,
  replaceWindowsPaths,
  spawnAndAssert,
  transform,
  transformProjectRoot,
  replaceTestDuration,
  basicTransform,
  defaultTransform,
  specTransform,
  junitTransform,
  lcovTransform,
  ensureCwdIsProjectRoot,
  canColorize,
};
