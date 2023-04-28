import { strictEqual } from "node:assert";
import { isMainThread, workerData, parentPort } from "node:worker_threads";

strictEqual(isMainThread, false);

// We want to make sure that internals are not leaked on the public module:
strictEqual(workerData, null);
strictEqual(parentPort, null);

// We don't want `import.meta.resolve` being available from loaders
// as the sync implementation is not compatible with calling async
// functions on the same thread.
strictEqual(typeof import.meta.resolve, 'undefined');
