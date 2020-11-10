import assert from 'assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export async function resolve(specifier, { parentURL }, defaultResolve) {
  if (mainLoad) {
    mainLoad = false;
    return defaultResolve(specifier, {parentURL}, defaultResolve);
  }
  try {
    await defaultResolve(specifier, {parentURL}, defaultResolve);
  }
  catch (e) {
    assert.strictEqual(e.code, 'ERR_MODULE_NOT_FOUND');
    return {
      url: 'nodejs:fs'
    };
  }
  assert.fail(`Module resolution for ${specifier} should be throw ERR_MODULE_NOT_FOUND`);
}
