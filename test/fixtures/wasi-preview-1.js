'use strict';

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const { parseArgs } = require('util');
const common = require('../common');
const { WASI } = require('wasi');
const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');

const args = parseArgs({
  allowPositionals: true,
  options: {
    target: {
      type: 'string',
      default: 'wasm32-wasip1',
    },
  },
  strict: false,
});

function returnOnExitEnvToValue(env) {
  const envValue = env.RETURN_ON_EXIT;
  if (envValue === undefined) {
    return undefined;
  }

  return envValue === 'true';
}

common.expectWarning('ExperimentalWarning',
                      'WASI is an experimental feature and might change at any time');

tmpdir.refresh();
const wasmDir = path.join(__dirname, '..', 'wasi', 'wasm');
const wasiPreview1 = new WASI({
  version: 'preview1',
  args: ['foo', '-bar', '--baz=value'],
  env: process.env,
  preopens: {
    '/sandbox': fixtures.path('wasi'),
    '/tmp': tmpdir.path,
  },
  returnOnExit: returnOnExitEnvToValue(process.env),
});

// Validate the getImportObject helper
assert.strictEqual(wasiPreview1.wasiImport,
                    wasiPreview1.getImportObject().wasi_snapshot_preview1);

(async () => {
  const importObject = { ...wasiPreview1.getImportObject() };
  if (args.values.target === 'wasm32-wasip1-threads') {
    let nextTid = 43;
    const workers = [];
    const terminateAllThreads = () => {
      workers.forEach((w) => w.terminate());
    };
    const proc_exit = importObject.wasi_snapshot_preview1.proc_exit;
    importObject.wasi_snapshot_preview1.proc_exit = function(code) {
      terminateAllThreads();
      return proc_exit.call(this, code);
    };
    const spawn = (startArg, threadId) => {
      const tid = nextTid++;
      const name = `pthread-${tid}`;
      const sab = new SharedArrayBuffer(8 + 8192);
      const result = new Int32Array(sab);

      const workerData = {
        name,
        startArg,
        tid,
        wasmModule,
        memory: importObject.env.memory,
        result,
      };

      const worker = new Worker(__filename, {
        name,
        argv: process.argv.slice(2),
        execArgv: [
          '--experimental-wasi-unstable-preview1',
        ],
        workerData,
      });
      workers[tid] = worker;

      worker.on('message', ({ cmd, startArg, threadId, tid }) => {
        if (cmd === 'loaded') {
          worker.unref();
        } else if (cmd === 'thread-spawn') {
          spawn(startArg, threadId);
        } else if (cmd === 'cleanup-thread') {
          workers[tid].terminate();
          delete workers[tid];
        }
      });

      worker.on('error', (e) => {
        terminateAllThreads();
        throw new Error(e);
      });

      const r = Atomics.wait(result, 0, 0, 1000);
      if (r === 'timed-out') {
        workers[tid].terminate();
        delete workers[tid];
        if (threadId) {
          Atomics.store(threadId, 0, -6);
          Atomics.notify(threadId, 0);
        }
        return -6;
      }
      if (Atomics.load(result, 0) !== 0) {
        const decoder = new TextDecoder();
        const nameLength = Atomics.load(result, 1);
        const messageLength = Atomics.load(result, 2);
        const stackLength = Atomics.load(result, 3);
        const name = decoder.decode(sab.slice(16, 16 + nameLength));
        const message = decoder.decode(sab.slice(16 + nameLength, 16 + nameLength + messageLength));
        const stack = decoder.decode(
          sab.slice(16 + nameLength + messageLength,
            16 + nameLength + messageLength + stackLength));
        const ErrorConstructor = globalThis[name] ?? (
          name === 'RuntimeError' ? (WebAssembly.RuntimeError ?? Error) : Error);
        const error = new ErrorConstructor(message);
        Object.defineProperty(error, 'stack', {
          value: stack,
          writable: true,
          enumerable: false,
          configurable: true,
        });
        Object.defineProperty(error, 'name', {
          value: name,
          writable: true,
          enumerable: false,
          configurable: true,
        });
        throw error;
      }
      if (threadId) {
        Atomics.store(threadId, 0, tid);
        Atomics.notify(threadId, 0);
      }
      return tid;
    };
    const memory = isMainThread ? new WebAssembly.Memory({
      initial: 16777216 / 65536,
      maximum: 2147483648 / 65536,
      shared: true,
    }) : workerData.memory;
    importObject.env ??= {};
    importObject.env.memory = memory;
    importObject.wasi = {
      'thread-spawn': (startArg) => {
        if (isMainThread) {
          return spawn(startArg);
        }
        const threadIdBuffer = new SharedArrayBuffer(4);
        const id = new Int32Array(threadIdBuffer);
        Atomics.store(id, 0, -1);
        parentPort.postMessage({
          cmd: 'thread-spawn',
          startArg,
          threadId: id,
        });
        Atomics.wait(id, 0, -1);
        const tid = Atomics.load(id, 0);
        return tid;
      },
    };
  }
  let wasmModule;
  let instancePreview1;
  try {
    if (isMainThread) {
      const modulePathPreview1 = path.join(wasmDir, `${args.positionals[0]}.wasm`);
      const bufferPreview1 = fs.readFileSync(modulePathPreview1);
      wasmModule = await WebAssembly.compile(bufferPreview1);
    } else {
      wasmModule = workerData.wasmModule;
    }
    instancePreview1 = await WebAssembly.instantiate(wasmModule, importObject);

    if (isMainThread) {
      wasiPreview1.start(instancePreview1);
    } else {
      wasiPreview1.finalizeBindings(instancePreview1, {
        memory: importObject.env.memory,
      });
      parentPort.postMessage({ cmd: 'loaded' });
      Atomics.store(workerData.result, 0, 0);
      Atomics.notify(workerData.result, 0);
    }
  } catch (e) {
    if (!isMainThread) {
      const encoder = new TextEncoder();
      const nameBuffer = encoder.encode(e.name);
      const messageBuffer = encoder.encode(e.message);
      const stackBuffer = encoder.encode(e.stack);
      Atomics.store(workerData.result, 0, 1);
      Atomics.store(workerData.result, 1, nameBuffer.length);
      Atomics.store(workerData.result, 2, messageBuffer.length);
      Atomics.store(workerData.result, 3, stackBuffer.length);
      const u8arr = new Uint8Array(workerData.result.buffer);
      u8arr.set(nameBuffer, 16);
      u8arr.set(messageBuffer, 16 + nameBuffer.length);
      u8arr.set(stackBuffer, 16 + nameBuffer.length + messageBuffer.length);
      Atomics.notify(workerData.result, 0);
    }
    throw e;
  }
  if (!isMainThread) {
    instancePreview1.exports.wasi_thread_start(workerData.tid, workerData.startArg);
    parentPort.postMessage({ cmd: 'cleanup-thread', tid: workerData.tid });
  }
})().then(common.mustCall());
