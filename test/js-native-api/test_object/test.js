'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for objects
const test_object = require(`./build/${common.buildType}/test_object`);


const object = {
  hello: 'world',
  array: [
    1, 94, 'str', 12.321, { test: 'obj in arr' }
  ],
  newObject: {
    test: 'obj in obj'
  }
};

assert.strictEqual(test_object.Get(object, 'hello'), 'world');
assert.strictEqual(test_object.GetNamed(object, 'hello'), 'world');
assert.deepStrictEqual(test_object.Get(object, 'array'),
                       [ 1, 94, 'str', 12.321, { test: 'obj in arr' } ]);
assert.deepStrictEqual(test_object.Get(object, 'newObject'),
                       { test: 'obj in obj' });

assert(test_object.Has(object, 'hello'));
assert(test_object.HasNamed(object, 'hello'));
assert(test_object.Has(object, 'array'));
assert(test_object.Has(object, 'newObject'));

const newObject = test_object.New();
assert(test_object.Has(newObject, 'test_number'));
assert.strictEqual(newObject.test_number, 987654321);
assert.strictEqual(newObject.test_string, 'test string');

{
  // Verify that napi_get_property() walks the prototype chain.
  function MyObject() {
    this.foo = 42;
    this.bar = 43;
  }

  MyObject.prototype.bar = 44;
  MyObject.prototype.baz = 45;

  const obj = new MyObject();

  assert.strictEqual(test_object.Get(obj, 'foo'), 42);
  assert.strictEqual(test_object.Get(obj, 'bar'), 43);
  assert.strictEqual(test_object.Get(obj, 'baz'), 45);
  assert.strictEqual(test_object.Get(obj, 'toString'),
                     Object.prototype.toString);
}

{
  // Verify that napi_has_own_property() fails if property is not a name.
  [true, false, null, undefined, {}, [], 0, 1, () => {}].forEach((value) => {
    assert.throws(() => {
      test_object.HasOwn({}, value);
    }, /^Error: A string or symbol was expected$/);
  });
}

{
  // Verify that napi_has_own_property() does not walk the prototype chain.
  const symbol1 = Symbol();
  const symbol2 = Symbol();

  function MyObject() {
    this.foo = 42;
    this.bar = 43;
    this[symbol1] = 44;
  }

  MyObject.prototype.bar = 45;
  MyObject.prototype.baz = 46;
  MyObject.prototype[symbol2] = 47;

  const obj = new MyObject();

  assert.strictEqual(test_object.HasOwn(obj, 'foo'), true);
  assert.strictEqual(test_object.HasOwn(obj, 'bar'), true);
  assert.strictEqual(test_object.HasOwn(obj, symbol1), true);
  assert.strictEqual(test_object.HasOwn(obj, 'baz'), false);
  assert.strictEqual(test_object.HasOwn(obj, 'toString'), false);
  assert.strictEqual(test_object.HasOwn(obj, symbol2), false);
}

{
  // test_object.Inflate increases all properties by 1
  const cube = {
    x: 10,
    y: 10,
    z: 10
  };

  assert.deepStrictEqual(test_object.Inflate(cube), { x: 11, y: 11, z: 11 });
  assert.deepStrictEqual(test_object.Inflate(cube), { x: 12, y: 12, z: 12 });
  assert.deepStrictEqual(test_object.Inflate(cube), { x: 13, y: 13, z: 13 });
  cube.t = 13;
  assert.deepStrictEqual(
    test_object.Inflate(cube), { x: 14, y: 14, z: 14, t: 14 });

  const sym1 = Symbol('1');
  const sym2 = Symbol('2');
  const sym3 = Symbol('3');
  const sym4 = Symbol('4');
  const object2 = {
    [sym1]: '@@iterator',
    [sym2]: sym3
  };

  assert(test_object.Has(object2, sym1));
  assert(test_object.Has(object2, sym2));
  assert.strictEqual(test_object.Get(object2, sym1), '@@iterator');
  assert.strictEqual(test_object.Get(object2, sym2), sym3);
  assert(test_object.Set(object2, 'string', 'value'));
  assert(test_object.SetNamed(object2, 'named_string', 'value'));
  assert(test_object.Set(object2, sym4, 123));
  assert(test_object.Has(object2, 'string'));
  assert(test_object.HasNamed(object2, 'named_string'));
  assert(test_object.Has(object2, sym4));
  assert.strictEqual(test_object.Get(object2, 'string'), 'value');
  assert.strictEqual(test_object.Get(object2, sym4), 123);
}

