import assert from 'node:assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export async function resolve(specifier, { importAssertions }, next) {
  if (mainLoad) {
    mainLoad = false;
    return next(specifier);
  }
  try {
    await next(specifier);
  }
  catch (e) {
    assert.strictEqual(e.code, 'ERR_MODULE_NOT_FOUND');
    return {
      url: 'node:fs',
      importAssertions,
    };
  }
  assert.fail(`Module resolution for ${specifier} should be throw ERR_MODULE_NOT_FOUND`);
}
