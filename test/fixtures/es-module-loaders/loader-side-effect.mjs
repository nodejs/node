// Arrow function so it closes over the this-value of the preload scope.
const globalPreloadSrc = () => {
  /* global getBuiltin */
  const assert = getBuiltin('assert');
  const vm = getBuiltin('vm');

  assert.strictEqual(typeof require, 'undefined');
  assert.strictEqual(typeof module, 'undefined');
  assert.strictEqual(typeof exports, 'undefined');
  assert.strictEqual(typeof __filename, 'undefined');
  assert.strictEqual(typeof __dirname, 'undefined');

  assert.strictEqual(this, globalThis);
  (globalThis.preloadOrder || (globalThis.preloadOrder = [])).push('loader');

  vm.runInThisContext(`\
var implicitGlobalProperty = 42;
const implicitGlobalConst = 42 * 42;
`);

  assert.strictEqual(globalThis.implicitGlobalProperty, 42);
  (implicitGlobalProperty).foo = 'bar'; // assert: not strict mode

  globalThis.explicitGlobalProperty = 42 * 42 * 42;
}

export function globalPreload() {
  return `\
<!-- assert: inside of script goal -->
(${globalPreloadSrc.toString()})();
`;
}
