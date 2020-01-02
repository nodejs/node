import assert from 'assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export async function resolve ({ specifier, parentURL }, defaultResolve, loader) {
  if (mainLoad) {
    mainLoad = false;
    return defaultResolve({specifier, parentURL}, defaultResolve, loader);
  }
  try {
    await defaultResolve({specifier, parentURL}, defaultResolve, loader);
  }
  catch (e) {
    assert.strictEqual(e.code, 'ERR_MODULE_NOT_FOUND');
    return {
      format: 'builtin',
      url: 'fs'
    };
  }
  assert.fail(`Module resolution for ${specifier} should be throw ERR_MODULE_NOT_FOUND`);
}
