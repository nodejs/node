
'use strict';

let spectest = {
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_f32: 666,
  global_f64: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2})
};
let handler = {
  get(target, prop) {
    return (prop in target) ?  target[prop] : {};
  }
};
let registry = new Proxy({spectest}, handler);

function register(name, instance) {
  registry[name] = instance.exports;
}

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  let validated;
  try {
    validated = WebAssembly.validate(buffer);
  } catch (e) {
    throw new Error("Wasm validate throws");
  }
  if (validated !== valid) {
    throw new Error("Wasm validate failure" + (valid ? "" : " expected"));
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance, name, args) {
  return instance.exports[name](...args);
}

function get(instance, name) {
  let v = instance.exports[name];
  return (v instanceof WebAssembly.Global) ? v.value : v;
}

function exports(name, instance) {
  return {[name]: instance.exports};
}

function run(action) {
  action();
}

function assert_malformed(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm decoding failure expected");
}

function assert_invalid(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_unlinkable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.LinkError) return;
  }
  throw new Error("Wasm linking failure expected");
}

function assert_uninstantiable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

function assert_trap(action) {
  try { action() } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
  try { action() } catch (e) {
    if (e instanceof StackOverflow) return;
  }
  throw new Error("Wasm resource exhaustion expected");
}

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

