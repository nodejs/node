import assert from 'assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export async function resolve (specifier, base, defaultResolve) {
  if (mainLoad) {
    mainLoad = false;
    return defaultResolve(specifier, base);
  }
  try {
    await defaultResolve(specifier, base);
  }
  catch (e) {
    assert.strictEqual(e.code, 'MODULE_NOT_FOUND');
    return {
      format: 'builtin',
      url: 'fs'
    };
  }
  assert.fail(`Module resolution for ${specifier} should be throw MODULE_NOT_FOUND`);
}
