import { strictEqual } from "node:assert";
import { isMainThread, workerData, parentPort } from "node:worker_threads";

// TODO(aduh95): switch this to `false` when loader hooks are run on a separate thread.
strictEqual(isMainThread, true);

// We want to make sure that internals are not leaked on the public module:
strictEqual(workerData, null);
strictEqual(parentPort, null);

// TODO(aduh95): switch to `"undefined"` when loader hooks are run on a separate thread.
// We don't want `import.meta.resolve` being available from loaders
// as the sync implementation is not compatible with calling async
// functions on the same thread.
strictEqual(typeof import.meta.resolve, 'function');
