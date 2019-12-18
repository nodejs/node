import { randomBytes } from 'crypto';
import { parentPort, workerData, threadId } from 'worker_threads';

globalThis.globalValue = 42;

// Create a random value unique to this instance of the hooks. Useful to ensure
// that threads share the same loader.
const identity = randomBytes(16).toString('hex');

function valueResolution(value) {
  const src = Object.entries(value).map(
    ([exportName, exportValue]) =>
      `export const ${exportName} = ${JSON.stringify(exportValue)};`
  ).join('\n');
  return {
    url: `data:text/javascript;base64,${Buffer.from(src).toString('base64')}`,
    format: 'module',
  };
}

export async function resolve(specifier, referrer, parentResolve) {
  if (!specifier.startsWith('test!')) {
    return parentResolve(specifier, referrer);
  }

  switch (specifier.substr(5)) {
    case 'worker_threads':
      return valueResolution({ parentPort, workerData });

    case 'globalValue':
      return valueResolution({ globalValue: globalThis.globalValue });

    case 'identity':
        return valueResolution({ threadId, identity })
  }

  throw new Error('Invalid test case');
}
