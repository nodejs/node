'use strict';
const {
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const {
  triggerUncaughtException,
  exitCodes: { kNoFailure },
} = internalBinding('errors');
const { getOptionValue } = require('internal/options');
const { emitExperimentalWarning } = require('internal/util');
const { FilesWatcher } = require('internal/watch_mode/files_watcher');
const { green, blue, red, white, clear } = require('internal/util/colors');

const { spawn } = require('child_process');
const { inspect } = require('util');
const { setTimeout, clearTimeout } = require('timers');
const { resolve } = require('path');
const { once, on } = require('events');

prepareMainThreadExecution(false, false);
markBootstrapComplete();

// TODO(MoLow): Make kill signal configurable
const kKillSignal = 'SIGTERM';
const kShouldFilterModules = getOptionValue('--watch-path').length === 0;
const kWatchedPaths = ArrayPrototypeMap(getOptionValue('--watch-path'), (path) => resolve(path));
const kPreserveOutput = getOptionValue('--watch-preserve-output');
const kCommand = ArrayPrototypeSlice(process.argv, 1);
const kCommandStr = inspect(ArrayPrototypeJoin(kCommand, ' '));
const args = ArrayPrototypeFilter(process.execArgv, (arg, i, arr) =>
  !StringPrototypeStartsWith(arg, '--watch-path') &&
  (!arr[i - 1] || !StringPrototypeStartsWith(arr[i - 1], '--watch-path')) &&
  arg !== '--watch' && !StringPrototypeStartsWith(arg, '--watch=') && arg !== '--watch-preserve-output');
ArrayPrototypePushApply(args, kCommand);

const watcher = new FilesWatcher({ debounce: 200, mode: kShouldFilterModules ? 'filter' : 'all' });
ArrayPrototypeForEach(kWatchedPaths, (p) => watcher.watchPath(p));

let graceTimer;
let child;
let exited;

function start() {
  exited = false;
  const stdio = kShouldFilterModules ? ['inherit', 'inherit', 'inherit', 'ipc'] : 'inherit';
  child = spawn(process.execPath, args, { stdio, env: { ...process.env, WATCH_REPORT_DEPENDENCIES: '1' } });
  watcher.watchChildProcessModules(child);
  child.once('exit', (code) => {
    exited = true;
    if (code === 0) {
      process.stdout.write(`${blue}Completed running ${kCommandStr}${white}\n`);
    } else {
      process.stdout.write(`${red}Failed running ${kCommandStr}${white}\n`);
    }
  });
}

async function killAndWait(signal = kKillSignal, force = false) {
  child?.removeAllListeners();
  if (!child) {
    return;
  }
  if ((child.killed || exited) && !force) {
    return;
  }
  const onExit = once(child, 'exit');
  child.kill(signal);
  const { 0: exitCode } = await onExit;
  return exitCode;
}

function reportGracefulTermination() {
  // Log if process takes more than 500ms to stop.
  let reported = false;
  clearTimeout(graceTimer);
  graceTimer = setTimeout(() => {
    reported = true;
    process.stdout.write(`${blue}Waiting for graceful termination...${white}\n`);
  }, 500).unref();
  return () => {
    clearTimeout(graceTimer);
    if (reported) {
      process.stdout.write(`${clear}${green}Gracefully restarted ${kCommandStr}${white}\n`);
    }
  };
}

async function stop() {
  watcher.clearFileFilters();
  const clearGraceReport = reportGracefulTermination();
  await killAndWait();
  clearGraceReport();
}

async function restart() {
  if (!kPreserveOutput) process.stdout.write(clear);
  process.stdout.write(`${green}Restarting ${kCommandStr}${white}\n`);
  await stop();
  start();
}

(async () => {
  emitExperimentalWarning('Watch mode');

  try {
    start();

    // eslint-disable-next-line no-unused-vars
    for await (const _ of on(watcher, 'changed')) {
      await restart();
    }
  } catch (error) {
    triggerUncaughtException(error, true /* fromPromise */);
  }
})();

// Exiting gracefully to avoid stdout/stderr getting written after
// parent process is killed.
// this is fairly safe since user code cannot run in this process
function signalHandler(signal) {
  return async () => {
    watcher.clear();
    const exitCode = await killAndWait(signal, true);
    process.exit(exitCode ?? kNoFailure);
  };
}
process.on('SIGTERM', signalHandler('SIGTERM'));
process.on('SIGINT', signalHandler('SIGINT'));
