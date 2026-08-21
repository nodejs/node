import { mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { promisify } from 'node:util';

// Test common.mustNotMutateObjectDeep()

const original = {
  foo: { bar: 'baz' },
  qux: null,
  quux: [
    'quuz',
    { corge: 'grault' },
  ],
};

// Make a copy to make sure original doesn't get altered by the function itself.
const backup = structuredClone(original);

// Wrapper for convenience:
const obj = () => mustNotMutateObjectDeep(original);

function testOriginal(root) {
  assert.deepStrictEqual(root, backup);
  return root.foo.bar === 'baz' && root.quux[1].corge.length === 6;
}

function definePropertyOnRoot(root) {
  Object.defineProperty(root, 'xyzzy', {});
}

function definePropertyOnFoo(root) {
  Object.defineProperty(root.foo, 'xyzzy', {});
}

function deletePropertyOnRoot(root) {
  delete root.foo;
}

function deletePropertyOnFoo(root) {
  delete root.foo.bar;
}

function preventExtensionsOnRoot(root) {
  Object.preventExtensions(root);
}

function preventExtensionsOnFoo(root) {
  Object.preventExtensions(root.foo);
}

function preventExtensionsOnRootViaSeal(root) {
  Object.seal(root);
}

function preventExtensionsOnFooViaSeal(root) {
  Object.seal(root.foo);
}

function preventExtensionsOnRootViaFreeze(root) {
  Object.freeze(root);
}

function preventExtensionsOnFooViaFreeze(root) {
  Object.freeze(root.foo);
}

function setOnRoot(root) {
  root.xyzzy = 'gwak';
}

function setOnFoo(root) {
  root.foo.xyzzy = 'gwak';
}

function setQux(root) {
  root.qux = 'gwak';
}

function setQuux(root) {
  root.quux.push('gwak');
}

function setQuuxItem(root) {
  root.quux[0] = 'gwak';
}

function setQuuxProperty(root) {
  root.quux[1].corge = 'gwak';
}

function setPrototypeOfRoot(root) {
  Object.setPrototypeOf(root, Array);
}

function setPrototypeOfFoo(root) {
  Object.setPrototypeOf(root.foo, Array);
}

function setPrototypeOfQuux(root) {
  Object.setPrototypeOf(root.quux, Array);
}


{
  assert.ok(testOriginal(obj()));

  assert.throws(
    () => definePropertyOnRoot(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => definePropertyOnFoo(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => deletePropertyOnRoot(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => deletePropertyOnFoo(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnRoot(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnFoo(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnRootViaSeal(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnFooViaSeal(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnRootViaFreeze(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => preventExtensionsOnFooViaFreeze(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setOnRoot(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setOnFoo(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setQux(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setQuux(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setQuux(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setQuuxItem(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setQuuxProperty(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setPrototypeOfRoot(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setPrototypeOfFoo(obj()),
    { code: 'ERR_ASSERTION' }
  );
  assert.throws(
    () => setPrototypeOfQuux(obj()),
    { code: 'ERR_ASSERTION' }
  );

  // Test that no mutation happened:
  assert.ok(testOriginal(obj()));
}

// Test various supported types, directly and nested:
[
  undefined, null, false, true, 42, 42n, Symbol('42'), NaN, Infinity, {}, [],
  () => {}, async () => {}, Promise.resolve(), Math, { __proto__: null },
].forEach((target) => {
  assert.deepStrictEqual(mustNotMutateObjectDeep(target), target);
  assert.deepStrictEqual(mustNotMutateObjectDeep({ target }), { target });
  assert.deepStrictEqual(mustNotMutateObjectDeep([ target ]), [ target ]);
});

// Test that passed functions keep working correctly:
{
  const fn = () => 'blep';
  fn.foo = {};
  const fnImmutableView = mustNotMutateObjectDeep(fn);
  assert.deepStrictEqual(fnImmutableView, fn);

  // Test that the function still works:
  assert.strictEqual(fn(), 'blep');
  assert.strictEqual(fnImmutableView(), 'blep');

  // Test that the original function is not deeply frozen:
  fn.foo.bar = 'baz';
  assert.strictEqual(fn.foo.bar, 'baz');
  assert.strictEqual(fnImmutableView.foo.bar, 'baz');

  // Test the original function is not frozen:
  fn.qux = 'quux';
  assert.strictEqual(fn.qux, 'quux');
  assert.strictEqual(fnImmutableView.qux, 'quux');

  // Redefining util.promisify.custom also works:
  promisify(mustNotMutateObjectDeep(promisify(fn)));
}