{
  // Wrap a pointer in a JS object, then verify the pointer can be unwrapped.
  const wrapper = {};
  test_object.Wrap(wrapper);

  assert(test_object.Unwrap(wrapper));
}

{
  // Verify that wrapping doesn't break an object's prototype chain.
  const wrapper = {};
  const protoA = { protoA: true };
  Object.setPrototypeOf(wrapper, protoA);
  test_object.Wrap(wrapper);

  assert(test_object.Unwrap(wrapper));
  assert(wrapper.protoA);
}

{
  // Verify the pointer can be unwrapped after inserting in the prototype chain.
  const wrapper = {};
  const protoA = { protoA: true };
  Object.setPrototypeOf(wrapper, protoA);
  test_object.Wrap(wrapper);

  const protoB = { protoB: true };
  Object.setPrototypeOf(protoB, Object.getPrototypeOf(wrapper));
  Object.setPrototypeOf(wrapper, protoB);

  assert(test_object.Unwrap(wrapper));
  assert(wrapper.protoA, true);
  assert(wrapper.protoB, true);
}

{
  // Verify that normal and nonexistent properties can be deleted.
  const sym = Symbol();
  const obj = { foo: 'bar', [sym]: 'baz' };

  assert.strictEqual('foo' in obj, true);
  assert.strictEqual(sym in obj, true);
  assert.strictEqual('does_not_exist' in obj, false);
  assert.strictEqual(test_object.Delete(obj, 'foo'), true);
  assert.strictEqual('foo' in obj, false);
  assert.strictEqual(sym in obj, true);
  assert.strictEqual('does_not_exist' in obj, false);
  assert.strictEqual(test_object.Delete(obj, sym), true);
  assert.strictEqual('foo' in obj, false);
  assert.strictEqual(sym in obj, false);
  assert.strictEqual('does_not_exist' in obj, false);
}

{
  // Verify that non-configurable properties are not deleted.
  const obj = {};

  Object.defineProperty(obj, 'foo', { configurable: false });
  assert.strictEqual(test_object.Delete(obj, 'foo'), false);
  assert.strictEqual('foo' in obj, true);
}

{
  // Verify that prototype properties are not deleted.
  function Foo() {
    this.foo = 'bar';
  }

  Foo.prototype.foo = 'baz';

  const obj = new Foo();

  assert.strictEqual(obj.foo, 'bar');
  assert.strictEqual(test_object.Delete(obj, 'foo'), true);
  assert.strictEqual(obj.foo, 'baz');
  assert.strictEqual(test_object.Delete(obj, 'foo'), true);
  assert.strictEqual(obj.foo, 'baz');
}

{
  // Verify that napi_get_property_names gets the right set of property names,
  // i.e.: includes prototypes, only enumerable properties, skips symbols,
  // and includes indices and converts them to strings.

  const object = Object.create({
    inherited: 1
  });

  const fooSymbol = Symbol('foo');

  object.normal = 2;
  object[fooSymbol] = 3;
  Object.defineProperty(object, 'unenumerable', {
    value: 4,
    enumerable: false,
    writable: true,
    configurable: true
  });
  object[5] = 5;

  assert.deepStrictEqual(test_object.GetPropertyNames(object),
                         ['5', 'normal', 'inherited']);

  assert.deepStrictEqual(test_object.GetSymbolNames(object),
                         [fooSymbol]);
}

// Verify that passing NULL to napi_set_property() results in the correct
// error.
assert.deepStrictEqual(test_object.TestSetProperty(), {
  envIsNull: 'Invalid argument',
  objectIsNull: 'Invalid argument',
  keyIsNull: 'Invalid argument',
  valueIsNull: 'Invalid argument'
});

// Verify that passing NULL to napi_has_property() results in the correct
// error.
assert.deepStrictEqual(test_object.TestHasProperty(), {
  envIsNull: 'Invalid argument',
  objectIsNull: 'Invalid argument',
  keyIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument'
});

// Verify that passing NULL to napi_get_property() results in the correct
// error.
assert.deepStrictEqual(test_object.TestGetProperty(), {
  envIsNull: 'Invalid argument',
  objectIsNull: 'Invalid argument',
  keyIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument'
});
