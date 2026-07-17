// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

let exports = {};
setup(() => {
  const builder = new WasmModuleBuilder();
  const structIndex = builder.addStruct([makeField(kWasmI32, true)]);
  const arrayIndex = builder.addArray(kWasmI32, true);
  const structIndex2 = builder.addStruct([makeField(kWasmF32, true)]);
  const arrayIndex2 = builder.addArray(kWasmF32, true);
  const funcIndex = builder.addType({ params: [], results: [] });
  const funcIndex2 = builder.addType({ params: [], results: [kWasmI32] });

  const argFunctions = [
    { name: "any", code: kWasmAnyRef },
    { name: "eq", code: kWasmEqRef },
    { name: "struct", code: kWasmStructRef },
    { name: "array", code: kWasmArrayRef },
    { name: "i31", code: kWasmI31Ref },
    { name: "func", code: kWasmFuncRef },
    { name: "extern", code: kWasmExternRef },
    { name: "none", code: kWasmNullRef },
    { name: "nofunc", code: kWasmNullFuncRef },
    { name: "noextern", code: kWasmNullExternRef },
    { name: "concreteStruct", code: structIndex },
    { name: "concreteArray", code: arrayIndex },
    { name: "concreteFunc", code: funcIndex },
  ];

  for (const desc of argFunctions) {
    builder
      .addFunction(desc.name + "Arg", makeSig_v_x(wasmRefType(desc.code)))
      .addBody([])
      .exportFunc();

    builder
      .addFunction(desc.name + "NullableArg", makeSig_v_x(wasmRefNullType(desc.code)))
      .addBody([])
      .exportFunc();
  }

  builder
    .addFunction("makeStruct", makeSig_r_v(wasmRefType(structIndex)))
    .addBody([...wasmI32Const(42),
              ...GCInstr(kExprStructNew), structIndex])
    .exportFunc();

  builder
    .addFunction("makeArray", makeSig_r_v(wasmRefType(arrayIndex)))
    .addBody([...wasmI32Const(5), ...wasmI32Const(42),
              ...GCInstr(kExprArrayNew), arrayIndex])
    .exportFunc();

  builder
    .addFunction("makeStruct2", makeSig_r_v(wasmRefType(structIndex2)))
    .addBody([...wasmF32Const(42),
              ...GCInstr(kExprStructNew), structIndex2])
    .exportFunc();

  builder
    .addFunction("makeArray2", makeSig_r_v(wasmRefType(arrayIndex2)))
    .addBody([...wasmF32Const(42), ...wasmI32Const(5),
              ...GCInstr(kExprArrayNew), arrayIndex2])
    .exportFunc();

  builder
    .addFunction("testFunc", funcIndex)
    .addBody([])
    .exportFunc();

  builder
    .addFunction("testFunc2", funcIndex2)
    .addBody([...wasmI32Const(42)])
    .exportFunc();

  const buffer = builder.toBuffer();
  const module = new WebAssembly.Module(buffer);
  const instance = new WebAssembly.Instance(module, {});
  exports = instance.exports;
});

test(() => {
  exports.anyArg(exports.makeStruct());
  exports.anyArg(exports.makeArray());
  exports.anyArg(42);
  exports.anyArg(42n);
  exports.anyArg("foo");
  exports.anyArg({});
  exports.anyArg(() => {});
  exports.anyArg(exports.testFunc);
  assert_throws_js(TypeError, () => exports.anyArg(null));

  exports.anyNullableArg(null);
  exports.anyNullableArg(exports.makeStruct());
  exports.anyNullableArg(exports.makeArray());
  exports.anyNullableArg(42);
  exports.anyNullableArg(42n);
  exports.anyNullableArg("foo");
  exports.anyNullableArg({});
  exports.anyNullableArg(() => {});
  exports.anyNullableArg(exports.testFunc);
}, "anyref casts");

test(() => {
  exports.eqArg(exports.makeStruct());
  exports.eqArg(exports.makeArray());
  exports.eqArg(42);
  assert_throws_js(TypeError, () => exports.eqArg(42n));
  assert_throws_js(TypeError, () => exports.eqArg("foo"));
  assert_throws_js(TypeError, () => exports.eqArg({}));
  assert_throws_js(TypeError, () => exports.eqArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.eqArg(() => {}));
  assert_throws_js(TypeError, () => exports.eqArg(null));

  exports.eqNullableArg(null);
  exports.eqNullableArg(exports.makeStruct());
  exports.eqNullableArg(exports.makeArray());
  exports.eqNullableArg(42);
  assert_throws_js(TypeError, () => exports.eqNullableArg(42n));
  assert_throws_js(TypeError, () => exports.eqNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.eqNullableArg({}));
  assert_throws_js(TypeError, () => exports.eqNullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.eqNullableArg(() => {}));
}, "eqref casts");

