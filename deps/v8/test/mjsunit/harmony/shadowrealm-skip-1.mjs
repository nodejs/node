export const foo = 'bar';
export const obj = {};
export const func = function () {
  return globalThis.foobar;
};
// Generates side-effects to the globalThis.
globalThis.foobar = 'inner-scope';
