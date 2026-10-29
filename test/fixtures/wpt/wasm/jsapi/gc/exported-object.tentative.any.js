// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

let functions = {};
setup(() => {
  const builder = new WasmModuleBuilder();

  const structIndex = builder.addStruct([makeField(kWasmI32, true)]);
  const arrayIndex = builder.addArray(kWasmI32, true);
  const structRef = wasmRefType(structIndex);
  const arrayRef = wasmRefType(arrayIndex);

  builder
    .addFunction("makeStruct", makeSig_r_v(structRef))
    .addBody([...wasmI32Const(42),
              ...GCInstr(kExprStructNew), structIndex])
    .exportFunc();

  builder
    .addFunction("makeArray", makeSig_r_v(arrayRef))
    .addBody([...wasmI32Const(5), ...wasmI32Const(42),
              ...GCInstr(kExprArrayNew), arrayIndex])
    .exportFunc();

  const buffer = builder.toBuffer();
  const module = new WebAssembly.Module(buffer);
  const instance = new WebAssembly.Instance(module, {});
  functions = instance.exports;
});

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_equals(struct.foo, undefined);
  assert_equals(struct[0], undefined);
  assert_equals(array.foo, undefined);
  assert_equals(array[0], undefined);
}, "property access");

test(() => {
  "use strict";
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => { struct.foo = 5; });
  assert_throws_js(TypeError, () => { array.foo = 5; });
  assert_throws_js(TypeError, () => { struct[0] = 5; });
  assert_throws_js(TypeError, () => { array[0] = 5; });
}, "property assignment (strict mode)");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => { struct.foo = 5; });
  assert_throws_js(TypeError, () => { array.foo = 5; });
  assert_throws_js(TypeError, () => { struct[0] = 5; });
  assert_throws_js(TypeError, () => { array[0] = 5; });
}, "property assignment (non-strict mode)");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_equals(Object.getOwnPropertyNames(struct).length, 0);
  assert_equals(Object.getOwnPropertyNames(array).length, 0);
}, "ownPropertyNames");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => Object.defineProperty(struct, "foo", { value: 1 }));
  assert_throws_js(TypeError, () => Object.defineProperty(array, "foo", { value: 1 }));
}, "defineProperty");

test(() => {
  "use strict";
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => delete struct.foo);
  assert_throws_js(TypeError, () => delete struct[0]);
  assert_throws_js(TypeError, () => delete array.foo);
  assert_throws_js(TypeError, () => delete array[0]);
}, "delete (strict mode)");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => delete struct.foo);
  assert_throws_js(TypeError, () => delete struct[0]);
  assert_throws_js(TypeError, () => delete array.foo);
  assert_throws_js(TypeError, () => delete array[0]);
}, "delete (non-strict mode)");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_equals(Object.getPrototypeOf(struct), null);
  assert_equals(Object.getPrototypeOf(array), null);
}, "getPrototypeOf");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => Object.setPrototypeOf(struct, {}));
  assert_throws_js(TypeError, () => Object.setPrototypeOf(array, {}));
}, "setPrototypeOf");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_false(Object.isExtensible(struct));
  assert_false(Object.isExtensible(array));
}, "isExtensible");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => Object.preventExtensions(struct));
  assert_throws_js(TypeError, () => Object.preventExtensions(array));
}, "preventExtensions");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => Object.seal(struct));
  assert_throws_js(TypeError, () => Object.seal(array));
}, "sealing");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_equals(typeof struct, "object");
  assert_equals(typeof array, "object");
}, "typeof");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => struct.toString());
  assert_equals(Object.prototype.toString.call(struct), "[object Object]");
  assert_throws_js(TypeError, () => array.toString());
  assert_equals(Object.prototype.toString.call(array), "[object Object]");
}, "toString");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  assert_throws_js(TypeError, () => struct.valueOf());
  assert_equals(Object.prototype.valueOf.call(struct), struct);
  assert_throws_js(TypeError, () => array.valueOf());
  assert_equals(Object.prototype.valueOf.call(array), array);
}, "valueOf");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  const map = new Map();
  map.set(struct, "struct");
  map.set(array, "array");
  assert_equals(map.get(struct), "struct");
  assert_equals(map.get(array), "array");
}, "GC objects as map keys");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  const set = new Set();
  set.add(struct);
  set.add(array);
  assert_true(set.has(struct));
  assert_true(set.has(array));
}, "GC objects as set element");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  const map = new WeakMap();
  map.set(struct, "struct");
  map.set(array, "array");
  assert_equals(map.get(struct), "struct");
  assert_equals(map.get(array), "array");
}, "GC objects as weak map keys");

test(() => {
  const struct = functions.makeStruct();
  const array = functions.makeArray();
  const set = new WeakSet();
  set.add(struct);
  set.add(array);
  assert_true(set.has(struct));
  assert_true(set.has(array));
}, "GC objects as weak set element");