test(() => {
  exports.structArg(exports.makeStruct());
  assert_throws_js(TypeError, () => exports.structArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.structArg(42));
  assert_throws_js(TypeError, () => exports.structArg(42n));
  assert_throws_js(TypeError, () => exports.structArg("foo"));
  assert_throws_js(TypeError, () => exports.structArg({}));
  assert_throws_js(TypeError, () => exports.structArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.structArg(() => {}));
  assert_throws_js(TypeError, () => exports.structArg(null));

  exports.structNullableArg(null);
  exports.structNullableArg(exports.makeStruct());
  assert_throws_js(TypeError, () => exports.structNullableArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.structNullableArg(42));
  assert_throws_js(TypeError, () => exports.structNullableArg(42n));
  assert_throws_js(TypeError, () => exports.structNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.structNullableArg({}));
  assert_throws_js(TypeError, () => exports.structNullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.structNullableArg(() => {}));
}, "structref casts");

test(() => {
  exports.arrayArg(exports.makeArray());
  assert_throws_js(TypeError, () => exports.arrayArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.arrayArg(42));
  assert_throws_js(TypeError, () => exports.arrayArg(42n));
  assert_throws_js(TypeError, () => exports.arrayArg("foo"));
  assert_throws_js(TypeError, () => exports.arrayArg({}));
  assert_throws_js(TypeError, () => exports.arrayArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.arrayArg(() => {}));
  assert_throws_js(TypeError, () => exports.arrayArg(null));

  exports.arrayNullableArg(null);
  exports.arrayNullableArg(exports.makeArray());
  assert_throws_js(TypeError, () => exports.arrayNullableArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.arrayNullableArg(42));
  assert_throws_js(TypeError, () => exports.arrayNullableArg(42n));
  assert_throws_js(TypeError, () => exports.arrayNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.arrayNullableArg({}));
  assert_throws_js(TypeError, () => exports.arrayNullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.arrayNullableArg(() => {}));
}, "arrayref casts");

test(() => {
  exports.i31Arg(42);
  assert_throws_js(TypeError, () => exports.i31Arg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.i31Arg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.i31Arg(42n));
  assert_throws_js(TypeError, () => exports.i31Arg("foo"));
  assert_throws_js(TypeError, () => exports.i31Arg({}));
  assert_throws_js(TypeError, () => exports.i31Arg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.i31Arg(() => {}));
  assert_throws_js(TypeError, () => exports.i31Arg(null));

  exports.i31NullableArg(null);
  exports.i31NullableArg(42);
  assert_throws_js(TypeError, () => exports.i31NullableArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.i31NullableArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.i31NullableArg(42n));
  assert_throws_js(TypeError, () => exports.i31NullableArg("foo"));
  assert_throws_js(TypeError, () => exports.i31NullableArg({}));
  assert_throws_js(TypeError, () => exports.i31NullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.i31NullableArg(() => {}));
}, "i31ref casts");

test(() => {
  exports.funcArg(exports.testFunc);
  assert_throws_js(TypeError, () => exports.funcArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.funcArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.funcArg(42));
  assert_throws_js(TypeError, () => exports.funcArg(42n));
  assert_throws_js(TypeError, () => exports.funcArg("foo"));
  assert_throws_js(TypeError, () => exports.funcArg({}));
  assert_throws_js(TypeError, () => exports.funcArg(() => {}));
  assert_throws_js(TypeError, () => exports.funcArg(null));

  exports.funcNullableArg(null);
  exports.funcNullableArg(exports.testFunc);
  assert_throws_js(TypeError, () => exports.funcNullableArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.funcNullableArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.funcNullableArg(42));
  assert_throws_js(TypeError, () => exports.funcNullableArg(42n));
  assert_throws_js(TypeError, () => exports.funcNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.funcNullableArg({}));
  assert_throws_js(TypeError, () => exports.funcNullableArg(() => {}));
}, "funcref casts");

