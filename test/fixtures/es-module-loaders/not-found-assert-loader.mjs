import assert from 'assert';

// a loader that asserts that the defaultResolve will throw "not found"
// (skipping the top-level main of course)
let mainLoad = true;
export function resolve(specifier, { parentURL, importAssertions }, nextResolve) {
  if (mainLoad) {
    mainLoad = false;
    return nextResolve(specifier, {parentURL, importAssertions});
  }
  try {
    nextResolve(specifier, {parentURL, importAssertions});
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