function assert_return_canonical_nan(action) {
  let actual = action();
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test that it's a canonical NaN.
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

function assert_return_arithmetic_nan(action) {
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test for specific bitpatterns here.
  let actual = action();
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

// bulk.wast:2
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x0b\x86\x80\x80\x80\x00\x01\x01\x03\x66\x6f\x6f");

// bulk.wast:6
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x83\x80\x80\x80\x00\x02\x00\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x03\x09\x8c\x80\x80\x80\x00\x01\x01\x70\x03\xd2\x00\x0b\xd0\x0b\xd2\x01\x0b\x0a\x8f\x80\x80\x80\x00\x02\x82\x80\x80\x80\x00\x00\x0b\x82\x80\x80\x80\x00\x00\x0b");

// bulk.wast:13
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x92\x80\x80\x80\x00\x02\x04\x66\x69\x6c\x6c\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9d\x80\x80\x80\x00\x02\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0b\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b");

// bulk.wast:27
run(() => call($3, "fill", [1, 255, 3]));

// bulk.wast:28
assert_return(() => call($3, "load8_u", [0]), 0);

// bulk.wast:29
assert_return(() => call($3, "load8_u", [1]), 255);

// bulk.wast:30
assert_return(() => call($3, "load8_u", [2]), 255);

// bulk.wast:31
assert_return(() => call($3, "load8_u", [3]), 255);

// bulk.wast:32
assert_return(() => call($3, "load8_u", [4]), 0);

// bulk.wast:35
run(() => call($3, "fill", [0, 48042, 2]));

// bulk.wast:36
assert_return(() => call($3, "load8_u", [0]), 170);

// bulk.wast:37
assert_return(() => call($3, "load8_u", [1]), 170);

// bulk.wast:40
run(() => call($3, "fill", [0, 0, 65536]));

// bulk.wast:43
assert_trap(() => call($3, "fill", [65280, 1, 257]));

// bulk.wast:45
assert_return(() => call($3, "load8_u", [65280]), 1);

// bulk.wast:46
assert_return(() => call($3, "load8_u", [65535]), 1);

// bulk.wast:49
run(() => call($3, "fill", [65536, 0, 0]));

// bulk.wast:52
assert_trap(() => call($3, "fill", [65537, 0, 0]));

// bulk.wast:57
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x92\x80\x80\x80\x00\x02\x04\x63\x6f\x70\x79\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x8a\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x04\xaa\xbb\xcc\xdd");

// bulk.wast:71
run(() => call($4, "copy", [10, 0, 4]));

// bulk.wast:73
assert_return(() => call($4, "load8_u", [9]), 0);

// bulk.wast:74
assert_return(() => call($4, "load8_u", [10]), 170);

// bulk.wast:75
assert_return(() => call($4, "load8_u", [11]), 187);

// bulk.wast:76
assert_return(() => call($4, "load8_u", [12]), 204);

// bulk.wast:77
assert_return(() => call($4, "load8_u", [13]), 221);

// bulk.wast:78
assert_return(() => call($4, "load8_u", [14]), 0);

// bulk.wast:81
run(() => call($4, "copy", [8, 10, 4]));

// bulk.wast:82
assert_return(() => call($4, "load8_u", [8]), 170);

// bulk.wast:83
assert_return(() => call($4, "load8_u", [9]), 187);

// bulk.wast:84
assert_return(() => call($4, "load8_u", [10]), 204);

// bulk.wast:85
assert_return(() => call($4, "load8_u", [11]), 221);

// bulk.wast:86
assert_return(() => call($4, "load8_u", [12]), 204);

// bulk.wast:87
assert_return(() => call($4, "load8_u", [13]), 221);

// bulk.wast:90
run(() => call($4, "copy", [10, 7, 6]));

// bulk.wast:91
assert_return(() => call($4, "load8_u", [10]), 0);

// bulk.wast:92
assert_return(() => call($4, "load8_u", [11]), 170);

// bulk.wast:93
assert_return(() => call($4, "load8_u", [12]), 187);

// bulk.wast:94
assert_return(() => call($4, "load8_u", [13]), 204);

// bulk.wast:95
assert_return(() => call($4, "load8_u", [14]), 221);

// bulk.wast:96
assert_return(() => call($4, "load8_u", [15]), 204);

// bulk.wast:97
assert_return(() => call($4, "load8_u", [16]), 0);

// bulk.wast:100
run(() => call($4, "copy", [65280, 0, 256]));

// bulk.wast:101
run(() => call($4, "copy", [65024, 65280, 256]));

// bulk.wast:104
assert_trap(() => call($4, "copy", [65534, 0, 3]));

// bulk.wast:106
assert_return(() => call($4, "load8_u", [65534]), 170);

// bulk.wast:107
assert_return(() => call($4, "load8_u", [65535]), 187);

// bulk.wast:110
run(() => call($4, "copy", [65536, 0, 0]));

// bulk.wast:111
run(() => call($4, "copy", [0, 65536, 0]));

// bulk.wast:114
assert_trap(() => call($4, "copy", [65537, 0, 0]));

// bulk.wast:116
assert_trap(() => call($4, "copy", [0, 65537, 0]));

// bulk.wast:121
let $5 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x92\x80\x80\x80\x00\x02\x04\x69\x6e\x69\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0c\x81\x80\x80\x80\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x08\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x87\x80\x80\x80\x00\x01\x01\x04\xaa\xbb\xcc\xdd");

// bulk.wast:135
run(() => call($5, "init", [0, 1, 2]));

// bulk.wast:136
assert_return(() => call($5, "load8_u", [0]), 187);

// bulk.wast:137
assert_return(() => call($5, "load8_u", [1]), 204);

// bulk.wast:138
assert_return(() => call($5, "load8_u", [2]), 0);

// bulk.wast:141
run(() => call($5, "init", [65532, 0, 4]));

// bulk.wast:144
assert_trap(() => call($5, "init", [65534, 0, 3]));

// bulk.wast:146
assert_return(() => call($5, "load8_u", [65534]), 170);

// bulk.wast:147
assert_return(() => call($5, "load8_u", [65535]), 187);

// bulk.wast:150
run(() => call($5, "init", [65536, 0, 0]));

// bulk.wast:151
run(() => call($5, "init", [0, 4, 0]));

// bulk.wast:154
assert_trap(() => call($5, "init", [65537, 0, 0]));

// bulk.wast:156
assert_trap(() => call($5, "init", [0, 5, 0]));

// bulk.wast:160
let $6 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x85\x80\x80\x80\x00\x04\x00\x00\x00\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\xbb\x80\x80\x80\x00\x04\x0c\x64\x72\x6f\x70\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x00\x0c\x69\x6e\x69\x74\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x01\x0b\x64\x72\x6f\x70\x5f\x61\x63\x74\x69\x76\x65\x00\x02\x0b\x69\x6e\x69\x74\x5f\x61\x63\x74\x69\x76\x65\x00\x03\x0c\x81\x80\x80\x80\x00\x02\x0a\xb7\x80\x80\x80\x00\x04\x85\x80\x80\x80\x00\x00\xfc\x09\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x41\x00\xfc\x08\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x09\x01\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x41\x00\xfc\x08\x01\x00\x0b\x0b\x88\x80\x80\x80\x00\x02\x01\x00\x00\x41\x00\x0b\x00");

// bulk.wast:174
run(() => call($6, "init_passive", []));

// bulk.wast:175
run(() => call($6, "drop_passive", []));

// bulk.wast:176
assert_trap(() => call($6, "drop_passive", []));

// bulk.wast:177
assert_trap(() => call($6, "init_passive", []));

// bulk.wast:178
assert_trap(() => call($6, "drop_active", []));

// bulk.wast:179
assert_trap(() => call($6, "init_active", []));

// bulk.wast:183
let $7 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x85\x80\x80\x80\x00\x04\x00\x00\x01\x02\x04\x84\x80\x80\x80\x00\x01\x70\x00\x03\x07\x8f\x80\x80\x80\x00\x02\x04\x69\x6e\x69\x74\x00\x02\x04\x63\x61\x6c\x6c\x00\x03\x09\x90\x80\x80\x80\x00\x01\x01\x70\x04\xd2\x00\x0b\xd2\x01\x0b\xd2\x00\x0b\xd2\x01\x0b\x0a\xb0\x80\x80\x80\x00\x04\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0c\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// bulk.wast:202
run(() => call($7, "init", [0, 1, 2]));

// bulk.wast:203
assert_return(() => call($7, "call", [0]), 1);

// bulk.wast:204
assert_return(() => call($7, "call", [1]), 0);

// bulk.wast:205
assert_trap(() => call($7, "call", [2]));

// bulk.wast:208
run(() => call($7, "init", [1, 2, 2]));

// bulk.wast:211
assert_trap(() => call($7, "init", [2, 0, 2]));

// bulk.wast:213
assert_return(() => call($7, "call", [2]), 0);

// bulk.wast:216
run(() => call($7, "init", [3, 0, 0]));

// bulk.wast:217
run(() => call($7, "init", [0, 4, 0]));

// bulk.wast:220
assert_trap(() => call($7, "init", [4, 0, 0]));

// bulk.wast:222
assert_trap(() => call($7, "init", [0, 5, 0]));

// bulk.wast:227
let $8 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x86\x80\x80\x80\x00\x05\x00\x00\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x01\x07\xbb\x80\x80\x80\x00\x04\x0c\x64\x72\x6f\x70\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x01\x0c\x69\x6e\x69\x74\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x02\x0b\x64\x72\x6f\x70\x5f\x61\x63\x74\x69\x76\x65\x00\x03\x0b\x69\x6e\x69\x74\x5f\x61\x63\x74\x69\x76\x65\x00\x04\x09\x8d\x80\x80\x80\x00\x02\x01\x70\x01\xd2\x00\x0b\x00\x41\x00\x0b\x01\x00\x0a\xbe\x80\x80\x80\x00\x05\x82\x80\x80\x80\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x0d\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x41\x00\xfc\x0c\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x0d\x01\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x41\x00\xfc\x0c\x01\x00\x0b");

// bulk.wast:242
run(() => call($8, "init_passive", []));

// bulk.wast:243
run(() => call($8, "drop_passive", []));

// bulk.wast:244
assert_trap(() => call($8, "drop_passive", []));

// bulk.wast:245
assert_trap(() => call($8, "init_passive", []));

// bulk.wast:246
assert_trap(() => call($8, "drop_active", []));

// bulk.wast:247
assert_trap(() => call($8, "init_active", []));

// bulk.wast:251
let $9 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x86\x80\x80\x80\x00\x05\x00\x00\x00\x01\x02\x04\x84\x80\x80\x80\x00\x01\x70\x00\x0a\x07\x8f\x80\x80\x80\x00\x02\x04\x63\x6f\x70\x79\x00\x03\x04\x63\x61\x6c\x6c\x00\x04\x09\x89\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x03\x00\x01\x02\x0a\xb9\x80\x80\x80\x00\x05\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// bulk.wast:270
run(() => call($9, "copy", [3, 0, 3]));

// bulk.wast:272
assert_return(() => call($9, "call", [3]), 0);

// bulk.wast:273
assert_return(() => call($9, "call", [4]), 1);

// bulk.wast:274
assert_return(() => call($9, "call", [5]), 2);

// bulk.wast:277
run(() => call($9, "copy", [0, 1, 3]));

// bulk.wast:279
assert_return(() => call($9, "call", [0]), 1);

// bulk.wast:280
assert_return(() => call($9, "call", [1]), 2);

// bulk.wast:281
assert_return(() => call($9, "call", [2]), 0);

// bulk.wast:284
run(() => call($9, "copy", [2, 0, 3]));

// bulk.wast:286
assert_return(() => call($9, "call", [2]), 1);

// bulk.wast:287
assert_return(() => call($9, "call", [3]), 2);

// bulk.wast:288
assert_return(() => call($9, "call", [4]), 0);

// bulk.wast:291
run(() => call($9, "copy", [6, 8, 2]));

// bulk.wast:292
run(() => call($9, "copy", [8, 6, 2]));

// bulk.wast:295
assert_trap(() => call($9, "call", [9]));

// bulk.wast:296
assert_trap(() => call($9, "copy", [9, 0, 2]));

// bulk.wast:298
assert_return(() => call($9, "call", [9]), 1);

// bulk.wast:301
run(() => call($9, "copy", [10, 0, 0]));

// bulk.wast:302
run(() => call($9, "copy", [0, 10, 0]));

// bulk.wast:305
assert_trap(() => call($9, "copy", [11, 0, 0]));

// bulk.wast:307
assert_trap(() => call($9, "copy", [0, 11, 0]));
