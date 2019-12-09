'use strict';

const { ObjectDefineProperty } = primordials;
const rawMethods = internalBinding('process_methods');
const {
  unavailable
} = require('internal/process/worker_thread_only');

process.abort = unavailable('process.abort()');
process.chdir = unavailable('process.chdir()');
process.umask = wrappedUmask;
process.cwd = rawMethods.cwd;

delete process._debugProcess;
delete process._debugEnd;
delete process._startProfilerIdleNotifier;
delete process._stopProfilerIdleNotifier;

function defineStream(name, getter) {
  ObjectDefineProperty(process, name, {
    configurable: true,
    enumerable: true,
    get: getter
  });
}

defineStream('stdout', getStdout);
defineStream('stdin', getStdin);
defineStream('stderr', getStderr);

// Worker threads don't receive signals.
const {
  startListeningIfSignal,
  stopListeningIfSignal
} = require('internal/process/signal');
process.removeListener('newListener', startListeningIfSignal);
process.removeListener('removeListener', stopListeningIfSignal);

// ---- keep the attachment of the wrappers above so that it's easier to ----
// ----              compare the setups side-by-side                    -----

const {
  createWorkerStdio
} = require('internal/worker/io');
const {
  codes: { ERR_WORKER_UNSUPPORTED_OPERATION }
} = require('internal/errors');

let workerStdio;
function lazyWorkerStdio() {
  if (!workerStdio) workerStdio = createWorkerStdio();
  return workerStdio;
}

function getStdout() { return lazyWorkerStdio().stdout; }

function getStderr() { return lazyWorkerStdio().stderr; }

function getStdin() { return lazyWorkerStdio().stdin; }

function wrappedUmask(mask) {
  // process.umask() is a read-only operation in workers.
  if (mask !== undefined) {
    throw new ERR_WORKER_UNSUPPORTED_OPERATION('Setting process.umask()');
  }

  return rawMethods.umask(mask);
}