test(() => {
  exports.externArg(exports.makeArray());
  exports.externArg(exports.makeStruct());
  exports.externArg(42);
  exports.externArg(42n);
  exports.externArg("foo");
  exports.externArg({});
  exports.externArg(exports.testFunc);
  exports.externArg(() => {});
  assert_throws_js(TypeError, () => exports.externArg(null));

  exports.externNullableArg(null);
  exports.externNullableArg(exports.makeArray());
  exports.externNullableArg(exports.makeStruct());
  exports.externNullableArg(42);
  exports.externNullableArg(42n);
  exports.externNullableArg("foo");
  exports.externNullableArg({});
  exports.externNullableArg(exports.testFunc);
  exports.externNullableArg(() => {});
}, "externref casts");

test(() => {
  for (const nullfunc of [exports.noneArg, exports.nofuncArg, exports.noexternArg]) {
    assert_throws_js(TypeError, () => nullfunc(exports.makeStruct()));
    assert_throws_js(TypeError, () => nullfunc(exports.makeArray()));
    assert_throws_js(TypeError, () => nullfunc(42));
    assert_throws_js(TypeError, () => nullfunc(42n));
    assert_throws_js(TypeError, () => nullfunc("foo"));
    assert_throws_js(TypeError, () => nullfunc({}));
    assert_throws_js(TypeError, () => nullfunc(exports.testFunc));
    assert_throws_js(TypeError, () => nullfunc(() => {}));
    assert_throws_js(TypeError, () => nullfunc(null));
  }

  for (const nullfunc of [exports.noneNullableArg, exports.nofuncNullableArg, exports.noexternNullableArg]) {
    nullfunc(null);
    assert_throws_js(TypeError, () => nullfunc(exports.makeStruct()));
    assert_throws_js(TypeError, () => nullfunc(exports.makeArray()));
    assert_throws_js(TypeError, () => nullfunc(42));
    assert_throws_js(TypeError, () => nullfunc(42n));
    assert_throws_js(TypeError, () => nullfunc("foo"));
    assert_throws_js(TypeError, () => nullfunc({}));
    assert_throws_js(TypeError, () => nullfunc(exports.testFunc));
    assert_throws_js(TypeError, () => nullfunc(() => {}));
  }
}, "null casts");

test(() => {
  exports.concreteStructArg(exports.makeStruct());
  assert_throws_js(TypeError, () => exports.concreteStructArg(exports.makeStruct2()));
  assert_throws_js(TypeError, () => exports.concreteStructArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.concreteStructArg(42));
  assert_throws_js(TypeError, () => exports.concreteStructArg(42n));
  assert_throws_js(TypeError, () => exports.concreteStructArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteStructArg({}));
  assert_throws_js(TypeError, () => exports.concreteStructArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.concreteStructArg(() => {}));
  assert_throws_js(TypeError, () => exports.concreteStructArg(null));

  exports.concreteStructNullableArg(null);
  exports.concreteStructNullableArg(exports.makeStruct());
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(exports.makeStruct2()));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(42));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(42n));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg({}));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.concreteStructNullableArg(() => {}));
}, "concrete struct casts");

test(() => {
  exports.concreteArrayArg(exports.makeArray());
  assert_throws_js(TypeError, () => exports.concreteArrayArg(exports.makeArray2()));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(42));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(42n));
  assert_throws_js(TypeError, () => exports.concreteArrayArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteArrayArg({}));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(() => {}));
  assert_throws_js(TypeError, () => exports.concreteArrayArg(null));

  exports.concreteArrayNullableArg(null);
  exports.concreteArrayNullableArg(exports.makeArray());
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(exports.makeArray2()));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(42));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(42n));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg({}));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(exports.testFunc));
  assert_throws_js(TypeError, () => exports.concreteArrayNullableArg(() => {}));
}, "concrete array casts");

test(() => {
  exports.concreteFuncArg(exports.testFunc);
  assert_throws_js(TypeError, () => exports.concreteFuncArg(exports.testFunc2));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(42));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(42n));
  assert_throws_js(TypeError, () => exports.concreteFuncArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteFuncArg({}));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(() => {}));
  assert_throws_js(TypeError, () => exports.concreteFuncArg(null));

  exports.concreteFuncNullableArg(null);
  exports.concreteFuncNullableArg(exports.testFunc);
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(exports.testFunc2));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(exports.makeArray()));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(exports.makeStruct()));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(42));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(42n));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg("foo"));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg({}));
  assert_throws_js(TypeError, () => exports.concreteFuncNullableArg(() => {}));
}, "concrete func casts");
