import { writeSync } from 'node:fs';


export async function resolve(specifier, context, next) {
  if (specifier === 'node:fs' || specifier.includes('loader')) {
    return next(specifier);
  }

  // Here for asserting dynamic import
  await import('xxx/loader-resolve-passthru.mjs');

  writeSync(1, 'resolve dynamic import' + '\n'); // Signal that this specific hook ran
  return next(specifier);
}
