import assert from 'assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export async function resolve(specifier, context, nextResolve) {
  if (mainLoad) {
    mainLoad = false;
    return null;
  }
  await assert.rejects(async () => {
    await nextResolve(specifier, context);
  }, {
    code: 'ERR_MODULE_NOT_FOUND',
  });
  return { url: 'nodejs:fs' };
}
