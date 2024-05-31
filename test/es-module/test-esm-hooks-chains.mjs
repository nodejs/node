import { spawnPromisified } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { Worker } from 'node:worker_threads';
import { execPath } from 'node:process';
import { fileURLToPath } from 'node:url';
import { ok, strictEqual, deepStrictEqual } from 'node:assert';
import { isDeepStrictEqual } from 'node:util';

async function spawn() {
  new Worker(fileURL('es-modules/test-esm-separate-chains.mjs'), { workerData: { count: 1 } });
  await import(fileURL('es-modules/test-esm-separate-chains-execute.mjs'));
  delete globalThis.__testESM;
}

async function test() {
  const loader = fileURLToPath(fileURL('es-module-loaders/hooks-log-verbose.mjs'));
  const { code, stdout, stderr } = await spawnPromisified(execPath, [
    '--expose-internals',
    '--import', 'data:text/javascript,globalThis.__testESM=true',
    // Note that the same loader is imported twice to verify duplication detection
    '--import', loader,
    '--import', loader,
    fileURLToPath(import.meta.url),
  ]);

  strictEqual(code, 0);

  const executeFileUrl = fileURL('/es-modules/test-esm-separate-chains-execute.mjs').toString();

  // Parse NDJSON output
  const linesOut = stdout.trim().split('\n').map((line) => JSON.parse(line)).reduce((accu, line) => {
    accu[line.threadId] ??= [];
    accu[line.threadId].push(line);
    return accu;
  }, {});
  const linesErr = stderr.trim().split('\n').map((line) => JSON.parse(line));

  // Verify threads behavior - Note that grouping is only inserted for readability

  // Verify the main thread
  {
    // A hook is only registered once
    strictEqual(linesOut[0].filter((line) => line.operation === 'initialize').length, 1);
    // There is only a chain for the main thread
    ok(linesOut[0].every((line) => !line.chain || line.chain === '0:1'));
    // The hook is called for resolve
    strictEqual(linesOut[0].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl;
    }).length, 1);
  }

  // Verify the first worker thread - It just inherits the hooks from the main thread
  {
    // Note that the threadId is 2 as the thread 1 is the hooks thread
    // The hook is copied from the parent thread so it's not reinitialized
    strictEqual(linesOut[2].filter((line) => line.operation === 'initialize').length, 0);
    // 6 resolves
    //  1 (data --import)
    //  1 (file --import)
    //  4 worker entrypoint imports
    // 1 execute
    strictEqual(linesOut[2].filter((line) => line.threadId === 2).length, 7);

    // The hook is called for resolve
    strictEqual(linesOut[0].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl;
    }).length, 1);
  }

  // Verify the second worker thread - Re-register the same hook but with data attached, so no duplicate
  {
    // The hook is copied from the parent thread but it is also reinitialized as there is new data
    strictEqual(linesOut[3].filter((line) => line.operation === 'initialize').length, 1);
    // The initialize hook has data only defined once
    strictEqual(linesOut[3].filter((line) => line.operation === 'initialize' && line.data).length, 1);

    // 1 initialize
    // 6 resolves
    //  1 (data --import)
    //  1 (file --import)
    //  4 worker entrypoint imports
    //    1 import has two hooks
    //    Note that the last worker is not resolve/load again since it is the current entrypoint
    // 1 execute
    strictEqual(linesOut[3].filter((line) => line.threadId === 3).length, 9);

    // The hook is called for resolve once per registration
    strictEqual(linesOut[3].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl && !line.data;
    }).length, 1);
    strictEqual(linesOut[3].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl &&
        isDeepStrictEqual(line.data, { label: 'first' });
    }).length, 1);
  }

  // Verify the third worker thread - Re-register the same hook but with data attached, so no duplicate
  {
    // The hook is copied from the parent thread but it is also reinitialized as there is new data
    strictEqual(linesOut[4].filter((line) => line.operation === 'initialize').length, 1);
    // The initialize hook has data only defined once
    strictEqual(linesOut[4].filter((line) => line.operation === 'initialize' && line.data).length, 1);

    // 1 initialize
    // 13 resolves
    //  2 (data --import, with and without data)
    //  2 (file --import, with and without data)
    //  9 (4 worker entrypoint imports, with and without data)
    //    1 import has three hooks
    //    Note that the last worker is not resolve/load again since it is the current entrypoint
    // 1 execute
    strictEqual(linesOut[4].filter((line) => line.threadId === 4).length, 15);

    // The hook is called for resolve once per registration
    strictEqual(linesOut[4].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl && !line.data;
    }).length, 1);
    strictEqual(linesOut[4].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl &&
        isDeepStrictEqual(line.data, { label: 'first' });
    }).length, 1);
    strictEqual(linesOut[4].filter((line) => {
      return line.operation === 'resolve' && line.specifier === executeFileUrl &&
        isDeepStrictEqual(line.data, { label: 'second' });
    }).length, 1);
  }

  // Make sure at the the chains are cleared.
  // The only one remaining are the chain for the thread hooks (1)
  // and the default chain (whose internal is serialized to null by JSON.stringify)
  deepStrictEqual(linesErr.at(-1), { operation: 'chains', names: [ 1, null ] });
}

const runner = globalThis.__testESM ? spawn : test;
await runner();
