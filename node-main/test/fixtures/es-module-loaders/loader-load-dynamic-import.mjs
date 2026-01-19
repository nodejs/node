import { writeSync } from 'node:fs';


export async function load(url, context, next) {
  if (url === 'node:fs' || url.includes('loader')) {
    return next(url);
  }

  // Here for asserting dynamic import
  await import('xxx/loader-load-passthru.mjs');

  writeSync(1, 'load dynamic import' + '\n'); // Signal that this specific hook ran
  return next(url, context);
}
