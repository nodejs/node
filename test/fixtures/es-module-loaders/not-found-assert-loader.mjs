import assert from 'node:assert';

// A loader that asserts that the defaultResolve will throw "not found"
export async function resolve(specifier, { importAttributes }, next) {
  if (specifier.startsWith('./not-found')) {
    await assert.rejects(next(specifier), { code: 'ERR_MODULE_NOT_FOUND' });
    return {
      url: 'node:fs',
      importAttributes,
    };
  }
  return next(specifier);
}
