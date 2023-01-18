let importedESM = 0;
let importedCJS = 0;

export function globalPreload({ port }) {
  port.on('message', (int32) => {
    port.postMessage({ importedESM, importedCJS });
    Atomics.store(int32, 0, 1);
    Atomics.notify(int32, 0);
  });
  port.unref();
  return `
    const { receiveMessageOnPort } = getBuiltin('worker_threads');
    global.getModuleTypeStats = async function getModuleTypeStats() {
      const sab = new SharedArrayBuffer(4);
      const int32 = new Int32Array(sab);
      port.postMessage(int32);
      // Artificial timeout to keep the event loop alive.
      // https://bugs.chromium.org/p/v8/issues/detail?id=13238
      // TODO(targos) Remove when V8 issue is resolved.
      const timeout = setTimeout(() => { throw new Error('timeout'); }, 1_000);
      await Atomics.waitAsync(int32, 0, 0).value;
      clearTimeout(timeout);
      return receiveMessageOnPort(port).message;
    };
  `;
}

export async function load(url, context, next) {
  return next(url);
}

export async function resolve(specifier, context, next) {
  const nextResult = await next(specifier, context);
  const { format } = nextResult;

  if (format === 'module' || specifier.endsWith('.mjs')) {
    importedESM++;
  } else if (format == null || format === 'commonjs') {
    importedCJS++;
  }

  return nextResult;
}

