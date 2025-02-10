import assert from 'node:assert';

// A loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course, and the built-in ones needed for run-worker).
let mainLoad = true;
export async function resolve(specifier, { importAttributes }, next) {
  if (mainLoad || specifier === 'path' || specifier === 'worker_threads') {
    mainLoad = false;
    return next(specifier);
  }
  await assert.rejects(next(specifier), { code: 'ERR_MODULE_NOT_FOUND' });
  return {
    url: 'node:fs',
    importAttributes,
  };
}
