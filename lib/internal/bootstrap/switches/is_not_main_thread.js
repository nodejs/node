'use strict';

const {
  ObjectDefineProperty,
} = primordials;

delete process._debugProcess;
delete process._debugEnd;
// Also drop the other main-thread-only helpers is_main_thread.js installs, so
// that this switch can be applied on top of a context bootstrapped for the
// main thread (as when a worker starts from the built-in snapshot).
delete process._debugPause;
delete process._startProfilerIdleNotifier;
delete process._stopProfilerIdleNotifier;

function defineStream(name, getter) {
  ObjectDefineProperty(process, name, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get: getter,
  });
}

defineStream('stdout', getStdout);
defineStream('stdin', getStdin);
defineStream('stderr', getStderr);

// Worker threads don't receive signals.
const {
  startListeningIfSignal,
  stopListeningIfSignal,
} = require('internal/process/signal');
process.removeListener('newListener', startListeningIfSignal);
process.removeListener('removeListener', stopListeningIfSignal);

// ---- keep the attachment of the wrappers above so that it's easier to ----
// ----              compare the setups side-by-side                    -----

const {
  createWorkerStdio,
  kStdioWantsMoreDataCallback,
} = require('internal/worker/io');

let workerStdio;
function lazyWorkerStdio() {
  if (workerStdio === undefined) {
    workerStdio = createWorkerStdio();
    process.on('exit', flushSync);
  }

  return workerStdio;
}

function flushSync() {
  workerStdio.stdout[kStdioWantsMoreDataCallback]();
  workerStdio.stderr[kStdioWantsMoreDataCallback]();
}

function getStdout() { return lazyWorkerStdio().stdout; }

function getStderr() { return lazyWorkerStdio().stderr; }

function getStdin() { return lazyWorkerStdio().stdin; }
