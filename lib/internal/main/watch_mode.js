'use strict';
const {
  ArrayPrototypeFlatMap,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
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
const { FilesWatcher } = require('internal/watch_mode/files_watcher');
const { green, blue, red, white, clear } = require('internal/util/colors');
const { convertToValidSignal } = require('internal/util');

const { spawn } = require('child_process');
const { inspect } = require('util');
const { setTimeout, clearTimeout } = require('timers');
const { resolve } = require('path');
const { once } = require('events');
const { createMatcher } = require('internal/fs/glob');
const { globSync } = require('fs');

prepareMainThreadExecution(false, false);
markBootstrapComplete();

function hasGlobPattern(path) {
  return createMatcher(path).hasMagic();
}

function handleWatchedPath(path) {
  if (hasGlobPattern(path)) {
    const matchedFilesFromGlob = globSync(path);
    const resolvedMatchedFiles = ArrayPrototypeMap(matchedFilesFromGlob, (path) => resolve(path));
    return resolvedMatchedFiles;
  }
  return resolve(path);
}

const kKillSignal = convertToValidSignal(getOptionValue('--watch-kill-signal'));
const kShouldFilterModules = getOptionValue('--watch-pattern').length === 0;
const kEnvFile = getOptionValue('--env-file') || getOptionValue('--env-file-if-exists');
const kWatchedPaths = ArrayPrototypeFlatMap(getOptionValue('--watch-path'), (path) => handleWatchedPath(path));
const kPreserveOutput = getOptionValue('--watch-preserve-output');
const kCommand = ArrayPrototypeSlice(process.argv, 1);
const kCommandStr = inspect(ArrayPrototypeJoin(kCommand, ' '));

const argsWithoutWatchOptions = [];

for (let i = 0; i < process.execArgv.length; i++) {
  const arg = process.execArgv[i];
  if (StringPrototypeStartsWith(arg, '--watch=')) {
    continue;
  }
  if (arg === '--watch') {
    const nextArg = process.execArgv[i + 1];
    if (nextArg && nextArg[0] !== '-') {
      // If `--watch` doesn't include `=` and the next
      // argument is not a flag then it is interpreted as
      // the watch argument, so we need to skip that as well
      i++;
    }
    continue;
  }
  if (StringPrototypeStartsWith(arg, '--watch-path')) {
    const lengthOfWatchPathStr = 12;
    if (arg[lengthOfWatchPathStr] !== '=') {
      // if --watch-path doesn't include `=` it means
      // that the next arg is the target path, so we
      // need to skip that as well
      i++;
    }
    continue;
  }
  ArrayPrototypePush(argsWithoutWatchOptions, arg);
}

ArrayPrototypePushApply(argsWithoutWatchOptions, kCommand);

const watcher = new FilesWatcher({ debounce: 200, mode: kShouldFilterModules ? 'filter' : 'all' });
ArrayPrototypeForEach(kWatchedPaths, (p) => watcher.watchPath(p));

let graceTimer;
let child;
let exited;

function start() {
  exited = false;
  const stdio = kShouldFilterModules ? ['inherit', 'inherit', 'inherit', 'ipc'] : 'inherit';
  child = spawn(process.execPath, argsWithoutWatchOptions, {
    stdio,
    env: {
      ...process.env,
      WATCH_REPORT_DEPENDENCIES: '1',
    },
  });
  watcher.watchChildProcessModules(child);
  if (kEnvFile) {
    watcher.filterFile(resolve(kEnvFile));
  }
  child.once('exit', (code) => {
    exited = true;
    const waitingForChanges = 'Waiting for file changes before restarting...';
    if (code === 0) {
      process.stdout.write(`${blue}Completed running ${kCommandStr}. ${waitingForChanges}${white}\n`);
    } else {
      process.stdout.write(`${red}Failed running ${kCommandStr}. ${waitingForChanges}${white}\n`);
    }
  });
  return child;
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
      if (!kPreserveOutput) {
        process.stdout.write(clear);
      }
      process.stdout.write(`${green}Gracefully restarted ${kCommandStr}${white}\n`);
    }
  };
}

async function stop(child) {
  // Without this line, the child process is still able to receive IPC, but is unable to send additional messages
  watcher.destroyIPC(child);
  watcher.clearFileFilters();
  const clearGraceReport = reportGracefulTermination();
  await killAndWait();
  clearGraceReport();
}

let restarting = false;
async function restart(child) {
  if (restarting) return;
  restarting = true;
  try {
    if (!kPreserveOutput) process.stdout.write(clear);
    process.stdout.write(`${green}Restarting ${kCommandStr}${white}\n`);
    await stop(child);
    return start();
  } finally {
    restarting = false;
  }
}

async function init() {
  let child = start();
  const restartChild = async () => {
    child = await restart(child);
  };
  watcher
    .on('changed', restartChild)
    .on('error', (error) => {
      watcher.off('changed', restartChild);
      triggerUncaughtException(error, true /* fromPromise */);
    });
}

init();

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
