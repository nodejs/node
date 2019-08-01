
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

// memory_copy.wast:5
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x95\x80\x80\x80\x00\x02\x83\x80\x80\x80\x00\x00\x01\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:14
run(() => call($1, "test", []));

// memory_copy.wast:16
assert_return(() => call($1, "load8_u", [0]), 0);

// memory_copy.wast:17
assert_return(() => call($1, "load8_u", [1]), 0);

// memory_copy.wast:18
assert_return(() => call($1, "load8_u", [2]), 3);

// memory_copy.wast:19
assert_return(() => call($1, "load8_u", [3]), 1);

// memory_copy.wast:20
assert_return(() => call($1, "load8_u", [4]), 4);

// memory_copy.wast:21
assert_return(() => call($1, "load8_u", [5]), 1);

// memory_copy.wast:22
assert_return(() => call($1, "load8_u", [6]), 0);

// memory_copy.wast:23
assert_return(() => call($1, "load8_u", [7]), 0);

// memory_copy.wast:24
assert_return(() => call($1, "load8_u", [8]), 0);

// memory_copy.wast:25
assert_return(() => call($1, "load8_u", [9]), 0);

// memory_copy.wast:26
assert_return(() => call($1, "load8_u", [10]), 0);

// memory_copy.wast:27
assert_return(() => call($1, "load8_u", [11]), 0);

// memory_copy.wast:28
assert_return(() => call($1, "load8_u", [12]), 7);

// memory_copy.wast:29
assert_return(() => call($1, "load8_u", [13]), 5);

// memory_copy.wast:30
assert_return(() => call($1, "load8_u", [14]), 2);

// memory_copy.wast:31
assert_return(() => call($1, "load8_u", [15]), 3);

// memory_copy.wast:32
assert_return(() => call($1, "load8_u", [16]), 6);

// memory_copy.wast:33
assert_return(() => call($1, "load8_u", [17]), 0);

// memory_copy.wast:34
assert_return(() => call($1, "load8_u", [18]), 0);

// memory_copy.wast:35
assert_return(() => call($1, "load8_u", [19]), 0);

// memory_copy.wast:36
assert_return(() => call($1, "load8_u", [20]), 0);

// memory_copy.wast:37
assert_return(() => call($1, "load8_u", [21]), 0);

// memory_copy.wast:38
assert_return(() => call($1, "load8_u", [22]), 0);

// memory_copy.wast:39
assert_return(() => call($1, "load8_u", [23]), 0);

// memory_copy.wast:40
assert_return(() => call($1, "load8_u", [24]), 0);

// memory_copy.wast:41
assert_return(() => call($1, "load8_u", [25]), 0);

// memory_copy.wast:42
assert_return(() => call($1, "load8_u", [26]), 0);

// memory_copy.wast:43
assert_return(() => call($1, "load8_u", [27]), 0);

// memory_copy.wast:44
assert_return(() => call($1, "load8_u", [28]), 0);

// memory_copy.wast:45
assert_return(() => call($1, "load8_u", [29]), 0);

// memory_copy.wast:47
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x0d\x41\x02\x41\x03\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:56
run(() => call($2, "test", []));

// memory_copy.wast:58
assert_return(() => call($2, "load8_u", [0]), 0);

// memory_copy.wast:59
assert_return(() => call($2, "load8_u", [1]), 0);

// memory_copy.wast:60
assert_return(() => call($2, "load8_u", [2]), 3);

// memory_copy.wast:61
assert_return(() => call($2, "load8_u", [3]), 1);

// memory_copy.wast:62
assert_return(() => call($2, "load8_u", [4]), 4);

// memory_copy.wast:63
assert_return(() => call($2, "load8_u", [5]), 1);

// memory_copy.wast:64
assert_return(() => call($2, "load8_u", [6]), 0);

// memory_copy.wast:65
assert_return(() => call($2, "load8_u", [7]), 0);

// memory_copy.wast:66
assert_return(() => call($2, "load8_u", [8]), 0);

// memory_copy.wast:67
assert_return(() => call($2, "load8_u", [9]), 0);

// memory_copy.wast:68
assert_return(() => call($2, "load8_u", [10]), 0);

// memory_copy.wast:69
assert_return(() => call($2, "load8_u", [11]), 0);

// memory_copy.wast:70
assert_return(() => call($2, "load8_u", [12]), 7);

// memory_copy.wast:71
assert_return(() => call($2, "load8_u", [13]), 3);

// memory_copy.wast:72
assert_return(() => call($2, "load8_u", [14]), 1);

// memory_copy.wast:73
assert_return(() => call($2, "load8_u", [15]), 4);

// memory_copy.wast:74
assert_return(() => call($2, "load8_u", [16]), 6);

// memory_copy.wast:75
assert_return(() => call($2, "load8_u", [17]), 0);

// memory_copy.wast:76
assert_return(() => call($2, "load8_u", [18]), 0);

// memory_copy.wast:77
assert_return(() => call($2, "load8_u", [19]), 0);

// memory_copy.wast:78
assert_return(() => call($2, "load8_u", [20]), 0);

// memory_copy.wast:79
assert_return(() => call($2, "load8_u", [21]), 0);

// memory_copy.wast:80
assert_return(() => call($2, "load8_u", [22]), 0);

// memory_copy.wast:81
assert_return(() => call($2, "load8_u", [23]), 0);

// memory_copy.wast:82
assert_return(() => call($2, "load8_u", [24]), 0);

// memory_copy.wast:83
assert_return(() => call($2, "load8_u", [25]), 0);

// memory_copy.wast:84
assert_return(() => call($2, "load8_u", [26]), 0);

// memory_copy.wast:85
assert_return(() => call($2, "load8_u", [27]), 0);

// memory_copy.wast:86
assert_return(() => call($2, "load8_u", [28]), 0);

// memory_copy.wast:87
assert_return(() => call($2, "load8_u", [29]), 0);

// memory_copy.wast:89
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x19\x41\x0f\x41\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:98
run(() => call($3, "test", []));

// memory_copy.wast:100
assert_return(() => call($3, "load8_u", [0]), 0);

// memory_copy.wast:101
assert_return(() => call($3, "load8_u", [1]), 0);

// memory_copy.wast:102
assert_return(() => call($3, "load8_u", [2]), 3);

// memory_copy.wast:103
assert_return(() => call($3, "load8_u", [3]), 1);

// memory_copy.wast:104
assert_return(() => call($3, "load8_u", [4]), 4);

// memory_copy.wast:105
assert_return(() => call($3, "load8_u", [5]), 1);

// memory_copy.wast:106
assert_return(() => call($3, "load8_u", [6]), 0);

// memory_copy.wast:107
assert_return(() => call($3, "load8_u", [7]), 0);

// memory_copy.wast:108
assert_return(() => call($3, "load8_u", [8]), 0);

// memory_copy.wast:109
assert_return(() => call($3, "load8_u", [9]), 0);

// memory_copy.wast:110
assert_return(() => call($3, "load8_u", [10]), 0);

// memory_copy.wast:111
assert_return(() => call($3, "load8_u", [11]), 0);

// memory_copy.wast:112
assert_return(() => call($3, "load8_u", [12]), 7);

// memory_copy.wast:113
assert_return(() => call($3, "load8_u", [13]), 5);

// memory_copy.wast:114
assert_return(() => call($3, "load8_u", [14]), 2);

// memory_copy.wast:115
assert_return(() => call($3, "load8_u", [15]), 3);

// memory_copy.wast:116
assert_return(() => call($3, "load8_u", [16]), 6);

// memory_copy.wast:117
assert_return(() => call($3, "load8_u", [17]), 0);

// memory_copy.wast:118
assert_return(() => call($3, "load8_u", [18]), 0);

// memory_copy.wast:119
assert_return(() => call($3, "load8_u", [19]), 0);

// memory_copy.wast:120
assert_return(() => call($3, "load8_u", [20]), 0);

// memory_copy.wast:121
assert_return(() => call($3, "load8_u", [21]), 0);

// memory_copy.wast:122
assert_return(() => call($3, "load8_u", [22]), 0);

// memory_copy.wast:123
assert_return(() => call($3, "load8_u", [23]), 0);

// memory_copy.wast:124
assert_return(() => call($3, "load8_u", [24]), 0);

// memory_copy.wast:125
assert_return(() => call($3, "load8_u", [25]), 3);

// memory_copy.wast:126
assert_return(() => call($3, "load8_u", [26]), 6);

// memory_copy.wast:127
assert_return(() => call($3, "load8_u", [27]), 0);

// memory_copy.wast:128
assert_return(() => call($3, "load8_u", [28]), 0);

// memory_copy.wast:129
assert_return(() => call($3, "load8_u", [29]), 0);

// memory_copy.wast:131
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x0d\x41\x19\x41\x03\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:140
run(() => call($4, "test", []));

// memory_copy.wast:142
assert_return(() => call($4, "load8_u", [0]), 0);

// memory_copy.wast:143
assert_return(() => call($4, "load8_u", [1]), 0);

// memory_copy.wast:144
assert_return(() => call($4, "load8_u", [2]), 3);

// memory_copy.wast:145
assert_return(() => call($4, "load8_u", [3]), 1);

// memory_copy.wast:146
assert_return(() => call($4, "load8_u", [4]), 4);

// memory_copy.wast:147
assert_return(() => call($4, "load8_u", [5]), 1);

// memory_copy.wast:148
assert_return(() => call($4, "load8_u", [6]), 0);

// memory_copy.wast:149
assert_return(() => call($4, "load8_u", [7]), 0);

// memory_copy.wast:150
assert_return(() => call($4, "load8_u", [8]), 0);

// memory_copy.wast:151
assert_return(() => call($4, "load8_u", [9]), 0);

// memory_copy.wast:152
assert_return(() => call($4, "load8_u", [10]), 0);

// memory_copy.wast:153
assert_return(() => call($4, "load8_u", [11]), 0);

// memory_copy.wast:154
assert_return(() => call($4, "load8_u", [12]), 7);

// memory_copy.wast:155
assert_return(() => call($4, "load8_u", [13]), 0);

// memory_copy.wast:156
assert_return(() => call($4, "load8_u", [14]), 0);

// memory_copy.wast:157
assert_return(() => call($4, "load8_u", [15]), 0);

// memory_copy.wast:158
assert_return(() => call($4, "load8_u", [16]), 6);

// memory_copy.wast:159
assert_return(() => call($4, "load8_u", [17]), 0);

// memory_copy.wast:160
assert_return(() => call($4, "load8_u", [18]), 0);

// memory_copy.wast:161
assert_return(() => call($4, "load8_u", [19]), 0);

// memory_copy.wast:162
assert_return(() => call($4, "load8_u", [20]), 0);

// memory_copy.wast:163
assert_return(() => call($4, "load8_u", [21]), 0);

// memory_copy.wast:164
assert_return(() => call($4, "load8_u", [22]), 0);

// memory_copy.wast:165
assert_return(() => call($4, "load8_u", [23]), 0);

// memory_copy.wast:166
assert_return(() => call($4, "load8_u", [24]), 0);

// memory_copy.wast:167
assert_return(() => call($4, "load8_u", [25]), 0);

// memory_copy.wast:168
assert_return(() => call($4, "load8_u", [26]), 0);

// memory_copy.wast:169
assert_return(() => call($4, "load8_u", [27]), 0);

// memory_copy.wast:170
assert_return(() => call($4, "load8_u", [28]), 0);

// memory_copy.wast:171
assert_return(() => call($4, "load8_u", [29]), 0);

// memory_copy.wast:173
let $5 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x14\x41\x16\x41\x04\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:182
run(() => call($5, "test", []));

// memory_copy.wast:184
assert_return(() => call($5, "load8_u", [0]), 0);

// memory_copy.wast:185
assert_return(() => call($5, "load8_u", [1]), 0);

// memory_copy.wast:186
assert_return(() => call($5, "load8_u", [2]), 3);

// memory_copy.wast:187
assert_return(() => call($5, "load8_u", [3]), 1);

// memory_copy.wast:188
assert_return(() => call($5, "load8_u", [4]), 4);

// memory_copy.wast:189
assert_return(() => call($5, "load8_u", [5]), 1);

// memory_copy.wast:190
assert_return(() => call($5, "load8_u", [6]), 0);

// memory_copy.wast:191
assert_return(() => call($5, "load8_u", [7]), 0);

// memory_copy.wast:192
assert_return(() => call($5, "load8_u", [8]), 0);

// memory_copy.wast:193
assert_return(() => call($5, "load8_u", [9]), 0);

// memory_copy.wast:194
assert_return(() => call($5, "load8_u", [10]), 0);

// memory_copy.wast:195
assert_return(() => call($5, "load8_u", [11]), 0);

// memory_copy.wast:196
assert_return(() => call($5, "load8_u", [12]), 7);

// memory_copy.wast:197
assert_return(() => call($5, "load8_u", [13]), 5);

// memory_copy.wast:198
assert_return(() => call($5, "load8_u", [14]), 2);

// memory_copy.wast:199
assert_return(() => call($5, "load8_u", [15]), 3);

// memory_copy.wast:200
assert_return(() => call($5, "load8_u", [16]), 6);

// memory_copy.wast:201
assert_return(() => call($5, "load8_u", [17]), 0);

// memory_copy.wast:202
assert_return(() => call($5, "load8_u", [18]), 0);

// memory_copy.wast:203
assert_return(() => call($5, "load8_u", [19]), 0);

// memory_copy.wast:204
assert_return(() => call($5, "load8_u", [20]), 0);

// memory_copy.wast:205
assert_return(() => call($5, "load8_u", [21]), 0);

// memory_copy.wast:206
assert_return(() => call($5, "load8_u", [22]), 0);

// memory_copy.wast:207
assert_return(() => call($5, "load8_u", [23]), 0);

// memory_copy.wast:208
assert_return(() => call($5, "load8_u", [24]), 0);

// memory_copy.wast:209
assert_return(() => call($5, "load8_u", [25]), 0);

// memory_copy.wast:210
assert_return(() => call($5, "load8_u", [26]), 0);

// memory_copy.wast:211
assert_return(() => call($5, "load8_u", [27]), 0);

// memory_copy.wast:212
assert_return(() => call($5, "load8_u", [28]), 0);

// memory_copy.wast:213
assert_return(() => call($5, "load8_u", [29]), 0);

// memory_copy.wast:215
let $6 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x19\x41\x01\x41\x03\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:224
run(() => call($6, "test", []));

// memory_copy.wast:226
assert_return(() => call($6, "load8_u", [0]), 0);

// memory_copy.wast:227
assert_return(() => call($6, "load8_u", [1]), 0);

// memory_copy.wast:228
assert_return(() => call($6, "load8_u", [2]), 3);

// memory_copy.wast:229
assert_return(() => call($6, "load8_u", [3]), 1);

// memory_copy.wast:230
assert_return(() => call($6, "load8_u", [4]), 4);

// memory_copy.wast:231
assert_return(() => call($6, "load8_u", [5]), 1);

// memory_copy.wast:232
assert_return(() => call($6, "load8_u", [6]), 0);

// memory_copy.wast:233
assert_return(() => call($6, "load8_u", [7]), 0);

// memory_copy.wast:234
assert_return(() => call($6, "load8_u", [8]), 0);

// memory_copy.wast:235
assert_return(() => call($6, "load8_u", [9]), 0);

// memory_copy.wast:236
assert_return(() => call($6, "load8_u", [10]), 0);

// memory_copy.wast:237
assert_return(() => call($6, "load8_u", [11]), 0);

// memory_copy.wast:238
assert_return(() => call($6, "load8_u", [12]), 7);

// memory_copy.wast:239
assert_return(() => call($6, "load8_u", [13]), 5);

// memory_copy.wast:240
assert_return(() => call($6, "load8_u", [14]), 2);

// memory_copy.wast:241
assert_return(() => call($6, "load8_u", [15]), 3);

// memory_copy.wast:242
assert_return(() => call($6, "load8_u", [16]), 6);

// memory_copy.wast:243
assert_return(() => call($6, "load8_u", [17]), 0);

// memory_copy.wast:244
assert_return(() => call($6, "load8_u", [18]), 0);

// memory_copy.wast:245
assert_return(() => call($6, "load8_u", [19]), 0);

// memory_copy.wast:246
assert_return(() => call($6, "load8_u", [20]), 0);

// memory_copy.wast:247
assert_return(() => call($6, "load8_u", [21]), 0);

// memory_copy.wast:248
assert_return(() => call($6, "load8_u", [22]), 0);

// memory_copy.wast:249
assert_return(() => call($6, "load8_u", [23]), 0);

// memory_copy.wast:250
assert_return(() => call($6, "load8_u", [24]), 0);

// memory_copy.wast:251
assert_return(() => call($6, "load8_u", [25]), 0);

// memory_copy.wast:252
assert_return(() => call($6, "load8_u", [26]), 3);

// memory_copy.wast:253
assert_return(() => call($6, "load8_u", [27]), 1);

// memory_copy.wast:254
assert_return(() => call($6, "load8_u", [28]), 0);

// memory_copy.wast:255
assert_return(() => call($6, "load8_u", [29]), 0);

// memory_copy.wast:257
let $7 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x0a\x41\x0c\x41\x07\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:266
run(() => call($7, "test", []));

// memory_copy.wast:268
assert_return(() => call($7, "load8_u", [0]), 0);

// memory_copy.wast:269
assert_return(() => call($7, "load8_u", [1]), 0);

// memory_copy.wast:270
assert_return(() => call($7, "load8_u", [2]), 3);

// memory_copy.wast:271
assert_return(() => call($7, "load8_u", [3]), 1);

// memory_copy.wast:272
assert_return(() => call($7, "load8_u", [4]), 4);

// memory_copy.wast:273
assert_return(() => call($7, "load8_u", [5]), 1);

// memory_copy.wast:274
assert_return(() => call($7, "load8_u", [6]), 0);

// memory_copy.wast:275
assert_return(() => call($7, "load8_u", [7]), 0);

// memory_copy.wast:276
assert_return(() => call($7, "load8_u", [8]), 0);

// memory_copy.wast:277
assert_return(() => call($7, "load8_u", [9]), 0);

// memory_copy.wast:278
assert_return(() => call($7, "load8_u", [10]), 7);

// memory_copy.wast:279
assert_return(() => call($7, "load8_u", [11]), 5);

// memory_copy.wast:280
assert_return(() => call($7, "load8_u", [12]), 2);

// memory_copy.wast:281
assert_return(() => call($7, "load8_u", [13]), 3);

// memory_copy.wast:282
assert_return(() => call($7, "load8_u", [14]), 6);

// memory_copy.wast:283
assert_return(() => call($7, "load8_u", [15]), 0);

// memory_copy.wast:284
assert_return(() => call($7, "load8_u", [16]), 0);

// memory_copy.wast:285
assert_return(() => call($7, "load8_u", [17]), 0);

// memory_copy.wast:286
assert_return(() => call($7, "load8_u", [18]), 0);

// memory_copy.wast:287
assert_return(() => call($7, "load8_u", [19]), 0);

// memory_copy.wast:288
assert_return(() => call($7, "load8_u", [20]), 0);

// memory_copy.wast:289
assert_return(() => call($7, "load8_u", [21]), 0);

// memory_copy.wast:290
assert_return(() => call($7, "load8_u", [22]), 0);

// memory_copy.wast:291
assert_return(() => call($7, "load8_u", [23]), 0);

// memory_copy.wast:292
assert_return(() => call($7, "load8_u", [24]), 0);

// memory_copy.wast:293
assert_return(() => call($7, "load8_u", [25]), 0);

// memory_copy.wast:294
assert_return(() => call($7, "load8_u", [26]), 0);

// memory_copy.wast:295
assert_return(() => call($7, "load8_u", [27]), 0);

// memory_copy.wast:296
assert_return(() => call($7, "load8_u", [28]), 0);

// memory_copy.wast:297
assert_return(() => call($7, "load8_u", [29]), 0);

// memory_copy.wast:299
let $8 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x9c\x80\x80\x80\x00\x03\x07\x6d\x65\x6d\x6f\x72\x79\x30\x02\x00\x04\x74\x65\x73\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x41\x0c\x41\x0a\x41\x07\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x94\x80\x80\x80\x00\x02\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06");

// memory_copy.wast:308
run(() => call($8, "test", []));

// memory_copy.wast:310
assert_return(() => call($8, "load8_u", [0]), 0);

// memory_copy.wast:311
assert_return(() => call($8, "load8_u", [1]), 0);

// memory_copy.wast:312
assert_return(() => call($8, "load8_u", [2]), 3);

// memory_copy.wast:313
assert_return(() => call($8, "load8_u", [3]), 1);

// memory_copy.wast:314
assert_return(() => call($8, "load8_u", [4]), 4);

// memory_copy.wast:315
assert_return(() => call($8, "load8_u", [5]), 1);

// memory_copy.wast:316
assert_return(() => call($8, "load8_u", [6]), 0);

// memory_copy.wast:317
assert_return(() => call($8, "load8_u", [7]), 0);

// memory_copy.wast:318
assert_return(() => call($8, "load8_u", [8]), 0);

// memory_copy.wast:319
assert_return(() => call($8, "load8_u", [9]), 0);

// memory_copy.wast:320
assert_return(() => call($8, "load8_u", [10]), 0);

// memory_copy.wast:321
assert_return(() => call($8, "load8_u", [11]), 0);

// memory_copy.wast:322
assert_return(() => call($8, "load8_u", [12]), 0);

// memory_copy.wast:323
assert_return(() => call($8, "load8_u", [13]), 0);

// memory_copy.wast:324
assert_return(() => call($8, "load8_u", [14]), 7);

// memory_copy.wast:325
assert_return(() => call($8, "load8_u", [15]), 5);

// memory_copy.wast:326
assert_return(() => call($8, "load8_u", [16]), 2);

// memory_copy.wast:327
assert_return(() => call($8, "load8_u", [17]), 3);

// memory_copy.wast:328
assert_return(() => call($8, "load8_u", [18]), 6);

// memory_copy.wast:329
assert_return(() => call($8, "load8_u", [19]), 0);

// memory_copy.wast:330
assert_return(() => call($8, "load8_u", [20]), 0);

// memory_copy.wast:331
assert_return(() => call($8, "load8_u", [21]), 0);

// memory_copy.wast:332
assert_return(() => call($8, "load8_u", [22]), 0);

// memory_copy.wast:333
assert_return(() => call($8, "load8_u", [23]), 0);

// memory_copy.wast:334
assert_return(() => call($8, "load8_u", [24]), 0);

// memory_copy.wast:335
assert_return(() => call($8, "load8_u", [25]), 0);

// memory_copy.wast:336
assert_return(() => call($8, "load8_u", [26]), 0);

// memory_copy.wast:337
assert_return(() => call($8, "load8_u", [27]), 0);

// memory_copy.wast:338
assert_return(() => call($8, "load8_u", [28]), 0);

// memory_copy.wast:339
assert_return(() => call($8, "load8_u", [29]), 0);

// memory_copy.wast:341
let $9 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9a\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:349
assert_trap(() => call($9, "run", [65516, 0, 40]));

// memory_copy.wast:352
assert_return(() => call($9, "load8_u", [0]), 0);

// memory_copy.wast:353
assert_return(() => call($9, "load8_u", [1]), 1);

// memory_copy.wast:354
assert_return(() => call($9, "load8_u", [2]), 2);

// memory_copy.wast:355
assert_return(() => call($9, "load8_u", [3]), 3);

// memory_copy.wast:356
assert_return(() => call($9, "load8_u", [4]), 4);

// memory_copy.wast:357
assert_return(() => call($9, "load8_u", [5]), 5);

// memory_copy.wast:358
assert_return(() => call($9, "load8_u", [6]), 6);

// memory_copy.wast:359
assert_return(() => call($9, "load8_u", [7]), 7);

// memory_copy.wast:360
assert_return(() => call($9, "load8_u", [8]), 8);

// memory_copy.wast:361
assert_return(() => call($9, "load8_u", [9]), 9);

// memory_copy.wast:362
assert_return(() => call($9, "load8_u", [10]), 10);

// memory_copy.wast:363
assert_return(() => call($9, "load8_u", [11]), 11);

// memory_copy.wast:364
assert_return(() => call($9, "load8_u", [12]), 12);

// memory_copy.wast:365
assert_return(() => call($9, "load8_u", [13]), 13);

// memory_copy.wast:366
assert_return(() => call($9, "load8_u", [14]), 14);

// memory_copy.wast:367
assert_return(() => call($9, "load8_u", [15]), 15);

// memory_copy.wast:368
assert_return(() => call($9, "load8_u", [16]), 16);

// memory_copy.wast:369
assert_return(() => call($9, "load8_u", [17]), 17);

// memory_copy.wast:370
assert_return(() => call($9, "load8_u", [18]), 18);

// memory_copy.wast:371
assert_return(() => call($9, "load8_u", [19]), 19);

// memory_copy.wast:372
assert_return(() => call($9, "load8_u", [218]), 0);

// memory_copy.wast:373
assert_return(() => call($9, "load8_u", [417]), 0);

// memory_copy.wast:374
assert_return(() => call($9, "load8_u", [616]), 0);

// memory_copy.wast:375
assert_return(() => call($9, "load8_u", [815]), 0);

// memory_copy.wast:376
assert_return(() => call($9, "load8_u", [1014]), 0);

// memory_copy.wast:377
assert_return(() => call($9, "load8_u", [1213]), 0);

// memory_copy.wast:378
assert_return(() => call($9, "load8_u", [1412]), 0);

// memory_copy.wast:379
assert_return(() => call($9, "load8_u", [1611]), 0);

// memory_copy.wast:380
assert_return(() => call($9, "load8_u", [1810]), 0);

// memory_copy.wast:381
assert_return(() => call($9, "load8_u", [2009]), 0);

// memory_copy.wast:382
assert_return(() => call($9, "load8_u", [2208]), 0);

// memory_copy.wast:383
assert_return(() => call($9, "load8_u", [2407]), 0);

// memory_copy.wast:384
assert_return(() => call($9, "load8_u", [2606]), 0);

// memory_copy.wast:385
assert_return(() => call($9, "load8_u", [2805]), 0);

// memory_copy.wast:386
assert_return(() => call($9, "load8_u", [3004]), 0);

// memory_copy.wast:387
assert_return(() => call($9, "load8_u", [3203]), 0);

// memory_copy.wast:388
assert_return(() => call($9, "load8_u", [3402]), 0);

// memory_copy.wast:389
assert_return(() => call($9, "load8_u", [3601]), 0);

// memory_copy.wast:390
assert_return(() => call($9, "load8_u", [3800]), 0);

// memory_copy.wast:391
assert_return(() => call($9, "load8_u", [3999]), 0);

// memory_copy.wast:392
assert_return(() => call($9, "load8_u", [4198]), 0);

// memory_copy.wast:393
assert_return(() => call($9, "load8_u", [4397]), 0);

// memory_copy.wast:394
assert_return(() => call($9, "load8_u", [4596]), 0);

// memory_copy.wast:395
assert_return(() => call($9, "load8_u", [4795]), 0);

// memory_copy.wast:396
assert_return(() => call($9, "load8_u", [4994]), 0);

// memory_copy.wast:397
assert_return(() => call($9, "load8_u", [5193]), 0);

// memory_copy.wast:398
assert_return(() => call($9, "load8_u", [5392]), 0);

// memory_copy.wast:399
assert_return(() => call($9, "load8_u", [5591]), 0);

// memory_copy.wast:400
assert_return(() => call($9, "load8_u", [5790]), 0);

// memory_copy.wast:401
assert_return(() => call($9, "load8_u", [5989]), 0);

// memory_copy.wast:402
assert_return(() => call($9, "load8_u", [6188]), 0);

// memory_copy.wast:403
assert_return(() => call($9, "load8_u", [6387]), 0);

// memory_copy.wast:404
assert_return(() => call($9, "load8_u", [6586]), 0);

// memory_copy.wast:405
assert_return(() => call($9, "load8_u", [6785]), 0);

// memory_copy.wast:406
assert_return(() => call($9, "load8_u", [6984]), 0);

// memory_copy.wast:407
assert_return(() => call($9, "load8_u", [7183]), 0);

// memory_copy.wast:408
assert_return(() => call($9, "load8_u", [7382]), 0);

// memory_copy.wast:409
assert_return(() => call($9, "load8_u", [7581]), 0);

// memory_copy.wast:410
assert_return(() => call($9, "load8_u", [7780]), 0);

// memory_copy.wast:411
assert_return(() => call($9, "load8_u", [7979]), 0);

// memory_copy.wast:412
assert_return(() => call($9, "load8_u", [8178]), 0);

// memory_copy.wast:413
assert_return(() => call($9, "load8_u", [8377]), 0);

// memory_copy.wast:414
assert_return(() => call($9, "load8_u", [8576]), 0);

// memory_copy.wast:415
assert_return(() => call($9, "load8_u", [8775]), 0);

// memory_copy.wast:416
assert_return(() => call($9, "load8_u", [8974]), 0);

// memory_copy.wast:417
assert_return(() => call($9, "load8_u", [9173]), 0);

// memory_copy.wast:418
assert_return(() => call($9, "load8_u", [9372]), 0);

// memory_copy.wast:419
assert_return(() => call($9, "load8_u", [9571]), 0);

// memory_copy.wast:420
assert_return(() => call($9, "load8_u", [9770]), 0);

// memory_copy.wast:421
assert_return(() => call($9, "load8_u", [9969]), 0);

// memory_copy.wast:422
assert_return(() => call($9, "load8_u", [10168]), 0);

// memory_copy.wast:423
assert_return(() => call($9, "load8_u", [10367]), 0);

// memory_copy.wast:424
assert_return(() => call($9, "load8_u", [10566]), 0);

// memory_copy.wast:425
assert_return(() => call($9, "load8_u", [10765]), 0);

// memory_copy.wast:426
assert_return(() => call($9, "load8_u", [10964]), 0);

// memory_copy.wast:427
assert_return(() => call($9, "load8_u", [11163]), 0);

// memory_copy.wast:428
assert_return(() => call($9, "load8_u", [11362]), 0);

// memory_copy.wast:429
assert_return(() => call($9, "load8_u", [11561]), 0);

// memory_copy.wast:430
assert_return(() => call($9, "load8_u", [11760]), 0);

// memory_copy.wast:431
assert_return(() => call($9, "load8_u", [11959]), 0);

// memory_copy.wast:432
assert_return(() => call($9, "load8_u", [12158]), 0);

// memory_copy.wast:433
assert_return(() => call($9, "load8_u", [12357]), 0);

// memory_copy.wast:434
assert_return(() => call($9, "load8_u", [12556]), 0);

// memory_copy.wast:435
assert_return(() => call($9, "load8_u", [12755]), 0);

// memory_copy.wast:436
assert_return(() => call($9, "load8_u", [12954]), 0);

// memory_copy.wast:437
assert_return(() => call($9, "load8_u", [13153]), 0);

// memory_copy.wast:438
assert_return(() => call($9, "load8_u", [13352]), 0);

// memory_copy.wast:439
assert_return(() => call($9, "load8_u", [13551]), 0);

// memory_copy.wast:440
assert_return(() => call($9, "load8_u", [13750]), 0);

// memory_copy.wast:441
assert_return(() => call($9, "load8_u", [13949]), 0);

// memory_copy.wast:442
assert_return(() => call($9, "load8_u", [14148]), 0);

// memory_copy.wast:443
assert_return(() => call($9, "load8_u", [14347]), 0);

// memory_copy.wast:444
assert_return(() => call($9, "load8_u", [14546]), 0);

// memory_copy.wast:445
assert_return(() => call($9, "load8_u", [14745]), 0);

// memory_copy.wast:446
assert_return(() => call($9, "load8_u", [14944]), 0);

// memory_copy.wast:447
assert_return(() => call($9, "load8_u", [15143]), 0);

// memory_copy.wast:448
assert_return(() => call($9, "load8_u", [15342]), 0);

// memory_copy.wast:449
assert_return(() => call($9, "load8_u", [15541]), 0);

// memory_copy.wast:450
assert_return(() => call($9, "load8_u", [15740]), 0);

// memory_copy.wast:451
assert_return(() => call($9, "load8_u", [15939]), 0);

// memory_copy.wast:452
assert_return(() => call($9, "load8_u", [16138]), 0);

// memory_copy.wast:453
assert_return(() => call($9, "load8_u", [16337]), 0);

// memory_copy.wast:454
assert_return(() => call($9, "load8_u", [16536]), 0);

// memory_copy.wast:455
assert_return(() => call($9, "load8_u", [16735]), 0);

// memory_copy.wast:456
assert_return(() => call($9, "load8_u", [16934]), 0);

// memory_copy.wast:457
assert_return(() => call($9, "load8_u", [17133]), 0);

// memory_copy.wast:458
assert_return(() => call($9, "load8_u", [17332]), 0);

// memory_copy.wast:459
assert_return(() => call($9, "load8_u", [17531]), 0);

// memory_copy.wast:460
assert_return(() => call($9, "load8_u", [17730]), 0);

// memory_copy.wast:461
assert_return(() => call($9, "load8_u", [17929]), 0);

// memory_copy.wast:462
assert_return(() => call($9, "load8_u", [18128]), 0);

// memory_copy.wast:463
assert_return(() => call($9, "load8_u", [18327]), 0);

// memory_copy.wast:464
assert_return(() => call($9, "load8_u", [18526]), 0);

// memory_copy.wast:465
assert_return(() => call($9, "load8_u", [18725]), 0);

// memory_copy.wast:466
assert_return(() => call($9, "load8_u", [18924]), 0);

// memory_copy.wast:467
assert_return(() => call($9, "load8_u", [19123]), 0);

// memory_copy.wast:468
assert_return(() => call($9, "load8_u", [19322]), 0);

// memory_copy.wast:469
assert_return(() => call($9, "load8_u", [19521]), 0);

// memory_copy.wast:470
assert_return(() => call($9, "load8_u", [19720]), 0);

// memory_copy.wast:471
assert_return(() => call($9, "load8_u", [19919]), 0);

// memory_copy.wast:472
assert_return(() => call($9, "load8_u", [20118]), 0);

// memory_copy.wast:473
assert_return(() => call($9, "load8_u", [20317]), 0);

// memory_copy.wast:474
assert_return(() => call($9, "load8_u", [20516]), 0);

// memory_copy.wast:475
assert_return(() => call($9, "load8_u", [20715]), 0);

// memory_copy.wast:476
assert_return(() => call($9, "load8_u", [20914]), 0);

// memory_copy.wast:477
assert_return(() => call($9, "load8_u", [21113]), 0);

// memory_copy.wast:478
assert_return(() => call($9, "load8_u", [21312]), 0);

// memory_copy.wast:479
assert_return(() => call($9, "load8_u", [21511]), 0);

// memory_copy.wast:480
assert_return(() => call($9, "load8_u", [21710]), 0);

// memory_copy.wast:481
assert_return(() => call($9, "load8_u", [21909]), 0);

// memory_copy.wast:482
assert_return(() => call($9, "load8_u", [22108]), 0);

// memory_copy.wast:483
assert_return(() => call($9, "load8_u", [22307]), 0);

// memory_copy.wast:484
assert_return(() => call($9, "load8_u", [22506]), 0);

// memory_copy.wast:485
assert_return(() => call($9, "load8_u", [22705]), 0);

// memory_copy.wast:486
assert_return(() => call($9, "load8_u", [22904]), 0);

// memory_copy.wast:487
assert_return(() => call($9, "load8_u", [23103]), 0);

// memory_copy.wast:488
assert_return(() => call($9, "load8_u", [23302]), 0);

// memory_copy.wast:489
assert_return(() => call($9, "load8_u", [23501]), 0);

// memory_copy.wast:490
assert_return(() => call($9, "load8_u", [23700]), 0);

// memory_copy.wast:491
assert_return(() => call($9, "load8_u", [23899]), 0);

// memory_copy.wast:492
assert_return(() => call($9, "load8_u", [24098]), 0);

// memory_copy.wast:493
assert_return(() => call($9, "load8_u", [24297]), 0);

// memory_copy.wast:494
assert_return(() => call($9, "load8_u", [24496]), 0);

// memory_copy.wast:495
assert_return(() => call($9, "load8_u", [24695]), 0);

// memory_copy.wast:496
assert_return(() => call($9, "load8_u", [24894]), 0);

// memory_copy.wast:497
assert_return(() => call($9, "load8_u", [25093]), 0);

// memory_copy.wast:498
assert_return(() => call($9, "load8_u", [25292]), 0);

// memory_copy.wast:499
assert_return(() => call($9, "load8_u", [25491]), 0);

// memory_copy.wast:500
assert_return(() => call($9, "load8_u", [25690]), 0);

// memory_copy.wast:501
assert_return(() => call($9, "load8_u", [25889]), 0);

// memory_copy.wast:502
assert_return(() => call($9, "load8_u", [26088]), 0);

// memory_copy.wast:503
assert_return(() => call($9, "load8_u", [26287]), 0);

// memory_copy.wast:504
assert_return(() => call($9, "load8_u", [26486]), 0);

// memory_copy.wast:505
assert_return(() => call($9, "load8_u", [26685]), 0);

// memory_copy.wast:506
assert_return(() => call($9, "load8_u", [26884]), 0);

// memory_copy.wast:507
assert_return(() => call($9, "load8_u", [27083]), 0);

// memory_copy.wast:508
assert_return(() => call($9, "load8_u", [27282]), 0);

// memory_copy.wast:509
assert_return(() => call($9, "load8_u", [27481]), 0);

// memory_copy.wast:510
assert_return(() => call($9, "load8_u", [27680]), 0);

// memory_copy.wast:511
assert_return(() => call($9, "load8_u", [27879]), 0);

// memory_copy.wast:512
assert_return(() => call($9, "load8_u", [28078]), 0);

// memory_copy.wast:513
assert_return(() => call($9, "load8_u", [28277]), 0);

// memory_copy.wast:514
assert_return(() => call($9, "load8_u", [28476]), 0);

// memory_copy.wast:515
assert_return(() => call($9, "load8_u", [28675]), 0);

// memory_copy.wast:516
assert_return(() => call($9, "load8_u", [28874]), 0);

// memory_copy.wast:517
assert_return(() => call($9, "load8_u", [29073]), 0);

// memory_copy.wast:518
assert_return(() => call($9, "load8_u", [29272]), 0);

// memory_copy.wast:519
assert_return(() => call($9, "load8_u", [29471]), 0);

// memory_copy.wast:520
assert_return(() => call($9, "load8_u", [29670]), 0);

// memory_copy.wast:521
assert_return(() => call($9, "load8_u", [29869]), 0);

// memory_copy.wast:522
assert_return(() => call($9, "load8_u", [30068]), 0);

// memory_copy.wast:523
assert_return(() => call($9, "load8_u", [30267]), 0);

// memory_copy.wast:524
assert_return(() => call($9, "load8_u", [30466]), 0);

// memory_copy.wast:525
assert_return(() => call($9, "load8_u", [30665]), 0);

// memory_copy.wast:526
assert_return(() => call($9, "load8_u", [30864]), 0);

// memory_copy.wast:527
assert_return(() => call($9, "load8_u", [31063]), 0);

// memory_copy.wast:528
assert_return(() => call($9, "load8_u", [31262]), 0);

// memory_copy.wast:529
assert_return(() => call($9, "load8_u", [31461]), 0);

// memory_copy.wast:530
assert_return(() => call($9, "load8_u", [31660]), 0);

// memory_copy.wast:531
assert_return(() => call($9, "load8_u", [31859]), 0);

// memory_copy.wast:532
assert_return(() => call($9, "load8_u", [32058]), 0);

// memory_copy.wast:533
assert_return(() => call($9, "load8_u", [32257]), 0);

// memory_copy.wast:534
assert_return(() => call($9, "load8_u", [32456]), 0);

// memory_copy.wast:535
assert_return(() => call($9, "load8_u", [32655]), 0);

// memory_copy.wast:536
assert_return(() => call($9, "load8_u", [32854]), 0);

// memory_copy.wast:537
assert_return(() => call($9, "load8_u", [33053]), 0);

// memory_copy.wast:538
assert_return(() => call($9, "load8_u", [33252]), 0);

// memory_copy.wast:539
assert_return(() => call($9, "load8_u", [33451]), 0);

// memory_copy.wast:540
assert_return(() => call($9, "load8_u", [33650]), 0);

// memory_copy.wast:541
assert_return(() => call($9, "load8_u", [33849]), 0);

// memory_copy.wast:542
assert_return(() => call($9, "load8_u", [34048]), 0);

// memory_copy.wast:543
assert_return(() => call($9, "load8_u", [34247]), 0);

// memory_copy.wast:544
assert_return(() => call($9, "load8_u", [34446]), 0);

// memory_copy.wast:545
assert_return(() => call($9, "load8_u", [34645]), 0);

// memory_copy.wast:546
assert_return(() => call($9, "load8_u", [34844]), 0);

// memory_copy.wast:547
assert_return(() => call($9, "load8_u", [35043]), 0);

// memory_copy.wast:548
assert_return(() => call($9, "load8_u", [35242]), 0);

// memory_copy.wast:549
assert_return(() => call($9, "load8_u", [35441]), 0);

// memory_copy.wast:550
assert_return(() => call($9, "load8_u", [35640]), 0);

// memory_copy.wast:551
assert_return(() => call($9, "load8_u", [35839]), 0);

// memory_copy.wast:552
assert_return(() => call($9, "load8_u", [36038]), 0);

// memory_copy.wast:553
assert_return(() => call($9, "load8_u", [36237]), 0);

// memory_copy.wast:554
assert_return(() => call($9, "load8_u", [36436]), 0);

// memory_copy.wast:555
assert_return(() => call($9, "load8_u", [36635]), 0);

// memory_copy.wast:556
assert_return(() => call($9, "load8_u", [36834]), 0);

// memory_copy.wast:557
assert_return(() => call($9, "load8_u", [37033]), 0);

// memory_copy.wast:558
assert_return(() => call($9, "load8_u", [37232]), 0);

// memory_copy.wast:559
assert_return(() => call($9, "load8_u", [37431]), 0);

// memory_copy.wast:560
assert_return(() => call($9, "load8_u", [37630]), 0);

// memory_copy.wast:561
assert_return(() => call($9, "load8_u", [37829]), 0);

// memory_copy.wast:562
assert_return(() => call($9, "load8_u", [38028]), 0);

// memory_copy.wast:563
assert_return(() => call($9, "load8_u", [38227]), 0);

// memory_copy.wast:564
assert_return(() => call($9, "load8_u", [38426]), 0);

// memory_copy.wast:565
assert_return(() => call($9, "load8_u", [38625]), 0);

// memory_copy.wast:566
assert_return(() => call($9, "load8_u", [38824]), 0);

// memory_copy.wast:567
assert_return(() => call($9, "load8_u", [39023]), 0);

// memory_copy.wast:568
assert_return(() => call($9, "load8_u", [39222]), 0);

// memory_copy.wast:569
assert_return(() => call($9, "load8_u", [39421]), 0);

// memory_copy.wast:570
assert_return(() => call($9, "load8_u", [39620]), 0);

// memory_copy.wast:571
assert_return(() => call($9, "load8_u", [39819]), 0);

// memory_copy.wast:572
assert_return(() => call($9, "load8_u", [40018]), 0);

// memory_copy.wast:573
assert_return(() => call($9, "load8_u", [40217]), 0);

// memory_copy.wast:574
assert_return(() => call($9, "load8_u", [40416]), 0);

// memory_copy.wast:575
assert_return(() => call($9, "load8_u", [40615]), 0);

// memory_copy.wast:576
assert_return(() => call($9, "load8_u", [40814]), 0);

// memory_copy.wast:577
assert_return(() => call($9, "load8_u", [41013]), 0);

// memory_copy.wast:578
assert_return(() => call($9, "load8_u", [41212]), 0);

// memory_copy.wast:579
assert_return(() => call($9, "load8_u", [41411]), 0);

// memory_copy.wast:580
assert_return(() => call($9, "load8_u", [41610]), 0);

// memory_copy.wast:581
assert_return(() => call($9, "load8_u", [41809]), 0);

// memory_copy.wast:582
assert_return(() => call($9, "load8_u", [42008]), 0);

// memory_copy.wast:583
assert_return(() => call($9, "load8_u", [42207]), 0);

// memory_copy.wast:584
assert_return(() => call($9, "load8_u", [42406]), 0);

// memory_copy.wast:585
assert_return(() => call($9, "load8_u", [42605]), 0);

// memory_copy.wast:586
assert_return(() => call($9, "load8_u", [42804]), 0);

// memory_copy.wast:587
assert_return(() => call($9, "load8_u", [43003]), 0);

// memory_copy.wast:588
assert_return(() => call($9, "load8_u", [43202]), 0);

// memory_copy.wast:589
assert_return(() => call($9, "load8_u", [43401]), 0);

// memory_copy.wast:590
assert_return(() => call($9, "load8_u", [43600]), 0);

// memory_copy.wast:591
assert_return(() => call($9, "load8_u", [43799]), 0);

// memory_copy.wast:592
assert_return(() => call($9, "load8_u", [43998]), 0);

// memory_copy.wast:593
assert_return(() => call($9, "load8_u", [44197]), 0);

// memory_copy.wast:594
assert_return(() => call($9, "load8_u", [44396]), 0);

// memory_copy.wast:595
assert_return(() => call($9, "load8_u", [44595]), 0);

// memory_copy.wast:596
assert_return(() => call($9, "load8_u", [44794]), 0);

// memory_copy.wast:597
assert_return(() => call($9, "load8_u", [44993]), 0);

// memory_copy.wast:598
assert_return(() => call($9, "load8_u", [45192]), 0);

// memory_copy.wast:599
assert_return(() => call($9, "load8_u", [45391]), 0);

// memory_copy.wast:600
assert_return(() => call($9, "load8_u", [45590]), 0);

// memory_copy.wast:601
assert_return(() => call($9, "load8_u", [45789]), 0);

// memory_copy.wast:602
assert_return(() => call($9, "load8_u", [45988]), 0);

// memory_copy.wast:603
assert_return(() => call($9, "load8_u", [46187]), 0);

// memory_copy.wast:604
assert_return(() => call($9, "load8_u", [46386]), 0);

// memory_copy.wast:605
assert_return(() => call($9, "load8_u", [46585]), 0);

// memory_copy.wast:606
assert_return(() => call($9, "load8_u", [46784]), 0);

// memory_copy.wast:607
assert_return(() => call($9, "load8_u", [46983]), 0);

// memory_copy.wast:608
assert_return(() => call($9, "load8_u", [47182]), 0);

// memory_copy.wast:609
assert_return(() => call($9, "load8_u", [47381]), 0);

// memory_copy.wast:610
assert_return(() => call($9, "load8_u", [47580]), 0);

// memory_copy.wast:611
assert_return(() => call($9, "load8_u", [47779]), 0);

// memory_copy.wast:612
assert_return(() => call($9, "load8_u", [47978]), 0);

// memory_copy.wast:613
assert_return(() => call($9, "load8_u", [48177]), 0);

// memory_copy.wast:614
assert_return(() => call($9, "load8_u", [48376]), 0);

// memory_copy.wast:615
assert_return(() => call($9, "load8_u", [48575]), 0);

// memory_copy.wast:616
assert_return(() => call($9, "load8_u", [48774]), 0);

// memory_copy.wast:617
assert_return(() => call($9, "load8_u", [48973]), 0);

// memory_copy.wast:618
assert_return(() => call($9, "load8_u", [49172]), 0);

// memory_copy.wast:619
assert_return(() => call($9, "load8_u", [49371]), 0);

// memory_copy.wast:620
assert_return(() => call($9, "load8_u", [49570]), 0);

// memory_copy.wast:621
assert_return(() => call($9, "load8_u", [49769]), 0);

// memory_copy.wast:622
assert_return(() => call($9, "load8_u", [49968]), 0);

// memory_copy.wast:623
assert_return(() => call($9, "load8_u", [50167]), 0);

// memory_copy.wast:624
assert_return(() => call($9, "load8_u", [50366]), 0);

// memory_copy.wast:625
assert_return(() => call($9, "load8_u", [50565]), 0);

// memory_copy.wast:626
assert_return(() => call($9, "load8_u", [50764]), 0);

// memory_copy.wast:627
assert_return(() => call($9, "load8_u", [50963]), 0);

// memory_copy.wast:628
assert_return(() => call($9, "load8_u", [51162]), 0);

// memory_copy.wast:629
assert_return(() => call($9, "load8_u", [51361]), 0);

// memory_copy.wast:630
assert_return(() => call($9, "load8_u", [51560]), 0);

// memory_copy.wast:631
assert_return(() => call($9, "load8_u", [51759]), 0);

// memory_copy.wast:632
assert_return(() => call($9, "load8_u", [51958]), 0);

// memory_copy.wast:633
assert_return(() => call($9, "load8_u", [52157]), 0);

// memory_copy.wast:634
assert_return(() => call($9, "load8_u", [52356]), 0);

// memory_copy.wast:635
assert_return(() => call($9, "load8_u", [52555]), 0);

// memory_copy.wast:636
assert_return(() => call($9, "load8_u", [52754]), 0);

// memory_copy.wast:637
assert_return(() => call($9, "load8_u", [52953]), 0);

// memory_copy.wast:638
assert_return(() => call($9, "load8_u", [53152]), 0);

// memory_copy.wast:639
assert_return(() => call($9, "load8_u", [53351]), 0);

// memory_copy.wast:640
assert_return(() => call($9, "load8_u", [53550]), 0);

// memory_copy.wast:641
assert_return(() => call($9, "load8_u", [53749]), 0);

// memory_copy.wast:642
assert_return(() => call($9, "load8_u", [53948]), 0);

// memory_copy.wast:643
assert_return(() => call($9, "load8_u", [54147]), 0);

// memory_copy.wast:644
assert_return(() => call($9, "load8_u", [54346]), 0);

// memory_copy.wast:645
assert_return(() => call($9, "load8_u", [54545]), 0);

// memory_copy.wast:646
assert_return(() => call($9, "load8_u", [54744]), 0);

// memory_copy.wast:647
assert_return(() => call($9, "load8_u", [54943]), 0);

// memory_copy.wast:648
assert_return(() => call($9, "load8_u", [55142]), 0);

// memory_copy.wast:649
assert_return(() => call($9, "load8_u", [55341]), 0);

// memory_copy.wast:650
assert_return(() => call($9, "load8_u", [55540]), 0);

// memory_copy.wast:651
assert_return(() => call($9, "load8_u", [55739]), 0);

// memory_copy.wast:652
assert_return(() => call($9, "load8_u", [55938]), 0);

// memory_copy.wast:653
assert_return(() => call($9, "load8_u", [56137]), 0);

// memory_copy.wast:654
assert_return(() => call($9, "load8_u", [56336]), 0);

// memory_copy.wast:655
assert_return(() => call($9, "load8_u", [56535]), 0);

// memory_copy.wast:656
assert_return(() => call($9, "load8_u", [56734]), 0);

// memory_copy.wast:657
assert_return(() => call($9, "load8_u", [56933]), 0);

// memory_copy.wast:658
assert_return(() => call($9, "load8_u", [57132]), 0);

// memory_copy.wast:659
assert_return(() => call($9, "load8_u", [57331]), 0);

// memory_copy.wast:660
assert_return(() => call($9, "load8_u", [57530]), 0);

// memory_copy.wast:661
assert_return(() => call($9, "load8_u", [57729]), 0);

// memory_copy.wast:662
assert_return(() => call($9, "load8_u", [57928]), 0);

// memory_copy.wast:663
assert_return(() => call($9, "load8_u", [58127]), 0);

// memory_copy.wast:664
assert_return(() => call($9, "load8_u", [58326]), 0);

// memory_copy.wast:665
assert_return(() => call($9, "load8_u", [58525]), 0);

// memory_copy.wast:666
assert_return(() => call($9, "load8_u", [58724]), 0);

// memory_copy.wast:667
assert_return(() => call($9, "load8_u", [58923]), 0);

// memory_copy.wast:668
assert_return(() => call($9, "load8_u", [59122]), 0);

// memory_copy.wast:669
assert_return(() => call($9, "load8_u", [59321]), 0);

// memory_copy.wast:670
assert_return(() => call($9, "load8_u", [59520]), 0);

// memory_copy.wast:671
assert_return(() => call($9, "load8_u", [59719]), 0);

// memory_copy.wast:672
assert_return(() => call($9, "load8_u", [59918]), 0);

// memory_copy.wast:673
assert_return(() => call($9, "load8_u", [60117]), 0);

// memory_copy.wast:674
assert_return(() => call($9, "load8_u", [60316]), 0);

// memory_copy.wast:675
assert_return(() => call($9, "load8_u", [60515]), 0);

// memory_copy.wast:676
assert_return(() => call($9, "load8_u", [60714]), 0);

// memory_copy.wast:677
assert_return(() => call($9, "load8_u", [60913]), 0);

// memory_copy.wast:678
assert_return(() => call($9, "load8_u", [61112]), 0);

// memory_copy.wast:679
assert_return(() => call($9, "load8_u", [61311]), 0);

// memory_copy.wast:680
assert_return(() => call($9, "load8_u", [61510]), 0);

// memory_copy.wast:681
assert_return(() => call($9, "load8_u", [61709]), 0);

// memory_copy.wast:682
assert_return(() => call($9, "load8_u", [61908]), 0);

// memory_copy.wast:683
assert_return(() => call($9, "load8_u", [62107]), 0);

// memory_copy.wast:684
assert_return(() => call($9, "load8_u", [62306]), 0);

// memory_copy.wast:685
assert_return(() => call($9, "load8_u", [62505]), 0);

// memory_copy.wast:686
assert_return(() => call($9, "load8_u", [62704]), 0);

// memory_copy.wast:687
assert_return(() => call($9, "load8_u", [62903]), 0);

// memory_copy.wast:688
assert_return(() => call($9, "load8_u", [63102]), 0);

// memory_copy.wast:689
assert_return(() => call($9, "load8_u", [63301]), 0);

// memory_copy.wast:690
assert_return(() => call($9, "load8_u", [63500]), 0);

// memory_copy.wast:691
assert_return(() => call($9, "load8_u", [63699]), 0);

// memory_copy.wast:692
assert_return(() => call($9, "load8_u", [63898]), 0);

// memory_copy.wast:693
assert_return(() => call($9, "load8_u", [64097]), 0);

// memory_copy.wast:694
assert_return(() => call($9, "load8_u", [64296]), 0);

// memory_copy.wast:695
assert_return(() => call($9, "load8_u", [64495]), 0);

// memory_copy.wast:696
assert_return(() => call($9, "load8_u", [64694]), 0);

// memory_copy.wast:697
assert_return(() => call($9, "load8_u", [64893]), 0);

// memory_copy.wast:698
assert_return(() => call($9, "load8_u", [65092]), 0);

// memory_copy.wast:699
assert_return(() => call($9, "load8_u", [65291]), 0);

// memory_copy.wast:700
assert_return(() => call($9, "load8_u", [65490]), 0);

// memory_copy.wast:701
assert_return(() => call($9, "load8_u", [65516]), 0);

// memory_copy.wast:702
assert_return(() => call($9, "load8_u", [65517]), 1);

// memory_copy.wast:703
assert_return(() => call($9, "load8_u", [65518]), 2);

// memory_copy.wast:704
assert_return(() => call($9, "load8_u", [65519]), 3);

// memory_copy.wast:705
assert_return(() => call($9, "load8_u", [65520]), 4);

// memory_copy.wast:706
assert_return(() => call($9, "load8_u", [65521]), 5);

// memory_copy.wast:707
assert_return(() => call($9, "load8_u", [65522]), 6);

// memory_copy.wast:708
assert_return(() => call($9, "load8_u", [65523]), 7);

// memory_copy.wast:709
assert_return(() => call($9, "load8_u", [65524]), 8);

// memory_copy.wast:710
assert_return(() => call($9, "load8_u", [65525]), 9);

// memory_copy.wast:711
assert_return(() => call($9, "load8_u", [65526]), 10);

// memory_copy.wast:712
assert_return(() => call($9, "load8_u", [65527]), 11);

// memory_copy.wast:713
assert_return(() => call($9, "load8_u", [65528]), 12);

// memory_copy.wast:714
assert_return(() => call($9, "load8_u", [65529]), 13);

// memory_copy.wast:715
assert_return(() => call($9, "load8_u", [65530]), 14);

// memory_copy.wast:716
assert_return(() => call($9, "load8_u", [65531]), 15);

// memory_copy.wast:717
assert_return(() => call($9, "load8_u", [65532]), 16);

// memory_copy.wast:718
assert_return(() => call($9, "load8_u", [65533]), 17);

// memory_copy.wast:719
assert_return(() => call($9, "load8_u", [65534]), 18);

// memory_copy.wast:720
assert_return(() => call($9, "load8_u", [65535]), 19);

// memory_copy.wast:722
let $10 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9b\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x15\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14");

// memory_copy.wast:730
assert_trap(() => call($10, "run", [65515, 0, 39]));

// memory_copy.wast:733
assert_return(() => call($10, "load8_u", [0]), 0);

// memory_copy.wast:734
assert_return(() => call($10, "load8_u", [1]), 1);

// memory_copy.wast:735
assert_return(() => call($10, "load8_u", [2]), 2);

// memory_copy.wast:736
assert_return(() => call($10, "load8_u", [3]), 3);

// memory_copy.wast:737
assert_return(() => call($10, "load8_u", [4]), 4);

// memory_copy.wast:738
assert_return(() => call($10, "load8_u", [5]), 5);

// memory_copy.wast:739
assert_return(() => call($10, "load8_u", [6]), 6);

// memory_copy.wast:740
assert_return(() => call($10, "load8_u", [7]), 7);

// memory_copy.wast:741
assert_return(() => call($10, "load8_u", [8]), 8);

// memory_copy.wast:742
assert_return(() => call($10, "load8_u", [9]), 9);

// memory_copy.wast:743
assert_return(() => call($10, "load8_u", [10]), 10);

// memory_copy.wast:744
assert_return(() => call($10, "load8_u", [11]), 11);

// memory_copy.wast:745
assert_return(() => call($10, "load8_u", [12]), 12);

// memory_copy.wast:746
assert_return(() => call($10, "load8_u", [13]), 13);

// memory_copy.wast:747
assert_return(() => call($10, "load8_u", [14]), 14);

// memory_copy.wast:748
assert_return(() => call($10, "load8_u", [15]), 15);

// memory_copy.wast:749
assert_return(() => call($10, "load8_u", [16]), 16);

// memory_copy.wast:750
assert_return(() => call($10, "load8_u", [17]), 17);

// memory_copy.wast:751
assert_return(() => call($10, "load8_u", [18]), 18);

// memory_copy.wast:752
assert_return(() => call($10, "load8_u", [19]), 19);

// memory_copy.wast:753
assert_return(() => call($10, "load8_u", [20]), 20);

// memory_copy.wast:754
assert_return(() => call($10, "load8_u", [219]), 0);

// memory_copy.wast:755
assert_return(() => call($10, "load8_u", [418]), 0);

// memory_copy.wast:756
assert_return(() => call($10, "load8_u", [617]), 0);

// memory_copy.wast:757
assert_return(() => call($10, "load8_u", [816]), 0);

// memory_copy.wast:758
assert_return(() => call($10, "load8_u", [1015]), 0);

// memory_copy.wast:759
assert_return(() => call($10, "load8_u", [1214]), 0);

// memory_copy.wast:760
assert_return(() => call($10, "load8_u", [1413]), 0);

// memory_copy.wast:761
assert_return(() => call($10, "load8_u", [1612]), 0);

// memory_copy.wast:762
assert_return(() => call($10, "load8_u", [1811]), 0);

// memory_copy.wast:763
assert_return(() => call($10, "load8_u", [2010]), 0);

// memory_copy.wast:764
assert_return(() => call($10, "load8_u", [2209]), 0);

// memory_copy.wast:765
assert_return(() => call($10, "load8_u", [2408]), 0);

// memory_copy.wast:766
assert_return(() => call($10, "load8_u", [2607]), 0);

// memory_copy.wast:767
assert_return(() => call($10, "load8_u", [2806]), 0);

// memory_copy.wast:768
assert_return(() => call($10, "load8_u", [3005]), 0);

// memory_copy.wast:769
assert_return(() => call($10, "load8_u", [3204]), 0);

// memory_copy.wast:770
assert_return(() => call($10, "load8_u", [3403]), 0);

// memory_copy.wast:771
assert_return(() => call($10, "load8_u", [3602]), 0);

// memory_copy.wast:772
assert_return(() => call($10, "load8_u", [3801]), 0);

// memory_copy.wast:773
assert_return(() => call($10, "load8_u", [4000]), 0);

// memory_copy.wast:774
assert_return(() => call($10, "load8_u", [4199]), 0);

// memory_copy.wast:775
assert_return(() => call($10, "load8_u", [4398]), 0);

// memory_copy.wast:776
assert_return(() => call($10, "load8_u", [4597]), 0);

// memory_copy.wast:777
assert_return(() => call($10, "load8_u", [4796]), 0);

// memory_copy.wast:778
assert_return(() => call($10, "load8_u", [4995]), 0);

// memory_copy.wast:779
assert_return(() => call($10, "load8_u", [5194]), 0);

// memory_copy.wast:780
assert_return(() => call($10, "load8_u", [5393]), 0);

// memory_copy.wast:781
assert_return(() => call($10, "load8_u", [5592]), 0);

// memory_copy.wast:782
assert_return(() => call($10, "load8_u", [5791]), 0);

// memory_copy.wast:783
assert_return(() => call($10, "load8_u", [5990]), 0);

// memory_copy.wast:784
assert_return(() => call($10, "load8_u", [6189]), 0);

// memory_copy.wast:785
assert_return(() => call($10, "load8_u", [6388]), 0);

// memory_copy.wast:786
assert_return(() => call($10, "load8_u", [6587]), 0);

// memory_copy.wast:787
assert_return(() => call($10, "load8_u", [6786]), 0);

// memory_copy.wast:788
assert_return(() => call($10, "load8_u", [6985]), 0);

// memory_copy.wast:789
assert_return(() => call($10, "load8_u", [7184]), 0);

// memory_copy.wast:790
assert_return(() => call($10, "load8_u", [7383]), 0);

// memory_copy.wast:791
assert_return(() => call($10, "load8_u", [7582]), 0);

// memory_copy.wast:792
assert_return(() => call($10, "load8_u", [7781]), 0);

// memory_copy.wast:793
assert_return(() => call($10, "load8_u", [7980]), 0);

// memory_copy.wast:794
assert_return(() => call($10, "load8_u", [8179]), 0);

// memory_copy.wast:795
assert_return(() => call($10, "load8_u", [8378]), 0);

// memory_copy.wast:796
assert_return(() => call($10, "load8_u", [8577]), 0);

// memory_copy.wast:797
assert_return(() => call($10, "load8_u", [8776]), 0);

// memory_copy.wast:798
assert_return(() => call($10, "load8_u", [8975]), 0);

// memory_copy.wast:799
assert_return(() => call($10, "load8_u", [9174]), 0);

// memory_copy.wast:800
assert_return(() => call($10, "load8_u", [9373]), 0);

// memory_copy.wast:801
assert_return(() => call($10, "load8_u", [9572]), 0);

// memory_copy.wast:802
assert_return(() => call($10, "load8_u", [9771]), 0);

// memory_copy.wast:803
assert_return(() => call($10, "load8_u", [9970]), 0);

// memory_copy.wast:804
assert_return(() => call($10, "load8_u", [10169]), 0);

// memory_copy.wast:805
assert_return(() => call($10, "load8_u", [10368]), 0);

// memory_copy.wast:806
assert_return(() => call($10, "load8_u", [10567]), 0);

// memory_copy.wast:807
assert_return(() => call($10, "load8_u", [10766]), 0);

// memory_copy.wast:808
assert_return(() => call($10, "load8_u", [10965]), 0);

// memory_copy.wast:809
assert_return(() => call($10, "load8_u", [11164]), 0);

// memory_copy.wast:810
assert_return(() => call($10, "load8_u", [11363]), 0);

// memory_copy.wast:811
assert_return(() => call($10, "load8_u", [11562]), 0);

// memory_copy.wast:812
assert_return(() => call($10, "load8_u", [11761]), 0);

// memory_copy.wast:813
assert_return(() => call($10, "load8_u", [11960]), 0);

// memory_copy.wast:814
assert_return(() => call($10, "load8_u", [12159]), 0);

// memory_copy.wast:815
assert_return(() => call($10, "load8_u", [12358]), 0);

// memory_copy.wast:816
assert_return(() => call($10, "load8_u", [12557]), 0);

// memory_copy.wast:817
assert_return(() => call($10, "load8_u", [12756]), 0);

// memory_copy.wast:818
assert_return(() => call($10, "load8_u", [12955]), 0);

// memory_copy.wast:819
assert_return(() => call($10, "load8_u", [13154]), 0);

// memory_copy.wast:820
assert_return(() => call($10, "load8_u", [13353]), 0);

// memory_copy.wast:821
assert_return(() => call($10, "load8_u", [13552]), 0);

// memory_copy.wast:822
assert_return(() => call($10, "load8_u", [13751]), 0);

// memory_copy.wast:823
assert_return(() => call($10, "load8_u", [13950]), 0);

// memory_copy.wast:824
assert_return(() => call($10, "load8_u", [14149]), 0);

// memory_copy.wast:825
assert_return(() => call($10, "load8_u", [14348]), 0);

// memory_copy.wast:826
assert_return(() => call($10, "load8_u", [14547]), 0);

// memory_copy.wast:827
assert_return(() => call($10, "load8_u", [14746]), 0);

// memory_copy.wast:828
assert_return(() => call($10, "load8_u", [14945]), 0);

// memory_copy.wast:829
assert_return(() => call($10, "load8_u", [15144]), 0);

// memory_copy.wast:830
assert_return(() => call($10, "load8_u", [15343]), 0);

// memory_copy.wast:831
assert_return(() => call($10, "load8_u", [15542]), 0);

// memory_copy.wast:832
assert_return(() => call($10, "load8_u", [15741]), 0);

// memory_copy.wast:833
assert_return(() => call($10, "load8_u", [15940]), 0);

// memory_copy.wast:834
assert_return(() => call($10, "load8_u", [16139]), 0);

// memory_copy.wast:835
assert_return(() => call($10, "load8_u", [16338]), 0);

// memory_copy.wast:836
assert_return(() => call($10, "load8_u", [16537]), 0);

// memory_copy.wast:837
assert_return(() => call($10, "load8_u", [16736]), 0);

// memory_copy.wast:838
assert_return(() => call($10, "load8_u", [16935]), 0);

// memory_copy.wast:839
assert_return(() => call($10, "load8_u", [17134]), 0);

// memory_copy.wast:840
assert_return(() => call($10, "load8_u", [17333]), 0);

// memory_copy.wast:841
assert_return(() => call($10, "load8_u", [17532]), 0);

// memory_copy.wast:842
assert_return(() => call($10, "load8_u", [17731]), 0);

// memory_copy.wast:843
assert_return(() => call($10, "load8_u", [17930]), 0);

// memory_copy.wast:844
assert_return(() => call($10, "load8_u", [18129]), 0);

// memory_copy.wast:845
assert_return(() => call($10, "load8_u", [18328]), 0);

// memory_copy.wast:846
assert_return(() => call($10, "load8_u", [18527]), 0);

// memory_copy.wast:847
assert_return(() => call($10, "load8_u", [18726]), 0);

// memory_copy.wast:848
assert_return(() => call($10, "load8_u", [18925]), 0);

// memory_copy.wast:849
assert_return(() => call($10, "load8_u", [19124]), 0);

// memory_copy.wast:850
assert_return(() => call($10, "load8_u", [19323]), 0);

// memory_copy.wast:851
assert_return(() => call($10, "load8_u", [19522]), 0);

// memory_copy.wast:852
assert_return(() => call($10, "load8_u", [19721]), 0);

// memory_copy.wast:853
assert_return(() => call($10, "load8_u", [19920]), 0);

// memory_copy.wast:854
assert_return(() => call($10, "load8_u", [20119]), 0);

// memory_copy.wast:855
assert_return(() => call($10, "load8_u", [20318]), 0);

// memory_copy.wast:856
assert_return(() => call($10, "load8_u", [20517]), 0);

// memory_copy.wast:857
assert_return(() => call($10, "load8_u", [20716]), 0);

// memory_copy.wast:858
assert_return(() => call($10, "load8_u", [20915]), 0);

// memory_copy.wast:859
assert_return(() => call($10, "load8_u", [21114]), 0);

// memory_copy.wast:860
assert_return(() => call($10, "load8_u", [21313]), 0);

// memory_copy.wast:861
assert_return(() => call($10, "load8_u", [21512]), 0);

// memory_copy.wast:862
assert_return(() => call($10, "load8_u", [21711]), 0);

// memory_copy.wast:863
assert_return(() => call($10, "load8_u", [21910]), 0);

// memory_copy.wast:864
assert_return(() => call($10, "load8_u", [22109]), 0);

// memory_copy.wast:865
assert_return(() => call($10, "load8_u", [22308]), 0);

// memory_copy.wast:866
assert_return(() => call($10, "load8_u", [22507]), 0);

// memory_copy.wast:867
assert_return(() => call($10, "load8_u", [22706]), 0);

// memory_copy.wast:868
assert_return(() => call($10, "load8_u", [22905]), 0);

// memory_copy.wast:869
assert_return(() => call($10, "load8_u", [23104]), 0);

// memory_copy.wast:870
assert_return(() => call($10, "load8_u", [23303]), 0);

// memory_copy.wast:871
assert_return(() => call($10, "load8_u", [23502]), 0);

// memory_copy.wast:872
assert_return(() => call($10, "load8_u", [23701]), 0);

// memory_copy.wast:873
assert_return(() => call($10, "load8_u", [23900]), 0);

// memory_copy.wast:874
assert_return(() => call($10, "load8_u", [24099]), 0);

// memory_copy.wast:875
assert_return(() => call($10, "load8_u", [24298]), 0);

// memory_copy.wast:876
assert_return(() => call($10, "load8_u", [24497]), 0);

// memory_copy.wast:877
assert_return(() => call($10, "load8_u", [24696]), 0);

// memory_copy.wast:878
assert_return(() => call($10, "load8_u", [24895]), 0);

// memory_copy.wast:879
assert_return(() => call($10, "load8_u", [25094]), 0);

// memory_copy.wast:880
assert_return(() => call($10, "load8_u", [25293]), 0);

// memory_copy.wast:881
assert_return(() => call($10, "load8_u", [25492]), 0);

// memory_copy.wast:882
assert_return(() => call($10, "load8_u", [25691]), 0);

// memory_copy.wast:883
assert_return(() => call($10, "load8_u", [25890]), 0);

// memory_copy.wast:884
assert_return(() => call($10, "load8_u", [26089]), 0);

// memory_copy.wast:885
assert_return(() => call($10, "load8_u", [26288]), 0);

// memory_copy.wast:886
assert_return(() => call($10, "load8_u", [26487]), 0);

// memory_copy.wast:887
assert_return(() => call($10, "load8_u", [26686]), 0);

// memory_copy.wast:888
assert_return(() => call($10, "load8_u", [26885]), 0);

// memory_copy.wast:889
assert_return(() => call($10, "load8_u", [27084]), 0);

// memory_copy.wast:890
assert_return(() => call($10, "load8_u", [27283]), 0);

// memory_copy.wast:891
assert_return(() => call($10, "load8_u", [27482]), 0);

// memory_copy.wast:892
assert_return(() => call($10, "load8_u", [27681]), 0);

// memory_copy.wast:893
assert_return(() => call($10, "load8_u", [27880]), 0);

// memory_copy.wast:894
assert_return(() => call($10, "load8_u", [28079]), 0);

// memory_copy.wast:895
assert_return(() => call($10, "load8_u", [28278]), 0);

// memory_copy.wast:896
assert_return(() => call($10, "load8_u", [28477]), 0);

// memory_copy.wast:897
assert_return(() => call($10, "load8_u", [28676]), 0);

// memory_copy.wast:898
assert_return(() => call($10, "load8_u", [28875]), 0);

// memory_copy.wast:899
assert_return(() => call($10, "load8_u", [29074]), 0);

// memory_copy.wast:900
assert_return(() => call($10, "load8_u", [29273]), 0);

// memory_copy.wast:901
assert_return(() => call($10, "load8_u", [29472]), 0);

// memory_copy.wast:902
assert_return(() => call($10, "load8_u", [29671]), 0);

// memory_copy.wast:903
assert_return(() => call($10, "load8_u", [29870]), 0);

// memory_copy.wast:904
assert_return(() => call($10, "load8_u", [30069]), 0);

// memory_copy.wast:905
assert_return(() => call($10, "load8_u", [30268]), 0);

// memory_copy.wast:906
assert_return(() => call($10, "load8_u", [30467]), 0);

// memory_copy.wast:907
assert_return(() => call($10, "load8_u", [30666]), 0);

// memory_copy.wast:908
assert_return(() => call($10, "load8_u", [30865]), 0);

// memory_copy.wast:909
assert_return(() => call($10, "load8_u", [31064]), 0);

// memory_copy.wast:910
assert_return(() => call($10, "load8_u", [31263]), 0);

// memory_copy.wast:911
assert_return(() => call($10, "load8_u", [31462]), 0);

// memory_copy.wast:912
assert_return(() => call($10, "load8_u", [31661]), 0);

// memory_copy.wast:913
assert_return(() => call($10, "load8_u", [31860]), 0);

// memory_copy.wast:914
assert_return(() => call($10, "load8_u", [32059]), 0);

// memory_copy.wast:915
assert_return(() => call($10, "load8_u", [32258]), 0);

// memory_copy.wast:916
assert_return(() => call($10, "load8_u", [32457]), 0);

// memory_copy.wast:917
assert_return(() => call($10, "load8_u", [32656]), 0);

// memory_copy.wast:918
assert_return(() => call($10, "load8_u", [32855]), 0);

// memory_copy.wast:919
assert_return(() => call($10, "load8_u", [33054]), 0);

// memory_copy.wast:920
assert_return(() => call($10, "load8_u", [33253]), 0);

// memory_copy.wast:921
assert_return(() => call($10, "load8_u", [33452]), 0);

// memory_copy.wast:922
assert_return(() => call($10, "load8_u", [33651]), 0);

// memory_copy.wast:923
assert_return(() => call($10, "load8_u", [33850]), 0);

// memory_copy.wast:924
assert_return(() => call($10, "load8_u", [34049]), 0);

// memory_copy.wast:925
assert_return(() => call($10, "load8_u", [34248]), 0);

// memory_copy.wast:926
assert_return(() => call($10, "load8_u", [34447]), 0);

// memory_copy.wast:927
assert_return(() => call($10, "load8_u", [34646]), 0);

// memory_copy.wast:928
assert_return(() => call($10, "load8_u", [34845]), 0);

// memory_copy.wast:929
assert_return(() => call($10, "load8_u", [35044]), 0);

// memory_copy.wast:930
assert_return(() => call($10, "load8_u", [35243]), 0);

// memory_copy.wast:931
assert_return(() => call($10, "load8_u", [35442]), 0);

// memory_copy.wast:932
assert_return(() => call($10, "load8_u", [35641]), 0);

// memory_copy.wast:933
assert_return(() => call($10, "load8_u", [35840]), 0);

// memory_copy.wast:934
assert_return(() => call($10, "load8_u", [36039]), 0);

// memory_copy.wast:935
assert_return(() => call($10, "load8_u", [36238]), 0);

// memory_copy.wast:936
assert_return(() => call($10, "load8_u", [36437]), 0);

// memory_copy.wast:937
assert_return(() => call($10, "load8_u", [36636]), 0);

// memory_copy.wast:938
assert_return(() => call($10, "load8_u", [36835]), 0);

// memory_copy.wast:939
assert_return(() => call($10, "load8_u", [37034]), 0);

// memory_copy.wast:940
assert_return(() => call($10, "load8_u", [37233]), 0);

// memory_copy.wast:941
assert_return(() => call($10, "load8_u", [37432]), 0);

// memory_copy.wast:942
assert_return(() => call($10, "load8_u", [37631]), 0);

// memory_copy.wast:943
assert_return(() => call($10, "load8_u", [37830]), 0);

// memory_copy.wast:944
assert_return(() => call($10, "load8_u", [38029]), 0);

// memory_copy.wast:945
assert_return(() => call($10, "load8_u", [38228]), 0);

// memory_copy.wast:946
assert_return(() => call($10, "load8_u", [38427]), 0);

// memory_copy.wast:947
assert_return(() => call($10, "load8_u", [38626]), 0);

// memory_copy.wast:948
assert_return(() => call($10, "load8_u", [38825]), 0);

// memory_copy.wast:949
assert_return(() => call($10, "load8_u", [39024]), 0);

// memory_copy.wast:950
assert_return(() => call($10, "load8_u", [39223]), 0);

// memory_copy.wast:951
assert_return(() => call($10, "load8_u", [39422]), 0);

// memory_copy.wast:952
assert_return(() => call($10, "load8_u", [39621]), 0);

// memory_copy.wast:953
assert_return(() => call($10, "load8_u", [39820]), 0);

// memory_copy.wast:954
assert_return(() => call($10, "load8_u", [40019]), 0);

// memory_copy.wast:955
assert_return(() => call($10, "load8_u", [40218]), 0);

// memory_copy.wast:956
assert_return(() => call($10, "load8_u", [40417]), 0);

// memory_copy.wast:957
assert_return(() => call($10, "load8_u", [40616]), 0);

// memory_copy.wast:958
assert_return(() => call($10, "load8_u", [40815]), 0);

// memory_copy.wast:959
assert_return(() => call($10, "load8_u", [41014]), 0);

// memory_copy.wast:960
assert_return(() => call($10, "load8_u", [41213]), 0);

// memory_copy.wast:961
assert_return(() => call($10, "load8_u", [41412]), 0);

// memory_copy.wast:962
assert_return(() => call($10, "load8_u", [41611]), 0);

// memory_copy.wast:963
assert_return(() => call($10, "load8_u", [41810]), 0);

// memory_copy.wast:964
assert_return(() => call($10, "load8_u", [42009]), 0);

// memory_copy.wast:965
assert_return(() => call($10, "load8_u", [42208]), 0);

// memory_copy.wast:966
assert_return(() => call($10, "load8_u", [42407]), 0);

// memory_copy.wast:967
assert_return(() => call($10, "load8_u", [42606]), 0);

// memory_copy.wast:968
assert_return(() => call($10, "load8_u", [42805]), 0);

// memory_copy.wast:969
assert_return(() => call($10, "load8_u", [43004]), 0);

// memory_copy.wast:970
assert_return(() => call($10, "load8_u", [43203]), 0);

// memory_copy.wast:971
assert_return(() => call($10, "load8_u", [43402]), 0);

// memory_copy.wast:972
assert_return(() => call($10, "load8_u", [43601]), 0);

// memory_copy.wast:973
assert_return(() => call($10, "load8_u", [43800]), 0);

// memory_copy.wast:974
assert_return(() => call($10, "load8_u", [43999]), 0);

// memory_copy.wast:975
assert_return(() => call($10, "load8_u", [44198]), 0);

// memory_copy.wast:976
assert_return(() => call($10, "load8_u", [44397]), 0);

// memory_copy.wast:977
assert_return(() => call($10, "load8_u", [44596]), 0);

// memory_copy.wast:978
assert_return(() => call($10, "load8_u", [44795]), 0);

// memory_copy.wast:979
assert_return(() => call($10, "load8_u", [44994]), 0);

// memory_copy.wast:980
assert_return(() => call($10, "load8_u", [45193]), 0);

// memory_copy.wast:981
assert_return(() => call($10, "load8_u", [45392]), 0);

// memory_copy.wast:982
assert_return(() => call($10, "load8_u", [45591]), 0);

// memory_copy.wast:983
assert_return(() => call($10, "load8_u", [45790]), 0);

// memory_copy.wast:984
assert_return(() => call($10, "load8_u", [45989]), 0);

// memory_copy.wast:985
assert_return(() => call($10, "load8_u", [46188]), 0);

// memory_copy.wast:986
assert_return(() => call($10, "load8_u", [46387]), 0);

// memory_copy.wast:987
assert_return(() => call($10, "load8_u", [46586]), 0);

// memory_copy.wast:988
assert_return(() => call($10, "load8_u", [46785]), 0);

// memory_copy.wast:989
assert_return(() => call($10, "load8_u", [46984]), 0);

// memory_copy.wast:990
assert_return(() => call($10, "load8_u", [47183]), 0);

// memory_copy.wast:991
assert_return(() => call($10, "load8_u", [47382]), 0);

// memory_copy.wast:992
assert_return(() => call($10, "load8_u", [47581]), 0);

// memory_copy.wast:993
assert_return(() => call($10, "load8_u", [47780]), 0);

// memory_copy.wast:994
assert_return(() => call($10, "load8_u", [47979]), 0);

// memory_copy.wast:995
assert_return(() => call($10, "load8_u", [48178]), 0);

// memory_copy.wast:996
assert_return(() => call($10, "load8_u", [48377]), 0);

// memory_copy.wast:997
assert_return(() => call($10, "load8_u", [48576]), 0);

// memory_copy.wast:998
assert_return(() => call($10, "load8_u", [48775]), 0);

// memory_copy.wast:999
assert_return(() => call($10, "load8_u", [48974]), 0);

// memory_copy.wast:1000
assert_return(() => call($10, "load8_u", [49173]), 0);

// memory_copy.wast:1001
assert_return(() => call($10, "load8_u", [49372]), 0);

// memory_copy.wast:1002
assert_return(() => call($10, "load8_u", [49571]), 0);

// memory_copy.wast:1003
assert_return(() => call($10, "load8_u", [49770]), 0);

// memory_copy.wast:1004
assert_return(() => call($10, "load8_u", [49969]), 0);

// memory_copy.wast:1005
assert_return(() => call($10, "load8_u", [50168]), 0);

// memory_copy.wast:1006
assert_return(() => call($10, "load8_u", [50367]), 0);

// memory_copy.wast:1007
assert_return(() => call($10, "load8_u", [50566]), 0);

// memory_copy.wast:1008
assert_return(() => call($10, "load8_u", [50765]), 0);

// memory_copy.wast:1009
assert_return(() => call($10, "load8_u", [50964]), 0);

// memory_copy.wast:1010
assert_return(() => call($10, "load8_u", [51163]), 0);

// memory_copy.wast:1011
assert_return(() => call($10, "load8_u", [51362]), 0);

// memory_copy.wast:1012
assert_return(() => call($10, "load8_u", [51561]), 0);

// memory_copy.wast:1013
assert_return(() => call($10, "load8_u", [51760]), 0);

// memory_copy.wast:1014
assert_return(() => call($10, "load8_u", [51959]), 0);

// memory_copy.wast:1015
assert_return(() => call($10, "load8_u", [52158]), 0);

// memory_copy.wast:1016
assert_return(() => call($10, "load8_u", [52357]), 0);

// memory_copy.wast:1017
assert_return(() => call($10, "load8_u", [52556]), 0);

// memory_copy.wast:1018
assert_return(() => call($10, "load8_u", [52755]), 0);

// memory_copy.wast:1019
assert_return(() => call($10, "load8_u", [52954]), 0);

// memory_copy.wast:1020
assert_return(() => call($10, "load8_u", [53153]), 0);

// memory_copy.wast:1021
assert_return(() => call($10, "load8_u", [53352]), 0);

// memory_copy.wast:1022
assert_return(() => call($10, "load8_u", [53551]), 0);

// memory_copy.wast:1023
assert_return(() => call($10, "load8_u", [53750]), 0);

// memory_copy.wast:1024
assert_return(() => call($10, "load8_u", [53949]), 0);

// memory_copy.wast:1025
assert_return(() => call($10, "load8_u", [54148]), 0);

// memory_copy.wast:1026
assert_return(() => call($10, "load8_u", [54347]), 0);

// memory_copy.wast:1027
assert_return(() => call($10, "load8_u", [54546]), 0);

// memory_copy.wast:1028
assert_return(() => call($10, "load8_u", [54745]), 0);

// memory_copy.wast:1029
assert_return(() => call($10, "load8_u", [54944]), 0);

// memory_copy.wast:1030
assert_return(() => call($10, "load8_u", [55143]), 0);

// memory_copy.wast:1031
assert_return(() => call($10, "load8_u", [55342]), 0);

// memory_copy.wast:1032
assert_return(() => call($10, "load8_u", [55541]), 0);

// memory_copy.wast:1033
assert_return(() => call($10, "load8_u", [55740]), 0);

// memory_copy.wast:1034
assert_return(() => call($10, "load8_u", [55939]), 0);

// memory_copy.wast:1035
assert_return(() => call($10, "load8_u", [56138]), 0);

// memory_copy.wast:1036
assert_return(() => call($10, "load8_u", [56337]), 0);

// memory_copy.wast:1037
assert_return(() => call($10, "load8_u", [56536]), 0);

// memory_copy.wast:1038
assert_return(() => call($10, "load8_u", [56735]), 0);

// memory_copy.wast:1039
assert_return(() => call($10, "load8_u", [56934]), 0);

// memory_copy.wast:1040
assert_return(() => call($10, "load8_u", [57133]), 0);

// memory_copy.wast:1041
assert_return(() => call($10, "load8_u", [57332]), 0);

// memory_copy.wast:1042
assert_return(() => call($10, "load8_u", [57531]), 0);

// memory_copy.wast:1043
assert_return(() => call($10, "load8_u", [57730]), 0);

// memory_copy.wast:1044
assert_return(() => call($10, "load8_u", [57929]), 0);

// memory_copy.wast:1045
assert_return(() => call($10, "load8_u", [58128]), 0);

// memory_copy.wast:1046
assert_return(() => call($10, "load8_u", [58327]), 0);

// memory_copy.wast:1047
assert_return(() => call($10, "load8_u", [58526]), 0);

// memory_copy.wast:1048
assert_return(() => call($10, "load8_u", [58725]), 0);

// memory_copy.wast:1049
assert_return(() => call($10, "load8_u", [58924]), 0);

// memory_copy.wast:1050
assert_return(() => call($10, "load8_u", [59123]), 0);

// memory_copy.wast:1051
assert_return(() => call($10, "load8_u", [59322]), 0);

// memory_copy.wast:1052
assert_return(() => call($10, "load8_u", [59521]), 0);

// memory_copy.wast:1053
assert_return(() => call($10, "load8_u", [59720]), 0);

// memory_copy.wast:1054
assert_return(() => call($10, "load8_u", [59919]), 0);

// memory_copy.wast:1055
assert_return(() => call($10, "load8_u", [60118]), 0);

// memory_copy.wast:1056
assert_return(() => call($10, "load8_u", [60317]), 0);

// memory_copy.wast:1057
assert_return(() => call($10, "load8_u", [60516]), 0);

// memory_copy.wast:1058
assert_return(() => call($10, "load8_u", [60715]), 0);

// memory_copy.wast:1059
assert_return(() => call($10, "load8_u", [60914]), 0);

// memory_copy.wast:1060
assert_return(() => call($10, "load8_u", [61113]), 0);

// memory_copy.wast:1061
assert_return(() => call($10, "load8_u", [61312]), 0);

// memory_copy.wast:1062
assert_return(() => call($10, "load8_u", [61511]), 0);

// memory_copy.wast:1063
assert_return(() => call($10, "load8_u", [61710]), 0);

// memory_copy.wast:1064
assert_return(() => call($10, "load8_u", [61909]), 0);

// memory_copy.wast:1065
assert_return(() => call($10, "load8_u", [62108]), 0);

// memory_copy.wast:1066
assert_return(() => call($10, "load8_u", [62307]), 0);

// memory_copy.wast:1067
assert_return(() => call($10, "load8_u", [62506]), 0);

// memory_copy.wast:1068
assert_return(() => call($10, "load8_u", [62705]), 0);

// memory_copy.wast:1069
assert_return(() => call($10, "load8_u", [62904]), 0);

// memory_copy.wast:1070
assert_return(() => call($10, "load8_u", [63103]), 0);

// memory_copy.wast:1071
assert_return(() => call($10, "load8_u", [63302]), 0);

// memory_copy.wast:1072
assert_return(() => call($10, "load8_u", [63501]), 0);

// memory_copy.wast:1073
assert_return(() => call($10, "load8_u", [63700]), 0);

// memory_copy.wast:1074
assert_return(() => call($10, "load8_u", [63899]), 0);

// memory_copy.wast:1075
assert_return(() => call($10, "load8_u", [64098]), 0);

// memory_copy.wast:1076
assert_return(() => call($10, "load8_u", [64297]), 0);

// memory_copy.wast:1077
assert_return(() => call($10, "load8_u", [64496]), 0);

// memory_copy.wast:1078
assert_return(() => call($10, "load8_u", [64695]), 0);

// memory_copy.wast:1079
assert_return(() => call($10, "load8_u", [64894]), 0);

// memory_copy.wast:1080
assert_return(() => call($10, "load8_u", [65093]), 0);

// memory_copy.wast:1081
assert_return(() => call($10, "load8_u", [65292]), 0);

// memory_copy.wast:1082
assert_return(() => call($10, "load8_u", [65491]), 0);

// memory_copy.wast:1083
assert_return(() => call($10, "load8_u", [65515]), 0);

// memory_copy.wast:1084
assert_return(() => call($10, "load8_u", [65516]), 1);

// memory_copy.wast:1085
assert_return(() => call($10, "load8_u", [65517]), 2);

// memory_copy.wast:1086
assert_return(() => call($10, "load8_u", [65518]), 3);

// memory_copy.wast:1087
assert_return(() => call($10, "load8_u", [65519]), 4);

// memory_copy.wast:1088
assert_return(() => call($10, "load8_u", [65520]), 5);

// memory_copy.wast:1089
assert_return(() => call($10, "load8_u", [65521]), 6);

// memory_copy.wast:1090
assert_return(() => call($10, "load8_u", [65522]), 7);

// memory_copy.wast:1091
assert_return(() => call($10, "load8_u", [65523]), 8);

// memory_copy.wast:1092
assert_return(() => call($10, "load8_u", [65524]), 9);

// memory_copy.wast:1093
assert_return(() => call($10, "load8_u", [65525]), 10);

// memory_copy.wast:1094
assert_return(() => call($10, "load8_u", [65526]), 11);

// memory_copy.wast:1095
assert_return(() => call($10, "load8_u", [65527]), 12);

// memory_copy.wast:1096
assert_return(() => call($10, "load8_u", [65528]), 13);

// memory_copy.wast:1097
assert_return(() => call($10, "load8_u", [65529]), 14);

// memory_copy.wast:1098
assert_return(() => call($10, "load8_u", [65530]), 15);

// memory_copy.wast:1099
assert_return(() => call($10, "load8_u", [65531]), 16);

// memory_copy.wast:1100
assert_return(() => call($10, "load8_u", [65532]), 17);

// memory_copy.wast:1101
assert_return(() => call($10, "load8_u", [65533]), 18);

// memory_copy.wast:1102
assert_return(() => call($10, "load8_u", [65534]), 19);

// memory_copy.wast:1103
assert_return(() => call($10, "load8_u", [65535]), 20);

// memory_copy.wast:1105
let $11 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xec\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:1113
assert_trap(() => call($11, "run", [0, 65516, 40]));

// memory_copy.wast:1116
assert_return(() => call($11, "load8_u", [0]), 0);

// memory_copy.wast:1117
assert_return(() => call($11, "load8_u", [1]), 1);

// memory_copy.wast:1118
assert_return(() => call($11, "load8_u", [2]), 2);

// memory_copy.wast:1119
assert_return(() => call($11, "load8_u", [3]), 3);

// memory_copy.wast:1120
assert_return(() => call($11, "load8_u", [4]), 4);

// memory_copy.wast:1121
assert_return(() => call($11, "load8_u", [5]), 5);

// memory_copy.wast:1122
assert_return(() => call($11, "load8_u", [6]), 6);

// memory_copy.wast:1123
assert_return(() => call($11, "load8_u", [7]), 7);

// memory_copy.wast:1124
assert_return(() => call($11, "load8_u", [8]), 8);

// memory_copy.wast:1125
assert_return(() => call($11, "load8_u", [9]), 9);

// memory_copy.wast:1126
assert_return(() => call($11, "load8_u", [10]), 10);

// memory_copy.wast:1127
assert_return(() => call($11, "load8_u", [11]), 11);

// memory_copy.wast:1128
assert_return(() => call($11, "load8_u", [12]), 12);

// memory_copy.wast:1129
assert_return(() => call($11, "load8_u", [13]), 13);

// memory_copy.wast:1130
assert_return(() => call($11, "load8_u", [14]), 14);

// memory_copy.wast:1131
assert_return(() => call($11, "load8_u", [15]), 15);

// memory_copy.wast:1132
assert_return(() => call($11, "load8_u", [16]), 16);

// memory_copy.wast:1133
assert_return(() => call($11, "load8_u", [17]), 17);

// memory_copy.wast:1134
assert_return(() => call($11, "load8_u", [18]), 18);

// memory_copy.wast:1135
assert_return(() => call($11, "load8_u", [19]), 19);

// memory_copy.wast:1136
assert_return(() => call($11, "load8_u", [218]), 0);

// memory_copy.wast:1137
assert_return(() => call($11, "load8_u", [417]), 0);

// memory_copy.wast:1138
assert_return(() => call($11, "load8_u", [616]), 0);

// memory_copy.wast:1139
assert_return(() => call($11, "load8_u", [815]), 0);

// memory_copy.wast:1140
assert_return(() => call($11, "load8_u", [1014]), 0);

// memory_copy.wast:1141
assert_return(() => call($11, "load8_u", [1213]), 0);

// memory_copy.wast:1142
assert_return(() => call($11, "load8_u", [1412]), 0);

// memory_copy.wast:1143
assert_return(() => call($11, "load8_u", [1611]), 0);

// memory_copy.wast:1144
assert_return(() => call($11, "load8_u", [1810]), 0);

// memory_copy.wast:1145
assert_return(() => call($11, "load8_u", [2009]), 0);

// memory_copy.wast:1146
assert_return(() => call($11, "load8_u", [2208]), 0);

// memory_copy.wast:1147
assert_return(() => call($11, "load8_u", [2407]), 0);

// memory_copy.wast:1148
assert_return(() => call($11, "load8_u", [2606]), 0);

// memory_copy.wast:1149
assert_return(() => call($11, "load8_u", [2805]), 0);

// memory_copy.wast:1150
assert_return(() => call($11, "load8_u", [3004]), 0);

// memory_copy.wast:1151
assert_return(() => call($11, "load8_u", [3203]), 0);

// memory_copy.wast:1152
assert_return(() => call($11, "load8_u", [3402]), 0);

// memory_copy.wast:1153
assert_return(() => call($11, "load8_u", [3601]), 0);

// memory_copy.wast:1154
assert_return(() => call($11, "load8_u", [3800]), 0);

// memory_copy.wast:1155
assert_return(() => call($11, "load8_u", [3999]), 0);

// memory_copy.wast:1156
assert_return(() => call($11, "load8_u", [4198]), 0);

// memory_copy.wast:1157
assert_return(() => call($11, "load8_u", [4397]), 0);

// memory_copy.wast:1158
assert_return(() => call($11, "load8_u", [4596]), 0);

// memory_copy.wast:1159
assert_return(() => call($11, "load8_u", [4795]), 0);

// memory_copy.wast:1160
assert_return(() => call($11, "load8_u", [4994]), 0);

// memory_copy.wast:1161
assert_return(() => call($11, "load8_u", [5193]), 0);

// memory_copy.wast:1162
assert_return(() => call($11, "load8_u", [5392]), 0);

// memory_copy.wast:1163
assert_return(() => call($11, "load8_u", [5591]), 0);

// memory_copy.wast:1164
assert_return(() => call($11, "load8_u", [5790]), 0);

// memory_copy.wast:1165
assert_return(() => call($11, "load8_u", [5989]), 0);

// memory_copy.wast:1166
assert_return(() => call($11, "load8_u", [6188]), 0);

// memory_copy.wast:1167
assert_return(() => call($11, "load8_u", [6387]), 0);

// memory_copy.wast:1168
assert_return(() => call($11, "load8_u", [6586]), 0);

// memory_copy.wast:1169
assert_return(() => call($11, "load8_u", [6785]), 0);

// memory_copy.wast:1170
assert_return(() => call($11, "load8_u", [6984]), 0);

// memory_copy.wast:1171
assert_return(() => call($11, "load8_u", [7183]), 0);

// memory_copy.wast:1172
assert_return(() => call($11, "load8_u", [7382]), 0);

// memory_copy.wast:1173
assert_return(() => call($11, "load8_u", [7581]), 0);

// memory_copy.wast:1174
assert_return(() => call($11, "load8_u", [7780]), 0);

// memory_copy.wast:1175
assert_return(() => call($11, "load8_u", [7979]), 0);

// memory_copy.wast:1176
assert_return(() => call($11, "load8_u", [8178]), 0);

// memory_copy.wast:1177
assert_return(() => call($11, "load8_u", [8377]), 0);

// memory_copy.wast:1178
assert_return(() => call($11, "load8_u", [8576]), 0);

// memory_copy.wast:1179
assert_return(() => call($11, "load8_u", [8775]), 0);

// memory_copy.wast:1180
assert_return(() => call($11, "load8_u", [8974]), 0);

// memory_copy.wast:1181
assert_return(() => call($11, "load8_u", [9173]), 0);

// memory_copy.wast:1182
assert_return(() => call($11, "load8_u", [9372]), 0);

// memory_copy.wast:1183
assert_return(() => call($11, "load8_u", [9571]), 0);

// memory_copy.wast:1184
assert_return(() => call($11, "load8_u", [9770]), 0);

// memory_copy.wast:1185
assert_return(() => call($11, "load8_u", [9969]), 0);

// memory_copy.wast:1186
assert_return(() => call($11, "load8_u", [10168]), 0);

// memory_copy.wast:1187
assert_return(() => call($11, "load8_u", [10367]), 0);

// memory_copy.wast:1188
assert_return(() => call($11, "load8_u", [10566]), 0);

// memory_copy.wast:1189
assert_return(() => call($11, "load8_u", [10765]), 0);

// memory_copy.wast:1190
assert_return(() => call($11, "load8_u", [10964]), 0);

// memory_copy.wast:1191
assert_return(() => call($11, "load8_u", [11163]), 0);

// memory_copy.wast:1192
assert_return(() => call($11, "load8_u", [11362]), 0);

// memory_copy.wast:1193
assert_return(() => call($11, "load8_u", [11561]), 0);

// memory_copy.wast:1194
assert_return(() => call($11, "load8_u", [11760]), 0);

// memory_copy.wast:1195
assert_return(() => call($11, "load8_u", [11959]), 0);

// memory_copy.wast:1196
assert_return(() => call($11, "load8_u", [12158]), 0);

// memory_copy.wast:1197
assert_return(() => call($11, "load8_u", [12357]), 0);

// memory_copy.wast:1198
assert_return(() => call($11, "load8_u", [12556]), 0);

// memory_copy.wast:1199
assert_return(() => call($11, "load8_u", [12755]), 0);

// memory_copy.wast:1200
assert_return(() => call($11, "load8_u", [12954]), 0);

// memory_copy.wast:1201
assert_return(() => call($11, "load8_u", [13153]), 0);

// memory_copy.wast:1202
assert_return(() => call($11, "load8_u", [13352]), 0);

// memory_copy.wast:1203
assert_return(() => call($11, "load8_u", [13551]), 0);

// memory_copy.wast:1204
assert_return(() => call($11, "load8_u", [13750]), 0);

// memory_copy.wast:1205
assert_return(() => call($11, "load8_u", [13949]), 0);

// memory_copy.wast:1206
assert_return(() => call($11, "load8_u", [14148]), 0);

// memory_copy.wast:1207
assert_return(() => call($11, "load8_u", [14347]), 0);

// memory_copy.wast:1208
assert_return(() => call($11, "load8_u", [14546]), 0);

// memory_copy.wast:1209
assert_return(() => call($11, "load8_u", [14745]), 0);

// memory_copy.wast:1210
assert_return(() => call($11, "load8_u", [14944]), 0);

// memory_copy.wast:1211
assert_return(() => call($11, "load8_u", [15143]), 0);

// memory_copy.wast:1212
assert_return(() => call($11, "load8_u", [15342]), 0);

// memory_copy.wast:1213
assert_return(() => call($11, "load8_u", [15541]), 0);

// memory_copy.wast:1214
assert_return(() => call($11, "load8_u", [15740]), 0);

// memory_copy.wast:1215
assert_return(() => call($11, "load8_u", [15939]), 0);

// memory_copy.wast:1216
assert_return(() => call($11, "load8_u", [16138]), 0);

// memory_copy.wast:1217
assert_return(() => call($11, "load8_u", [16337]), 0);

// memory_copy.wast:1218
assert_return(() => call($11, "load8_u", [16536]), 0);

// memory_copy.wast:1219
assert_return(() => call($11, "load8_u", [16735]), 0);

// memory_copy.wast:1220
assert_return(() => call($11, "load8_u", [16934]), 0);

// memory_copy.wast:1221
assert_return(() => call($11, "load8_u", [17133]), 0);

// memory_copy.wast:1222
assert_return(() => call($11, "load8_u", [17332]), 0);

// memory_copy.wast:1223
assert_return(() => call($11, "load8_u", [17531]), 0);

// memory_copy.wast:1224
assert_return(() => call($11, "load8_u", [17730]), 0);

// memory_copy.wast:1225
assert_return(() => call($11, "load8_u", [17929]), 0);

// memory_copy.wast:1226
assert_return(() => call($11, "load8_u", [18128]), 0);

// memory_copy.wast:1227
assert_return(() => call($11, "load8_u", [18327]), 0);

// memory_copy.wast:1228
assert_return(() => call($11, "load8_u", [18526]), 0);

// memory_copy.wast:1229
assert_return(() => call($11, "load8_u", [18725]), 0);

// memory_copy.wast:1230
assert_return(() => call($11, "load8_u", [18924]), 0);

// memory_copy.wast:1231
assert_return(() => call($11, "load8_u", [19123]), 0);

// memory_copy.wast:1232
assert_return(() => call($11, "load8_u", [19322]), 0);

// memory_copy.wast:1233
assert_return(() => call($11, "load8_u", [19521]), 0);

// memory_copy.wast:1234
assert_return(() => call($11, "load8_u", [19720]), 0);

// memory_copy.wast:1235
assert_return(() => call($11, "load8_u", [19919]), 0);

// memory_copy.wast:1236
assert_return(() => call($11, "load8_u", [20118]), 0);

// memory_copy.wast:1237
assert_return(() => call($11, "load8_u", [20317]), 0);

// memory_copy.wast:1238
assert_return(() => call($11, "load8_u", [20516]), 0);

// memory_copy.wast:1239
assert_return(() => call($11, "load8_u", [20715]), 0);

// memory_copy.wast:1240
assert_return(() => call($11, "load8_u", [20914]), 0);

// memory_copy.wast:1241
assert_return(() => call($11, "load8_u", [21113]), 0);

// memory_copy.wast:1242
assert_return(() => call($11, "load8_u", [21312]), 0);

// memory_copy.wast:1243
assert_return(() => call($11, "load8_u", [21511]), 0);

// memory_copy.wast:1244
assert_return(() => call($11, "load8_u", [21710]), 0);

// memory_copy.wast:1245
assert_return(() => call($11, "load8_u", [21909]), 0);

// memory_copy.wast:1246
assert_return(() => call($11, "load8_u", [22108]), 0);

// memory_copy.wast:1247
assert_return(() => call($11, "load8_u", [22307]), 0);

// memory_copy.wast:1248
assert_return(() => call($11, "load8_u", [22506]), 0);

// memory_copy.wast:1249
assert_return(() => call($11, "load8_u", [22705]), 0);

// memory_copy.wast:1250
assert_return(() => call($11, "load8_u", [22904]), 0);

// memory_copy.wast:1251
assert_return(() => call($11, "load8_u", [23103]), 0);

// memory_copy.wast:1252
assert_return(() => call($11, "load8_u", [23302]), 0);

// memory_copy.wast:1253
assert_return(() => call($11, "load8_u", [23501]), 0);

// memory_copy.wast:1254
assert_return(() => call($11, "load8_u", [23700]), 0);

// memory_copy.wast:1255
assert_return(() => call($11, "load8_u", [23899]), 0);

// memory_copy.wast:1256
assert_return(() => call($11, "load8_u", [24098]), 0);

// memory_copy.wast:1257
assert_return(() => call($11, "load8_u", [24297]), 0);

// memory_copy.wast:1258
assert_return(() => call($11, "load8_u", [24496]), 0);

// memory_copy.wast:1259
assert_return(() => call($11, "load8_u", [24695]), 0);

// memory_copy.wast:1260
assert_return(() => call($11, "load8_u", [24894]), 0);

// memory_copy.wast:1261
assert_return(() => call($11, "load8_u", [25093]), 0);

// memory_copy.wast:1262
assert_return(() => call($11, "load8_u", [25292]), 0);

// memory_copy.wast:1263
assert_return(() => call($11, "load8_u", [25491]), 0);

// memory_copy.wast:1264
assert_return(() => call($11, "load8_u", [25690]), 0);

// memory_copy.wast:1265
assert_return(() => call($11, "load8_u", [25889]), 0);

// memory_copy.wast:1266
assert_return(() => call($11, "load8_u", [26088]), 0);

// memory_copy.wast:1267
assert_return(() => call($11, "load8_u", [26287]), 0);

// memory_copy.wast:1268
assert_return(() => call($11, "load8_u", [26486]), 0);

// memory_copy.wast:1269
assert_return(() => call($11, "load8_u", [26685]), 0);

// memory_copy.wast:1270
assert_return(() => call($11, "load8_u", [26884]), 0);

// memory_copy.wast:1271
assert_return(() => call($11, "load8_u", [27083]), 0);

// memory_copy.wast:1272
assert_return(() => call($11, "load8_u", [27282]), 0);

// memory_copy.wast:1273
assert_return(() => call($11, "load8_u", [27481]), 0);

// memory_copy.wast:1274
assert_return(() => call($11, "load8_u", [27680]), 0);

// memory_copy.wast:1275
assert_return(() => call($11, "load8_u", [27879]), 0);

// memory_copy.wast:1276
assert_return(() => call($11, "load8_u", [28078]), 0);

// memory_copy.wast:1277
assert_return(() => call($11, "load8_u", [28277]), 0);

// memory_copy.wast:1278
assert_return(() => call($11, "load8_u", [28476]), 0);

// memory_copy.wast:1279
assert_return(() => call($11, "load8_u", [28675]), 0);

// memory_copy.wast:1280
assert_return(() => call($11, "load8_u", [28874]), 0);

// memory_copy.wast:1281
assert_return(() => call($11, "load8_u", [29073]), 0);

// memory_copy.wast:1282
assert_return(() => call($11, "load8_u", [29272]), 0);

// memory_copy.wast:1283
assert_return(() => call($11, "load8_u", [29471]), 0);

// memory_copy.wast:1284
assert_return(() => call($11, "load8_u", [29670]), 0);

// memory_copy.wast:1285
assert_return(() => call($11, "load8_u", [29869]), 0);

// memory_copy.wast:1286
assert_return(() => call($11, "load8_u", [30068]), 0);

// memory_copy.wast:1287
assert_return(() => call($11, "load8_u", [30267]), 0);

// memory_copy.wast:1288
assert_return(() => call($11, "load8_u", [30466]), 0);

// memory_copy.wast:1289
assert_return(() => call($11, "load8_u", [30665]), 0);

// memory_copy.wast:1290
assert_return(() => call($11, "load8_u", [30864]), 0);

// memory_copy.wast:1291
assert_return(() => call($11, "load8_u", [31063]), 0);

// memory_copy.wast:1292
assert_return(() => call($11, "load8_u", [31262]), 0);

// memory_copy.wast:1293
assert_return(() => call($11, "load8_u", [31461]), 0);

// memory_copy.wast:1294
assert_return(() => call($11, "load8_u", [31660]), 0);

// memory_copy.wast:1295
assert_return(() => call($11, "load8_u", [31859]), 0);

// memory_copy.wast:1296
assert_return(() => call($11, "load8_u", [32058]), 0);

// memory_copy.wast:1297
assert_return(() => call($11, "load8_u", [32257]), 0);

// memory_copy.wast:1298
assert_return(() => call($11, "load8_u", [32456]), 0);

// memory_copy.wast:1299
assert_return(() => call($11, "load8_u", [32655]), 0);

// memory_copy.wast:1300
assert_return(() => call($11, "load8_u", [32854]), 0);

// memory_copy.wast:1301
assert_return(() => call($11, "load8_u", [33053]), 0);

// memory_copy.wast:1302
assert_return(() => call($11, "load8_u", [33252]), 0);

// memory_copy.wast:1303
assert_return(() => call($11, "load8_u", [33451]), 0);

// memory_copy.wast:1304
assert_return(() => call($11, "load8_u", [33650]), 0);

// memory_copy.wast:1305
assert_return(() => call($11, "load8_u", [33849]), 0);

// memory_copy.wast:1306
assert_return(() => call($11, "load8_u", [34048]), 0);

// memory_copy.wast:1307
assert_return(() => call($11, "load8_u", [34247]), 0);

// memory_copy.wast:1308
assert_return(() => call($11, "load8_u", [34446]), 0);

// memory_copy.wast:1309
assert_return(() => call($11, "load8_u", [34645]), 0);

// memory_copy.wast:1310
assert_return(() => call($11, "load8_u", [34844]), 0);

// memory_copy.wast:1311
assert_return(() => call($11, "load8_u", [35043]), 0);

// memory_copy.wast:1312
assert_return(() => call($11, "load8_u", [35242]), 0);

// memory_copy.wast:1313
assert_return(() => call($11, "load8_u", [35441]), 0);

// memory_copy.wast:1314
assert_return(() => call($11, "load8_u", [35640]), 0);

// memory_copy.wast:1315
assert_return(() => call($11, "load8_u", [35839]), 0);

// memory_copy.wast:1316
assert_return(() => call($11, "load8_u", [36038]), 0);

// memory_copy.wast:1317
assert_return(() => call($11, "load8_u", [36237]), 0);

// memory_copy.wast:1318
assert_return(() => call($11, "load8_u", [36436]), 0);

// memory_copy.wast:1319
assert_return(() => call($11, "load8_u", [36635]), 0);

// memory_copy.wast:1320
assert_return(() => call($11, "load8_u", [36834]), 0);

// memory_copy.wast:1321
assert_return(() => call($11, "load8_u", [37033]), 0);

// memory_copy.wast:1322
assert_return(() => call($11, "load8_u", [37232]), 0);

// memory_copy.wast:1323
assert_return(() => call($11, "load8_u", [37431]), 0);

// memory_copy.wast:1324
assert_return(() => call($11, "load8_u", [37630]), 0);

// memory_copy.wast:1325
assert_return(() => call($11, "load8_u", [37829]), 0);

// memory_copy.wast:1326
assert_return(() => call($11, "load8_u", [38028]), 0);

// memory_copy.wast:1327
assert_return(() => call($11, "load8_u", [38227]), 0);

// memory_copy.wast:1328
assert_return(() => call($11, "load8_u", [38426]), 0);

// memory_copy.wast:1329
assert_return(() => call($11, "load8_u", [38625]), 0);

// memory_copy.wast:1330
assert_return(() => call($11, "load8_u", [38824]), 0);

// memory_copy.wast:1331
assert_return(() => call($11, "load8_u", [39023]), 0);

// memory_copy.wast:1332
assert_return(() => call($11, "load8_u", [39222]), 0);

// memory_copy.wast:1333
assert_return(() => call($11, "load8_u", [39421]), 0);

// memory_copy.wast:1334
assert_return(() => call($11, "load8_u", [39620]), 0);

// memory_copy.wast:1335
assert_return(() => call($11, "load8_u", [39819]), 0);

// memory_copy.wast:1336
assert_return(() => call($11, "load8_u", [40018]), 0);

// memory_copy.wast:1337
assert_return(() => call($11, "load8_u", [40217]), 0);

// memory_copy.wast:1338
assert_return(() => call($11, "load8_u", [40416]), 0);

// memory_copy.wast:1339
assert_return(() => call($11, "load8_u", [40615]), 0);

// memory_copy.wast:1340
assert_return(() => call($11, "load8_u", [40814]), 0);

// memory_copy.wast:1341
assert_return(() => call($11, "load8_u", [41013]), 0);

// memory_copy.wast:1342
assert_return(() => call($11, "load8_u", [41212]), 0);

// memory_copy.wast:1343
assert_return(() => call($11, "load8_u", [41411]), 0);

// memory_copy.wast:1344
assert_return(() => call($11, "load8_u", [41610]), 0);

// memory_copy.wast:1345
assert_return(() => call($11, "load8_u", [41809]), 0);

// memory_copy.wast:1346
assert_return(() => call($11, "load8_u", [42008]), 0);

// memory_copy.wast:1347
assert_return(() => call($11, "load8_u", [42207]), 0);

// memory_copy.wast:1348
assert_return(() => call($11, "load8_u", [42406]), 0);

// memory_copy.wast:1349
assert_return(() => call($11, "load8_u", [42605]), 0);

// memory_copy.wast:1350
assert_return(() => call($11, "load8_u", [42804]), 0);

// memory_copy.wast:1351
assert_return(() => call($11, "load8_u", [43003]), 0);

// memory_copy.wast:1352
assert_return(() => call($11, "load8_u", [43202]), 0);

// memory_copy.wast:1353
assert_return(() => call($11, "load8_u", [43401]), 0);

// memory_copy.wast:1354
assert_return(() => call($11, "load8_u", [43600]), 0);

// memory_copy.wast:1355
assert_return(() => call($11, "load8_u", [43799]), 0);

// memory_copy.wast:1356
assert_return(() => call($11, "load8_u", [43998]), 0);

// memory_copy.wast:1357
assert_return(() => call($11, "load8_u", [44197]), 0);

// memory_copy.wast:1358
assert_return(() => call($11, "load8_u", [44396]), 0);

// memory_copy.wast:1359
assert_return(() => call($11, "load8_u", [44595]), 0);

// memory_copy.wast:1360
assert_return(() => call($11, "load8_u", [44794]), 0);

// memory_copy.wast:1361
assert_return(() => call($11, "load8_u", [44993]), 0);

// memory_copy.wast:1362
assert_return(() => call($11, "load8_u", [45192]), 0);

// memory_copy.wast:1363
assert_return(() => call($11, "load8_u", [45391]), 0);

// memory_copy.wast:1364
assert_return(() => call($11, "load8_u", [45590]), 0);

// memory_copy.wast:1365
assert_return(() => call($11, "load8_u", [45789]), 0);

// memory_copy.wast:1366
assert_return(() => call($11, "load8_u", [45988]), 0);

// memory_copy.wast:1367
assert_return(() => call($11, "load8_u", [46187]), 0);

// memory_copy.wast:1368
assert_return(() => call($11, "load8_u", [46386]), 0);

// memory_copy.wast:1369
assert_return(() => call($11, "load8_u", [46585]), 0);

// memory_copy.wast:1370
assert_return(() => call($11, "load8_u", [46784]), 0);

// memory_copy.wast:1371
assert_return(() => call($11, "load8_u", [46983]), 0);

// memory_copy.wast:1372
assert_return(() => call($11, "load8_u", [47182]), 0);

// memory_copy.wast:1373
assert_return(() => call($11, "load8_u", [47381]), 0);

// memory_copy.wast:1374
assert_return(() => call($11, "load8_u", [47580]), 0);

// memory_copy.wast:1375
assert_return(() => call($11, "load8_u", [47779]), 0);

// memory_copy.wast:1376
assert_return(() => call($11, "load8_u", [47978]), 0);

// memory_copy.wast:1377
assert_return(() => call($11, "load8_u", [48177]), 0);

// memory_copy.wast:1378
assert_return(() => call($11, "load8_u", [48376]), 0);

// memory_copy.wast:1379
assert_return(() => call($11, "load8_u", [48575]), 0);

// memory_copy.wast:1380
assert_return(() => call($11, "load8_u", [48774]), 0);

// memory_copy.wast:1381
assert_return(() => call($11, "load8_u", [48973]), 0);

// memory_copy.wast:1382
assert_return(() => call($11, "load8_u", [49172]), 0);

// memory_copy.wast:1383
assert_return(() => call($11, "load8_u", [49371]), 0);

// memory_copy.wast:1384
assert_return(() => call($11, "load8_u", [49570]), 0);

// memory_copy.wast:1385
assert_return(() => call($11, "load8_u", [49769]), 0);

// memory_copy.wast:1386
assert_return(() => call($11, "load8_u", [49968]), 0);

// memory_copy.wast:1387
assert_return(() => call($11, "load8_u", [50167]), 0);

// memory_copy.wast:1388
assert_return(() => call($11, "load8_u", [50366]), 0);

// memory_copy.wast:1389
assert_return(() => call($11, "load8_u", [50565]), 0);

// memory_copy.wast:1390
assert_return(() => call($11, "load8_u", [50764]), 0);

// memory_copy.wast:1391
assert_return(() => call($11, "load8_u", [50963]), 0);

// memory_copy.wast:1392
assert_return(() => call($11, "load8_u", [51162]), 0);

// memory_copy.wast:1393
assert_return(() => call($11, "load8_u", [51361]), 0);

// memory_copy.wast:1394
assert_return(() => call($11, "load8_u", [51560]), 0);

// memory_copy.wast:1395
assert_return(() => call($11, "load8_u", [51759]), 0);

// memory_copy.wast:1396
assert_return(() => call($11, "load8_u", [51958]), 0);

// memory_copy.wast:1397
assert_return(() => call($11, "load8_u", [52157]), 0);

// memory_copy.wast:1398
assert_return(() => call($11, "load8_u", [52356]), 0);

// memory_copy.wast:1399
assert_return(() => call($11, "load8_u", [52555]), 0);

// memory_copy.wast:1400
assert_return(() => call($11, "load8_u", [52754]), 0);

// memory_copy.wast:1401
assert_return(() => call($11, "load8_u", [52953]), 0);

// memory_copy.wast:1402
assert_return(() => call($11, "load8_u", [53152]), 0);

// memory_copy.wast:1403
assert_return(() => call($11, "load8_u", [53351]), 0);

// memory_copy.wast:1404
assert_return(() => call($11, "load8_u", [53550]), 0);

// memory_copy.wast:1405
assert_return(() => call($11, "load8_u", [53749]), 0);

// memory_copy.wast:1406
assert_return(() => call($11, "load8_u", [53948]), 0);

// memory_copy.wast:1407
assert_return(() => call($11, "load8_u", [54147]), 0);

// memory_copy.wast:1408
assert_return(() => call($11, "load8_u", [54346]), 0);

// memory_copy.wast:1409
assert_return(() => call($11, "load8_u", [54545]), 0);

// memory_copy.wast:1410
assert_return(() => call($11, "load8_u", [54744]), 0);

// memory_copy.wast:1411
assert_return(() => call($11, "load8_u", [54943]), 0);

// memory_copy.wast:1412
assert_return(() => call($11, "load8_u", [55142]), 0);

// memory_copy.wast:1413
assert_return(() => call($11, "load8_u", [55341]), 0);

// memory_copy.wast:1414
assert_return(() => call($11, "load8_u", [55540]), 0);

// memory_copy.wast:1415
assert_return(() => call($11, "load8_u", [55739]), 0);

// memory_copy.wast:1416
assert_return(() => call($11, "load8_u", [55938]), 0);

// memory_copy.wast:1417
assert_return(() => call($11, "load8_u", [56137]), 0);

// memory_copy.wast:1418
assert_return(() => call($11, "load8_u", [56336]), 0);

// memory_copy.wast:1419
assert_return(() => call($11, "load8_u", [56535]), 0);

// memory_copy.wast:1420
assert_return(() => call($11, "load8_u", [56734]), 0);

// memory_copy.wast:1421
assert_return(() => call($11, "load8_u", [56933]), 0);

// memory_copy.wast:1422
assert_return(() => call($11, "load8_u", [57132]), 0);

// memory_copy.wast:1423
assert_return(() => call($11, "load8_u", [57331]), 0);

// memory_copy.wast:1424
assert_return(() => call($11, "load8_u", [57530]), 0);

// memory_copy.wast:1425
assert_return(() => call($11, "load8_u", [57729]), 0);

// memory_copy.wast:1426
assert_return(() => call($11, "load8_u", [57928]), 0);

// memory_copy.wast:1427
assert_return(() => call($11, "load8_u", [58127]), 0);

// memory_copy.wast:1428
assert_return(() => call($11, "load8_u", [58326]), 0);

// memory_copy.wast:1429
assert_return(() => call($11, "load8_u", [58525]), 0);

// memory_copy.wast:1430
assert_return(() => call($11, "load8_u", [58724]), 0);

// memory_copy.wast:1431
assert_return(() => call($11, "load8_u", [58923]), 0);

// memory_copy.wast:1432
assert_return(() => call($11, "load8_u", [59122]), 0);

// memory_copy.wast:1433
assert_return(() => call($11, "load8_u", [59321]), 0);

// memory_copy.wast:1434
assert_return(() => call($11, "load8_u", [59520]), 0);

// memory_copy.wast:1435
assert_return(() => call($11, "load8_u", [59719]), 0);

// memory_copy.wast:1436
assert_return(() => call($11, "load8_u", [59918]), 0);

// memory_copy.wast:1437
assert_return(() => call($11, "load8_u", [60117]), 0);

// memory_copy.wast:1438
assert_return(() => call($11, "load8_u", [60316]), 0);

// memory_copy.wast:1439
assert_return(() => call($11, "load8_u", [60515]), 0);

// memory_copy.wast:1440
assert_return(() => call($11, "load8_u", [60714]), 0);

// memory_copy.wast:1441
assert_return(() => call($11, "load8_u", [60913]), 0);

// memory_copy.wast:1442
assert_return(() => call($11, "load8_u", [61112]), 0);

// memory_copy.wast:1443
assert_return(() => call($11, "load8_u", [61311]), 0);

// memory_copy.wast:1444
assert_return(() => call($11, "load8_u", [61510]), 0);

// memory_copy.wast:1445
assert_return(() => call($11, "load8_u", [61709]), 0);

// memory_copy.wast:1446
assert_return(() => call($11, "load8_u", [61908]), 0);

// memory_copy.wast:1447
assert_return(() => call($11, "load8_u", [62107]), 0);

// memory_copy.wast:1448
assert_return(() => call($11, "load8_u", [62306]), 0);

// memory_copy.wast:1449
assert_return(() => call($11, "load8_u", [62505]), 0);

// memory_copy.wast:1450
assert_return(() => call($11, "load8_u", [62704]), 0);

// memory_copy.wast:1451
assert_return(() => call($11, "load8_u", [62903]), 0);

// memory_copy.wast:1452
assert_return(() => call($11, "load8_u", [63102]), 0);

// memory_copy.wast:1453
assert_return(() => call($11, "load8_u", [63301]), 0);

// memory_copy.wast:1454
assert_return(() => call($11, "load8_u", [63500]), 0);

// memory_copy.wast:1455
assert_return(() => call($11, "load8_u", [63699]), 0);

// memory_copy.wast:1456
assert_return(() => call($11, "load8_u", [63898]), 0);

// memory_copy.wast:1457
assert_return(() => call($11, "load8_u", [64097]), 0);

// memory_copy.wast:1458
assert_return(() => call($11, "load8_u", [64296]), 0);

// memory_copy.wast:1459
assert_return(() => call($11, "load8_u", [64495]), 0);

// memory_copy.wast:1460
assert_return(() => call($11, "load8_u", [64694]), 0);

// memory_copy.wast:1461
assert_return(() => call($11, "load8_u", [64893]), 0);

// memory_copy.wast:1462
assert_return(() => call($11, "load8_u", [65092]), 0);

// memory_copy.wast:1463
assert_return(() => call($11, "load8_u", [65291]), 0);

// memory_copy.wast:1464
assert_return(() => call($11, "load8_u", [65490]), 0);

// memory_copy.wast:1465
assert_return(() => call($11, "load8_u", [65516]), 0);

// memory_copy.wast:1466
assert_return(() => call($11, "load8_u", [65517]), 1);

// memory_copy.wast:1467
assert_return(() => call($11, "load8_u", [65518]), 2);

// memory_copy.wast:1468
assert_return(() => call($11, "load8_u", [65519]), 3);

// memory_copy.wast:1469
assert_return(() => call($11, "load8_u", [65520]), 4);

// memory_copy.wast:1470
assert_return(() => call($11, "load8_u", [65521]), 5);

// memory_copy.wast:1471
assert_return(() => call($11, "load8_u", [65522]), 6);

// memory_copy.wast:1472
assert_return(() => call($11, "load8_u", [65523]), 7);

// memory_copy.wast:1473
assert_return(() => call($11, "load8_u", [65524]), 8);

// memory_copy.wast:1474
assert_return(() => call($11, "load8_u", [65525]), 9);

// memory_copy.wast:1475
assert_return(() => call($11, "load8_u", [65526]), 10);

// memory_copy.wast:1476
assert_return(() => call($11, "load8_u", [65527]), 11);

// memory_copy.wast:1477
assert_return(() => call($11, "load8_u", [65528]), 12);

// memory_copy.wast:1478
assert_return(() => call($11, "load8_u", [65529]), 13);

// memory_copy.wast:1479
assert_return(() => call($11, "load8_u", [65530]), 14);

// memory_copy.wast:1480
assert_return(() => call($11, "load8_u", [65531]), 15);

// memory_copy.wast:1481
assert_return(() => call($11, "load8_u", [65532]), 16);

// memory_copy.wast:1482
assert_return(() => call($11, "load8_u", [65533]), 17);

// memory_copy.wast:1483
assert_return(() => call($11, "load8_u", [65534]), 18);

// memory_copy.wast:1484
assert_return(() => call($11, "load8_u", [65535]), 19);

// memory_copy.wast:1486
let $12 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9d\x80\x80\x80\x00\x01\x00\x41\xeb\xff\x03\x0b\x15\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14");

// memory_copy.wast:1494
assert_trap(() => call($12, "run", [0, 65515, 39]));

// memory_copy.wast:1497
assert_return(() => call($12, "load8_u", [0]), 0);

// memory_copy.wast:1498
assert_return(() => call($12, "load8_u", [1]), 1);

// memory_copy.wast:1499
assert_return(() => call($12, "load8_u", [2]), 2);

// memory_copy.wast:1500
assert_return(() => call($12, "load8_u", [3]), 3);

// memory_copy.wast:1501
assert_return(() => call($12, "load8_u", [4]), 4);

// memory_copy.wast:1502
assert_return(() => call($12, "load8_u", [5]), 5);

// memory_copy.wast:1503
assert_return(() => call($12, "load8_u", [6]), 6);

// memory_copy.wast:1504
assert_return(() => call($12, "load8_u", [7]), 7);

// memory_copy.wast:1505
assert_return(() => call($12, "load8_u", [8]), 8);

// memory_copy.wast:1506
assert_return(() => call($12, "load8_u", [9]), 9);

// memory_copy.wast:1507
assert_return(() => call($12, "load8_u", [10]), 10);

// memory_copy.wast:1508
assert_return(() => call($12, "load8_u", [11]), 11);

// memory_copy.wast:1509
assert_return(() => call($12, "load8_u", [12]), 12);

// memory_copy.wast:1510
assert_return(() => call($12, "load8_u", [13]), 13);

// memory_copy.wast:1511
assert_return(() => call($12, "load8_u", [14]), 14);

// memory_copy.wast:1512
assert_return(() => call($12, "load8_u", [15]), 15);

// memory_copy.wast:1513
assert_return(() => call($12, "load8_u", [16]), 16);

// memory_copy.wast:1514
assert_return(() => call($12, "load8_u", [17]), 17);

// memory_copy.wast:1515
assert_return(() => call($12, "load8_u", [18]), 18);

// memory_copy.wast:1516
assert_return(() => call($12, "load8_u", [19]), 19);

// memory_copy.wast:1517
assert_return(() => call($12, "load8_u", [20]), 20);

// memory_copy.wast:1518
assert_return(() => call($12, "load8_u", [219]), 0);

// memory_copy.wast:1519
assert_return(() => call($12, "load8_u", [418]), 0);

// memory_copy.wast:1520
assert_return(() => call($12, "load8_u", [617]), 0);

// memory_copy.wast:1521
assert_return(() => call($12, "load8_u", [816]), 0);

// memory_copy.wast:1522
assert_return(() => call($12, "load8_u", [1015]), 0);

// memory_copy.wast:1523
assert_return(() => call($12, "load8_u", [1214]), 0);

// memory_copy.wast:1524
assert_return(() => call($12, "load8_u", [1413]), 0);

// memory_copy.wast:1525
assert_return(() => call($12, "load8_u", [1612]), 0);

// memory_copy.wast:1526
assert_return(() => call($12, "load8_u", [1811]), 0);

// memory_copy.wast:1527
assert_return(() => call($12, "load8_u", [2010]), 0);

// memory_copy.wast:1528
assert_return(() => call($12, "load8_u", [2209]), 0);

// memory_copy.wast:1529
assert_return(() => call($12, "load8_u", [2408]), 0);

// memory_copy.wast:1530
assert_return(() => call($12, "load8_u", [2607]), 0);

// memory_copy.wast:1531
assert_return(() => call($12, "load8_u", [2806]), 0);

// memory_copy.wast:1532
assert_return(() => call($12, "load8_u", [3005]), 0);

// memory_copy.wast:1533
assert_return(() => call($12, "load8_u", [3204]), 0);

// memory_copy.wast:1534
assert_return(() => call($12, "load8_u", [3403]), 0);

// memory_copy.wast:1535
assert_return(() => call($12, "load8_u", [3602]), 0);

// memory_copy.wast:1536
assert_return(() => call($12, "load8_u", [3801]), 0);

// memory_copy.wast:1537
assert_return(() => call($12, "load8_u", [4000]), 0);

// memory_copy.wast:1538
assert_return(() => call($12, "load8_u", [4199]), 0);

// memory_copy.wast:1539
assert_return(() => call($12, "load8_u", [4398]), 0);

// memory_copy.wast:1540
assert_return(() => call($12, "load8_u", [4597]), 0);

// memory_copy.wast:1541
assert_return(() => call($12, "load8_u", [4796]), 0);

// memory_copy.wast:1542
assert_return(() => call($12, "load8_u", [4995]), 0);

// memory_copy.wast:1543
assert_return(() => call($12, "load8_u", [5194]), 0);

// memory_copy.wast:1544
assert_return(() => call($12, "load8_u", [5393]), 0);

// memory_copy.wast:1545
assert_return(() => call($12, "load8_u", [5592]), 0);

// memory_copy.wast:1546
assert_return(() => call($12, "load8_u", [5791]), 0);

// memory_copy.wast:1547
assert_return(() => call($12, "load8_u", [5990]), 0);

// memory_copy.wast:1548
assert_return(() => call($12, "load8_u", [6189]), 0);

// memory_copy.wast:1549
assert_return(() => call($12, "load8_u", [6388]), 0);

// memory_copy.wast:1550
assert_return(() => call($12, "load8_u", [6587]), 0);

// memory_copy.wast:1551
assert_return(() => call($12, "load8_u", [6786]), 0);

// memory_copy.wast:1552
assert_return(() => call($12, "load8_u", [6985]), 0);

// memory_copy.wast:1553
assert_return(() => call($12, "load8_u", [7184]), 0);

// memory_copy.wast:1554
assert_return(() => call($12, "load8_u", [7383]), 0);

// memory_copy.wast:1555
assert_return(() => call($12, "load8_u", [7582]), 0);

// memory_copy.wast:1556
assert_return(() => call($12, "load8_u", [7781]), 0);

// memory_copy.wast:1557
assert_return(() => call($12, "load8_u", [7980]), 0);

// memory_copy.wast:1558
assert_return(() => call($12, "load8_u", [8179]), 0);

// memory_copy.wast:1559
assert_return(() => call($12, "load8_u", [8378]), 0);

// memory_copy.wast:1560
assert_return(() => call($12, "load8_u", [8577]), 0);

// memory_copy.wast:1561
assert_return(() => call($12, "load8_u", [8776]), 0);

// memory_copy.wast:1562
assert_return(() => call($12, "load8_u", [8975]), 0);

// memory_copy.wast:1563
assert_return(() => call($12, "load8_u", [9174]), 0);

// memory_copy.wast:1564
assert_return(() => call($12, "load8_u", [9373]), 0);

// memory_copy.wast:1565
assert_return(() => call($12, "load8_u", [9572]), 0);

// memory_copy.wast:1566
assert_return(() => call($12, "load8_u", [9771]), 0);

// memory_copy.wast:1567
assert_return(() => call($12, "load8_u", [9970]), 0);

// memory_copy.wast:1568
assert_return(() => call($12, "load8_u", [10169]), 0);

// memory_copy.wast:1569
assert_return(() => call($12, "load8_u", [10368]), 0);

// memory_copy.wast:1570
assert_return(() => call($12, "load8_u", [10567]), 0);

// memory_copy.wast:1571
assert_return(() => call($12, "load8_u", [10766]), 0);

// memory_copy.wast:1572
assert_return(() => call($12, "load8_u", [10965]), 0);

// memory_copy.wast:1573
assert_return(() => call($12, "load8_u", [11164]), 0);

// memory_copy.wast:1574
assert_return(() => call($12, "load8_u", [11363]), 0);

// memory_copy.wast:1575
assert_return(() => call($12, "load8_u", [11562]), 0);

// memory_copy.wast:1576
assert_return(() => call($12, "load8_u", [11761]), 0);

// memory_copy.wast:1577
assert_return(() => call($12, "load8_u", [11960]), 0);

// memory_copy.wast:1578
assert_return(() => call($12, "load8_u", [12159]), 0);

// memory_copy.wast:1579
assert_return(() => call($12, "load8_u", [12358]), 0);

// memory_copy.wast:1580
assert_return(() => call($12, "load8_u", [12557]), 0);

// memory_copy.wast:1581
assert_return(() => call($12, "load8_u", [12756]), 0);

// memory_copy.wast:1582
assert_return(() => call($12, "load8_u", [12955]), 0);

// memory_copy.wast:1583
assert_return(() => call($12, "load8_u", [13154]), 0);

// memory_copy.wast:1584
assert_return(() => call($12, "load8_u", [13353]), 0);

// memory_copy.wast:1585
assert_return(() => call($12, "load8_u", [13552]), 0);

// memory_copy.wast:1586
assert_return(() => call($12, "load8_u", [13751]), 0);

// memory_copy.wast:1587
assert_return(() => call($12, "load8_u", [13950]), 0);

// memory_copy.wast:1588
assert_return(() => call($12, "load8_u", [14149]), 0);

// memory_copy.wast:1589
assert_return(() => call($12, "load8_u", [14348]), 0);

// memory_copy.wast:1590
assert_return(() => call($12, "load8_u", [14547]), 0);

// memory_copy.wast:1591
assert_return(() => call($12, "load8_u", [14746]), 0);

// memory_copy.wast:1592
assert_return(() => call($12, "load8_u", [14945]), 0);

// memory_copy.wast:1593
assert_return(() => call($12, "load8_u", [15144]), 0);

// memory_copy.wast:1594
assert_return(() => call($12, "load8_u", [15343]), 0);

// memory_copy.wast:1595
assert_return(() => call($12, "load8_u", [15542]), 0);

// memory_copy.wast:1596
assert_return(() => call($12, "load8_u", [15741]), 0);

// memory_copy.wast:1597
assert_return(() => call($12, "load8_u", [15940]), 0);

// memory_copy.wast:1598
assert_return(() => call($12, "load8_u", [16139]), 0);

// memory_copy.wast:1599
assert_return(() => call($12, "load8_u", [16338]), 0);

// memory_copy.wast:1600
assert_return(() => call($12, "load8_u", [16537]), 0);

// memory_copy.wast:1601
assert_return(() => call($12, "load8_u", [16736]), 0);

// memory_copy.wast:1602
assert_return(() => call($12, "load8_u", [16935]), 0);

// memory_copy.wast:1603
assert_return(() => call($12, "load8_u", [17134]), 0);

// memory_copy.wast:1604
assert_return(() => call($12, "load8_u", [17333]), 0);

// memory_copy.wast:1605
assert_return(() => call($12, "load8_u", [17532]), 0);

// memory_copy.wast:1606
assert_return(() => call($12, "load8_u", [17731]), 0);

// memory_copy.wast:1607
assert_return(() => call($12, "load8_u", [17930]), 0);

// memory_copy.wast:1608
assert_return(() => call($12, "load8_u", [18129]), 0);

// memory_copy.wast:1609
assert_return(() => call($12, "load8_u", [18328]), 0);

// memory_copy.wast:1610
assert_return(() => call($12, "load8_u", [18527]), 0);

// memory_copy.wast:1611
assert_return(() => call($12, "load8_u", [18726]), 0);

// memory_copy.wast:1612
assert_return(() => call($12, "load8_u", [18925]), 0);

// memory_copy.wast:1613
assert_return(() => call($12, "load8_u", [19124]), 0);

// memory_copy.wast:1614
assert_return(() => call($12, "load8_u", [19323]), 0);

// memory_copy.wast:1615
assert_return(() => call($12, "load8_u", [19522]), 0);

// memory_copy.wast:1616
assert_return(() => call($12, "load8_u", [19721]), 0);

// memory_copy.wast:1617
assert_return(() => call($12, "load8_u", [19920]), 0);

// memory_copy.wast:1618
assert_return(() => call($12, "load8_u", [20119]), 0);

// memory_copy.wast:1619
assert_return(() => call($12, "load8_u", [20318]), 0);

// memory_copy.wast:1620
assert_return(() => call($12, "load8_u", [20517]), 0);

// memory_copy.wast:1621
assert_return(() => call($12, "load8_u", [20716]), 0);

// memory_copy.wast:1622
assert_return(() => call($12, "load8_u", [20915]), 0);

// memory_copy.wast:1623
assert_return(() => call($12, "load8_u", [21114]), 0);

// memory_copy.wast:1624
assert_return(() => call($12, "load8_u", [21313]), 0);

// memory_copy.wast:1625
assert_return(() => call($12, "load8_u", [21512]), 0);

// memory_copy.wast:1626
assert_return(() => call($12, "load8_u", [21711]), 0);

// memory_copy.wast:1627
assert_return(() => call($12, "load8_u", [21910]), 0);

// memory_copy.wast:1628
assert_return(() => call($12, "load8_u", [22109]), 0);

// memory_copy.wast:1629
assert_return(() => call($12, "load8_u", [22308]), 0);

// memory_copy.wast:1630
assert_return(() => call($12, "load8_u", [22507]), 0);

// memory_copy.wast:1631
assert_return(() => call($12, "load8_u", [22706]), 0);

// memory_copy.wast:1632
assert_return(() => call($12, "load8_u", [22905]), 0);

// memory_copy.wast:1633
assert_return(() => call($12, "load8_u", [23104]), 0);

// memory_copy.wast:1634
assert_return(() => call($12, "load8_u", [23303]), 0);

// memory_copy.wast:1635
assert_return(() => call($12, "load8_u", [23502]), 0);

// memory_copy.wast:1636
assert_return(() => call($12, "load8_u", [23701]), 0);

// memory_copy.wast:1637
assert_return(() => call($12, "load8_u", [23900]), 0);

// memory_copy.wast:1638
assert_return(() => call($12, "load8_u", [24099]), 0);

// memory_copy.wast:1639
assert_return(() => call($12, "load8_u", [24298]), 0);

// memory_copy.wast:1640
assert_return(() => call($12, "load8_u", [24497]), 0);

// memory_copy.wast:1641
assert_return(() => call($12, "load8_u", [24696]), 0);

// memory_copy.wast:1642
assert_return(() => call($12, "load8_u", [24895]), 0);

// memory_copy.wast:1643
assert_return(() => call($12, "load8_u", [25094]), 0);

// memory_copy.wast:1644
assert_return(() => call($12, "load8_u", [25293]), 0);

// memory_copy.wast:1645
assert_return(() => call($12, "load8_u", [25492]), 0);

// memory_copy.wast:1646
assert_return(() => call($12, "load8_u", [25691]), 0);

// memory_copy.wast:1647
assert_return(() => call($12, "load8_u", [25890]), 0);

// memory_copy.wast:1648
assert_return(() => call($12, "load8_u", [26089]), 0);

// memory_copy.wast:1649
assert_return(() => call($12, "load8_u", [26288]), 0);

// memory_copy.wast:1650
assert_return(() => call($12, "load8_u", [26487]), 0);

// memory_copy.wast:1651
assert_return(() => call($12, "load8_u", [26686]), 0);

// memory_copy.wast:1652
assert_return(() => call($12, "load8_u", [26885]), 0);

// memory_copy.wast:1653
assert_return(() => call($12, "load8_u", [27084]), 0);

// memory_copy.wast:1654
assert_return(() => call($12, "load8_u", [27283]), 0);

// memory_copy.wast:1655
assert_return(() => call($12, "load8_u", [27482]), 0);

// memory_copy.wast:1656
assert_return(() => call($12, "load8_u", [27681]), 0);

// memory_copy.wast:1657
assert_return(() => call($12, "load8_u", [27880]), 0);

// memory_copy.wast:1658
assert_return(() => call($12, "load8_u", [28079]), 0);

// memory_copy.wast:1659
assert_return(() => call($12, "load8_u", [28278]), 0);

// memory_copy.wast:1660
assert_return(() => call($12, "load8_u", [28477]), 0);

// memory_copy.wast:1661
assert_return(() => call($12, "load8_u", [28676]), 0);

// memory_copy.wast:1662
assert_return(() => call($12, "load8_u", [28875]), 0);

// memory_copy.wast:1663
assert_return(() => call($12, "load8_u", [29074]), 0);

// memory_copy.wast:1664
assert_return(() => call($12, "load8_u", [29273]), 0);

// memory_copy.wast:1665
assert_return(() => call($12, "load8_u", [29472]), 0);

// memory_copy.wast:1666
assert_return(() => call($12, "load8_u", [29671]), 0);

// memory_copy.wast:1667
assert_return(() => call($12, "load8_u", [29870]), 0);

// memory_copy.wast:1668
assert_return(() => call($12, "load8_u", [30069]), 0);

// memory_copy.wast:1669
assert_return(() => call($12, "load8_u", [30268]), 0);

// memory_copy.wast:1670
assert_return(() => call($12, "load8_u", [30467]), 0);

// memory_copy.wast:1671
assert_return(() => call($12, "load8_u", [30666]), 0);

// memory_copy.wast:1672
assert_return(() => call($12, "load8_u", [30865]), 0);

// memory_copy.wast:1673
assert_return(() => call($12, "load8_u", [31064]), 0);

// memory_copy.wast:1674
assert_return(() => call($12, "load8_u", [31263]), 0);

// memory_copy.wast:1675
assert_return(() => call($12, "load8_u", [31462]), 0);

// memory_copy.wast:1676
assert_return(() => call($12, "load8_u", [31661]), 0);

// memory_copy.wast:1677
assert_return(() => call($12, "load8_u", [31860]), 0);

// memory_copy.wast:1678
assert_return(() => call($12, "load8_u", [32059]), 0);

// memory_copy.wast:1679
assert_return(() => call($12, "load8_u", [32258]), 0);

// memory_copy.wast:1680
assert_return(() => call($12, "load8_u", [32457]), 0);

// memory_copy.wast:1681
assert_return(() => call($12, "load8_u", [32656]), 0);

// memory_copy.wast:1682
assert_return(() => call($12, "load8_u", [32855]), 0);

// memory_copy.wast:1683
assert_return(() => call($12, "load8_u", [33054]), 0);

// memory_copy.wast:1684
assert_return(() => call($12, "load8_u", [33253]), 0);

// memory_copy.wast:1685
assert_return(() => call($12, "load8_u", [33452]), 0);

// memory_copy.wast:1686
assert_return(() => call($12, "load8_u", [33651]), 0);

// memory_copy.wast:1687
assert_return(() => call($12, "load8_u", [33850]), 0);

// memory_copy.wast:1688
assert_return(() => call($12, "load8_u", [34049]), 0);

// memory_copy.wast:1689
assert_return(() => call($12, "load8_u", [34248]), 0);

// memory_copy.wast:1690
assert_return(() => call($12, "load8_u", [34447]), 0);

// memory_copy.wast:1691
assert_return(() => call($12, "load8_u", [34646]), 0);

// memory_copy.wast:1692
assert_return(() => call($12, "load8_u", [34845]), 0);

// memory_copy.wast:1693
assert_return(() => call($12, "load8_u", [35044]), 0);

// memory_copy.wast:1694
assert_return(() => call($12, "load8_u", [35243]), 0);

// memory_copy.wast:1695
assert_return(() => call($12, "load8_u", [35442]), 0);

// memory_copy.wast:1696
assert_return(() => call($12, "load8_u", [35641]), 0);

// memory_copy.wast:1697
assert_return(() => call($12, "load8_u", [35840]), 0);

// memory_copy.wast:1698
assert_return(() => call($12, "load8_u", [36039]), 0);

// memory_copy.wast:1699
assert_return(() => call($12, "load8_u", [36238]), 0);

// memory_copy.wast:1700
assert_return(() => call($12, "load8_u", [36437]), 0);

// memory_copy.wast:1701
assert_return(() => call($12, "load8_u", [36636]), 0);

// memory_copy.wast:1702
assert_return(() => call($12, "load8_u", [36835]), 0);

// memory_copy.wast:1703
assert_return(() => call($12, "load8_u", [37034]), 0);

// memory_copy.wast:1704
assert_return(() => call($12, "load8_u", [37233]), 0);

// memory_copy.wast:1705
assert_return(() => call($12, "load8_u", [37432]), 0);

// memory_copy.wast:1706
assert_return(() => call($12, "load8_u", [37631]), 0);

// memory_copy.wast:1707
assert_return(() => call($12, "load8_u", [37830]), 0);

// memory_copy.wast:1708
assert_return(() => call($12, "load8_u", [38029]), 0);

// memory_copy.wast:1709
assert_return(() => call($12, "load8_u", [38228]), 0);

// memory_copy.wast:1710
assert_return(() => call($12, "load8_u", [38427]), 0);

// memory_copy.wast:1711
assert_return(() => call($12, "load8_u", [38626]), 0);

// memory_copy.wast:1712
assert_return(() => call($12, "load8_u", [38825]), 0);

// memory_copy.wast:1713
assert_return(() => call($12, "load8_u", [39024]), 0);

// memory_copy.wast:1714
assert_return(() => call($12, "load8_u", [39223]), 0);

// memory_copy.wast:1715
assert_return(() => call($12, "load8_u", [39422]), 0);

// memory_copy.wast:1716
assert_return(() => call($12, "load8_u", [39621]), 0);

// memory_copy.wast:1717
assert_return(() => call($12, "load8_u", [39820]), 0);

// memory_copy.wast:1718
assert_return(() => call($12, "load8_u", [40019]), 0);

// memory_copy.wast:1719
assert_return(() => call($12, "load8_u", [40218]), 0);

// memory_copy.wast:1720
assert_return(() => call($12, "load8_u", [40417]), 0);

// memory_copy.wast:1721
assert_return(() => call($12, "load8_u", [40616]), 0);

// memory_copy.wast:1722
assert_return(() => call($12, "load8_u", [40815]), 0);

// memory_copy.wast:1723
assert_return(() => call($12, "load8_u", [41014]), 0);

// memory_copy.wast:1724
assert_return(() => call($12, "load8_u", [41213]), 0);

// memory_copy.wast:1725
assert_return(() => call($12, "load8_u", [41412]), 0);

// memory_copy.wast:1726
assert_return(() => call($12, "load8_u", [41611]), 0);

// memory_copy.wast:1727
assert_return(() => call($12, "load8_u", [41810]), 0);

// memory_copy.wast:1728
assert_return(() => call($12, "load8_u", [42009]), 0);

// memory_copy.wast:1729
assert_return(() => call($12, "load8_u", [42208]), 0);

// memory_copy.wast:1730
assert_return(() => call($12, "load8_u", [42407]), 0);

// memory_copy.wast:1731
assert_return(() => call($12, "load8_u", [42606]), 0);

// memory_copy.wast:1732
assert_return(() => call($12, "load8_u", [42805]), 0);

// memory_copy.wast:1733
assert_return(() => call($12, "load8_u", [43004]), 0);

// memory_copy.wast:1734
assert_return(() => call($12, "load8_u", [43203]), 0);

// memory_copy.wast:1735
assert_return(() => call($12, "load8_u", [43402]), 0);

// memory_copy.wast:1736
assert_return(() => call($12, "load8_u", [43601]), 0);

// memory_copy.wast:1737
assert_return(() => call($12, "load8_u", [43800]), 0);

// memory_copy.wast:1738
assert_return(() => call($12, "load8_u", [43999]), 0);

// memory_copy.wast:1739
assert_return(() => call($12, "load8_u", [44198]), 0);

// memory_copy.wast:1740
assert_return(() => call($12, "load8_u", [44397]), 0);

// memory_copy.wast:1741
assert_return(() => call($12, "load8_u", [44596]), 0);

// memory_copy.wast:1742
assert_return(() => call($12, "load8_u", [44795]), 0);

// memory_copy.wast:1743
assert_return(() => call($12, "load8_u", [44994]), 0);

// memory_copy.wast:1744
assert_return(() => call($12, "load8_u", [45193]), 0);

// memory_copy.wast:1745
assert_return(() => call($12, "load8_u", [45392]), 0);

// memory_copy.wast:1746
assert_return(() => call($12, "load8_u", [45591]), 0);

// memory_copy.wast:1747
assert_return(() => call($12, "load8_u", [45790]), 0);

// memory_copy.wast:1748
assert_return(() => call($12, "load8_u", [45989]), 0);

// memory_copy.wast:1749
assert_return(() => call($12, "load8_u", [46188]), 0);

// memory_copy.wast:1750
assert_return(() => call($12, "load8_u", [46387]), 0);

// memory_copy.wast:1751
assert_return(() => call($12, "load8_u", [46586]), 0);

// memory_copy.wast:1752
assert_return(() => call($12, "load8_u", [46785]), 0);

// memory_copy.wast:1753
assert_return(() => call($12, "load8_u", [46984]), 0);

// memory_copy.wast:1754
assert_return(() => call($12, "load8_u", [47183]), 0);

// memory_copy.wast:1755
assert_return(() => call($12, "load8_u", [47382]), 0);

// memory_copy.wast:1756
assert_return(() => call($12, "load8_u", [47581]), 0);

// memory_copy.wast:1757
assert_return(() => call($12, "load8_u", [47780]), 0);

// memory_copy.wast:1758
assert_return(() => call($12, "load8_u", [47979]), 0);

// memory_copy.wast:1759
assert_return(() => call($12, "load8_u", [48178]), 0);

// memory_copy.wast:1760
assert_return(() => call($12, "load8_u", [48377]), 0);

// memory_copy.wast:1761
assert_return(() => call($12, "load8_u", [48576]), 0);

// memory_copy.wast:1762
assert_return(() => call($12, "load8_u", [48775]), 0);

// memory_copy.wast:1763
assert_return(() => call($12, "load8_u", [48974]), 0);

// memory_copy.wast:1764
assert_return(() => call($12, "load8_u", [49173]), 0);

// memory_copy.wast:1765
assert_return(() => call($12, "load8_u", [49372]), 0);

// memory_copy.wast:1766
assert_return(() => call($12, "load8_u", [49571]), 0);

// memory_copy.wast:1767
assert_return(() => call($12, "load8_u", [49770]), 0);

// memory_copy.wast:1768
assert_return(() => call($12, "load8_u", [49969]), 0);

// memory_copy.wast:1769
assert_return(() => call($12, "load8_u", [50168]), 0);

// memory_copy.wast:1770
assert_return(() => call($12, "load8_u", [50367]), 0);

// memory_copy.wast:1771
assert_return(() => call($12, "load8_u", [50566]), 0);

// memory_copy.wast:1772
assert_return(() => call($12, "load8_u", [50765]), 0);

// memory_copy.wast:1773
assert_return(() => call($12, "load8_u", [50964]), 0);

// memory_copy.wast:1774
assert_return(() => call($12, "load8_u", [51163]), 0);

// memory_copy.wast:1775
assert_return(() => call($12, "load8_u", [51362]), 0);

// memory_copy.wast:1776
assert_return(() => call($12, "load8_u", [51561]), 0);

// memory_copy.wast:1777
assert_return(() => call($12, "load8_u", [51760]), 0);

// memory_copy.wast:1778
assert_return(() => call($12, "load8_u", [51959]), 0);

// memory_copy.wast:1779
assert_return(() => call($12, "load8_u", [52158]), 0);

// memory_copy.wast:1780
assert_return(() => call($12, "load8_u", [52357]), 0);

// memory_copy.wast:1781
assert_return(() => call($12, "load8_u", [52556]), 0);

// memory_copy.wast:1782
assert_return(() => call($12, "load8_u", [52755]), 0);

// memory_copy.wast:1783
assert_return(() => call($12, "load8_u", [52954]), 0);

// memory_copy.wast:1784
assert_return(() => call($12, "load8_u", [53153]), 0);

// memory_copy.wast:1785
assert_return(() => call($12, "load8_u", [53352]), 0);

// memory_copy.wast:1786
assert_return(() => call($12, "load8_u", [53551]), 0);

// memory_copy.wast:1787
assert_return(() => call($12, "load8_u", [53750]), 0);

// memory_copy.wast:1788
assert_return(() => call($12, "load8_u", [53949]), 0);

// memory_copy.wast:1789
assert_return(() => call($12, "load8_u", [54148]), 0);

// memory_copy.wast:1790
assert_return(() => call($12, "load8_u", [54347]), 0);

// memory_copy.wast:1791
assert_return(() => call($12, "load8_u", [54546]), 0);

// memory_copy.wast:1792
assert_return(() => call($12, "load8_u", [54745]), 0);

// memory_copy.wast:1793
assert_return(() => call($12, "load8_u", [54944]), 0);

// memory_copy.wast:1794
assert_return(() => call($12, "load8_u", [55143]), 0);

// memory_copy.wast:1795
assert_return(() => call($12, "load8_u", [55342]), 0);

// memory_copy.wast:1796
assert_return(() => call($12, "load8_u", [55541]), 0);

// memory_copy.wast:1797
assert_return(() => call($12, "load8_u", [55740]), 0);

// memory_copy.wast:1798
assert_return(() => call($12, "load8_u", [55939]), 0);

// memory_copy.wast:1799
assert_return(() => call($12, "load8_u", [56138]), 0);

// memory_copy.wast:1800
assert_return(() => call($12, "load8_u", [56337]), 0);

// memory_copy.wast:1801
assert_return(() => call($12, "load8_u", [56536]), 0);

// memory_copy.wast:1802
assert_return(() => call($12, "load8_u", [56735]), 0);

// memory_copy.wast:1803
assert_return(() => call($12, "load8_u", [56934]), 0);

// memory_copy.wast:1804
assert_return(() => call($12, "load8_u", [57133]), 0);

// memory_copy.wast:1805
assert_return(() => call($12, "load8_u", [57332]), 0);

// memory_copy.wast:1806
assert_return(() => call($12, "load8_u", [57531]), 0);

// memory_copy.wast:1807
assert_return(() => call($12, "load8_u", [57730]), 0);

// memory_copy.wast:1808
assert_return(() => call($12, "load8_u", [57929]), 0);

// memory_copy.wast:1809
assert_return(() => call($12, "load8_u", [58128]), 0);

// memory_copy.wast:1810
assert_return(() => call($12, "load8_u", [58327]), 0);

// memory_copy.wast:1811
assert_return(() => call($12, "load8_u", [58526]), 0);

// memory_copy.wast:1812
assert_return(() => call($12, "load8_u", [58725]), 0);

// memory_copy.wast:1813
assert_return(() => call($12, "load8_u", [58924]), 0);

// memory_copy.wast:1814
assert_return(() => call($12, "load8_u", [59123]), 0);

// memory_copy.wast:1815
assert_return(() => call($12, "load8_u", [59322]), 0);

// memory_copy.wast:1816
assert_return(() => call($12, "load8_u", [59521]), 0);

// memory_copy.wast:1817
assert_return(() => call($12, "load8_u", [59720]), 0);

// memory_copy.wast:1818
assert_return(() => call($12, "load8_u", [59919]), 0);

// memory_copy.wast:1819
assert_return(() => call($12, "load8_u", [60118]), 0);

// memory_copy.wast:1820
assert_return(() => call($12, "load8_u", [60317]), 0);

// memory_copy.wast:1821
assert_return(() => call($12, "load8_u", [60516]), 0);

// memory_copy.wast:1822
assert_return(() => call($12, "load8_u", [60715]), 0);

// memory_copy.wast:1823
assert_return(() => call($12, "load8_u", [60914]), 0);

// memory_copy.wast:1824
assert_return(() => call($12, "load8_u", [61113]), 0);

// memory_copy.wast:1825
assert_return(() => call($12, "load8_u", [61312]), 0);

// memory_copy.wast:1826
assert_return(() => call($12, "load8_u", [61511]), 0);

// memory_copy.wast:1827
assert_return(() => call($12, "load8_u", [61710]), 0);

// memory_copy.wast:1828
assert_return(() => call($12, "load8_u", [61909]), 0);

// memory_copy.wast:1829
assert_return(() => call($12, "load8_u", [62108]), 0);

// memory_copy.wast:1830
assert_return(() => call($12, "load8_u", [62307]), 0);

// memory_copy.wast:1831
assert_return(() => call($12, "load8_u", [62506]), 0);

// memory_copy.wast:1832
assert_return(() => call($12, "load8_u", [62705]), 0);

// memory_copy.wast:1833
assert_return(() => call($12, "load8_u", [62904]), 0);

// memory_copy.wast:1834
assert_return(() => call($12, "load8_u", [63103]), 0);

// memory_copy.wast:1835
assert_return(() => call($12, "load8_u", [63302]), 0);

// memory_copy.wast:1836
assert_return(() => call($12, "load8_u", [63501]), 0);

// memory_copy.wast:1837
assert_return(() => call($12, "load8_u", [63700]), 0);

// memory_copy.wast:1838
assert_return(() => call($12, "load8_u", [63899]), 0);

// memory_copy.wast:1839
assert_return(() => call($12, "load8_u", [64098]), 0);

// memory_copy.wast:1840
assert_return(() => call($12, "load8_u", [64297]), 0);

// memory_copy.wast:1841
assert_return(() => call($12, "load8_u", [64496]), 0);

// memory_copy.wast:1842
assert_return(() => call($12, "load8_u", [64695]), 0);

// memory_copy.wast:1843
assert_return(() => call($12, "load8_u", [64894]), 0);

// memory_copy.wast:1844
assert_return(() => call($12, "load8_u", [65093]), 0);

// memory_copy.wast:1845
assert_return(() => call($12, "load8_u", [65292]), 0);

// memory_copy.wast:1846
assert_return(() => call($12, "load8_u", [65491]), 0);

// memory_copy.wast:1847
assert_return(() => call($12, "load8_u", [65515]), 0);

// memory_copy.wast:1848
assert_return(() => call($12, "load8_u", [65516]), 1);

// memory_copy.wast:1849
assert_return(() => call($12, "load8_u", [65517]), 2);

// memory_copy.wast:1850
assert_return(() => call($12, "load8_u", [65518]), 3);

// memory_copy.wast:1851
assert_return(() => call($12, "load8_u", [65519]), 4);

// memory_copy.wast:1852
assert_return(() => call($12, "load8_u", [65520]), 5);

// memory_copy.wast:1853
assert_return(() => call($12, "load8_u", [65521]), 6);

// memory_copy.wast:1854
assert_return(() => call($12, "load8_u", [65522]), 7);

// memory_copy.wast:1855
assert_return(() => call($12, "load8_u", [65523]), 8);

// memory_copy.wast:1856
assert_return(() => call($12, "load8_u", [65524]), 9);

// memory_copy.wast:1857
assert_return(() => call($12, "load8_u", [65525]), 10);

// memory_copy.wast:1858
assert_return(() => call($12, "load8_u", [65526]), 11);

// memory_copy.wast:1859
assert_return(() => call($12, "load8_u", [65527]), 12);

// memory_copy.wast:1860
assert_return(() => call($12, "load8_u", [65528]), 13);

// memory_copy.wast:1861
assert_return(() => call($12, "load8_u", [65529]), 14);

// memory_copy.wast:1862
assert_return(() => call($12, "load8_u", [65530]), 15);

// memory_copy.wast:1863
assert_return(() => call($12, "load8_u", [65531]), 16);

// memory_copy.wast:1864
assert_return(() => call($12, "load8_u", [65532]), 17);

// memory_copy.wast:1865
assert_return(() => call($12, "load8_u", [65533]), 18);

// memory_copy.wast:1866
assert_return(() => call($12, "load8_u", [65534]), 19);

// memory_copy.wast:1867
assert_return(() => call($12, "load8_u", [65535]), 20);

// memory_copy.wast:1869
let $13 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xce\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:1877
assert_trap(() => call($13, "run", [65516, 65486, 40]));

// memory_copy.wast:1880
assert_return(() => call($13, "load8_u", [198]), 0);

// memory_copy.wast:1881
assert_return(() => call($13, "load8_u", [397]), 0);

// memory_copy.wast:1882
assert_return(() => call($13, "load8_u", [596]), 0);

// memory_copy.wast:1883
assert_return(() => call($13, "load8_u", [795]), 0);

// memory_copy.wast:1884
assert_return(() => call($13, "load8_u", [994]), 0);

// memory_copy.wast:1885
assert_return(() => call($13, "load8_u", [1193]), 0);

// memory_copy.wast:1886
assert_return(() => call($13, "load8_u", [1392]), 0);

// memory_copy.wast:1887
assert_return(() => call($13, "load8_u", [1591]), 0);

// memory_copy.wast:1888
assert_return(() => call($13, "load8_u", [1790]), 0);

// memory_copy.wast:1889
assert_return(() => call($13, "load8_u", [1989]), 0);

// memory_copy.wast:1890
assert_return(() => call($13, "load8_u", [2188]), 0);

// memory_copy.wast:1891
assert_return(() => call($13, "load8_u", [2387]), 0);

// memory_copy.wast:1892
assert_return(() => call($13, "load8_u", [2586]), 0);

// memory_copy.wast:1893
assert_return(() => call($13, "load8_u", [2785]), 0);

// memory_copy.wast:1894
assert_return(() => call($13, "load8_u", [2984]), 0);

// memory_copy.wast:1895
assert_return(() => call($13, "load8_u", [3183]), 0);

// memory_copy.wast:1896
assert_return(() => call($13, "load8_u", [3382]), 0);

// memory_copy.wast:1897
assert_return(() => call($13, "load8_u", [3581]), 0);

// memory_copy.wast:1898
assert_return(() => call($13, "load8_u", [3780]), 0);

// memory_copy.wast:1899
assert_return(() => call($13, "load8_u", [3979]), 0);

// memory_copy.wast:1900
assert_return(() => call($13, "load8_u", [4178]), 0);

// memory_copy.wast:1901
assert_return(() => call($13, "load8_u", [4377]), 0);

// memory_copy.wast:1902
assert_return(() => call($13, "load8_u", [4576]), 0);

// memory_copy.wast:1903
assert_return(() => call($13, "load8_u", [4775]), 0);

// memory_copy.wast:1904
assert_return(() => call($13, "load8_u", [4974]), 0);

// memory_copy.wast:1905
assert_return(() => call($13, "load8_u", [5173]), 0);

// memory_copy.wast:1906
assert_return(() => call($13, "load8_u", [5372]), 0);

// memory_copy.wast:1907
assert_return(() => call($13, "load8_u", [5571]), 0);

// memory_copy.wast:1908
assert_return(() => call($13, "load8_u", [5770]), 0);

// memory_copy.wast:1909
assert_return(() => call($13, "load8_u", [5969]), 0);

// memory_copy.wast:1910
assert_return(() => call($13, "load8_u", [6168]), 0);

// memory_copy.wast:1911
assert_return(() => call($13, "load8_u", [6367]), 0);

// memory_copy.wast:1912
assert_return(() => call($13, "load8_u", [6566]), 0);

// memory_copy.wast:1913
assert_return(() => call($13, "load8_u", [6765]), 0);

// memory_copy.wast:1914
assert_return(() => call($13, "load8_u", [6964]), 0);

// memory_copy.wast:1915
assert_return(() => call($13, "load8_u", [7163]), 0);

// memory_copy.wast:1916
assert_return(() => call($13, "load8_u", [7362]), 0);

// memory_copy.wast:1917
assert_return(() => call($13, "load8_u", [7561]), 0);

// memory_copy.wast:1918
assert_return(() => call($13, "load8_u", [7760]), 0);

// memory_copy.wast:1919
assert_return(() => call($13, "load8_u", [7959]), 0);

// memory_copy.wast:1920
assert_return(() => call($13, "load8_u", [8158]), 0);

// memory_copy.wast:1921
assert_return(() => call($13, "load8_u", [8357]), 0);

// memory_copy.wast:1922
assert_return(() => call($13, "load8_u", [8556]), 0);

// memory_copy.wast:1923
assert_return(() => call($13, "load8_u", [8755]), 0);

// memory_copy.wast:1924
assert_return(() => call($13, "load8_u", [8954]), 0);

// memory_copy.wast:1925
assert_return(() => call($13, "load8_u", [9153]), 0);

// memory_copy.wast:1926
assert_return(() => call($13, "load8_u", [9352]), 0);

// memory_copy.wast:1927
assert_return(() => call($13, "load8_u", [9551]), 0);

// memory_copy.wast:1928
assert_return(() => call($13, "load8_u", [9750]), 0);

// memory_copy.wast:1929
assert_return(() => call($13, "load8_u", [9949]), 0);

// memory_copy.wast:1930
assert_return(() => call($13, "load8_u", [10148]), 0);

// memory_copy.wast:1931
assert_return(() => call($13, "load8_u", [10347]), 0);

// memory_copy.wast:1932
assert_return(() => call($13, "load8_u", [10546]), 0);

// memory_copy.wast:1933
assert_return(() => call($13, "load8_u", [10745]), 0);

// memory_copy.wast:1934
assert_return(() => call($13, "load8_u", [10944]), 0);

// memory_copy.wast:1935
assert_return(() => call($13, "load8_u", [11143]), 0);

// memory_copy.wast:1936
assert_return(() => call($13, "load8_u", [11342]), 0);

// memory_copy.wast:1937
assert_return(() => call($13, "load8_u", [11541]), 0);

// memory_copy.wast:1938
assert_return(() => call($13, "load8_u", [11740]), 0);

// memory_copy.wast:1939
assert_return(() => call($13, "load8_u", [11939]), 0);

// memory_copy.wast:1940
assert_return(() => call($13, "load8_u", [12138]), 0);

// memory_copy.wast:1941
assert_return(() => call($13, "load8_u", [12337]), 0);

// memory_copy.wast:1942
assert_return(() => call($13, "load8_u", [12536]), 0);

// memory_copy.wast:1943
assert_return(() => call($13, "load8_u", [12735]), 0);

// memory_copy.wast:1944
assert_return(() => call($13, "load8_u", [12934]), 0);

// memory_copy.wast:1945
assert_return(() => call($13, "load8_u", [13133]), 0);

// memory_copy.wast:1946
assert_return(() => call($13, "load8_u", [13332]), 0);

// memory_copy.wast:1947
assert_return(() => call($13, "load8_u", [13531]), 0);

// memory_copy.wast:1948
assert_return(() => call($13, "load8_u", [13730]), 0);

// memory_copy.wast:1949
assert_return(() => call($13, "load8_u", [13929]), 0);

// memory_copy.wast:1950
assert_return(() => call($13, "load8_u", [14128]), 0);

// memory_copy.wast:1951
assert_return(() => call($13, "load8_u", [14327]), 0);

// memory_copy.wast:1952
assert_return(() => call($13, "load8_u", [14526]), 0);

// memory_copy.wast:1953
assert_return(() => call($13, "load8_u", [14725]), 0);

// memory_copy.wast:1954
assert_return(() => call($13, "load8_u", [14924]), 0);

// memory_copy.wast:1955
assert_return(() => call($13, "load8_u", [15123]), 0);

// memory_copy.wast:1956
assert_return(() => call($13, "load8_u", [15322]), 0);

// memory_copy.wast:1957
assert_return(() => call($13, "load8_u", [15521]), 0);

// memory_copy.wast:1958
assert_return(() => call($13, "load8_u", [15720]), 0);

// memory_copy.wast:1959
assert_return(() => call($13, "load8_u", [15919]), 0);

// memory_copy.wast:1960
assert_return(() => call($13, "load8_u", [16118]), 0);

// memory_copy.wast:1961
assert_return(() => call($13, "load8_u", [16317]), 0);

// memory_copy.wast:1962
assert_return(() => call($13, "load8_u", [16516]), 0);

// memory_copy.wast:1963
assert_return(() => call($13, "load8_u", [16715]), 0);

// memory_copy.wast:1964
assert_return(() => call($13, "load8_u", [16914]), 0);

// memory_copy.wast:1965
assert_return(() => call($13, "load8_u", [17113]), 0);

// memory_copy.wast:1966
assert_return(() => call($13, "load8_u", [17312]), 0);

// memory_copy.wast:1967
assert_return(() => call($13, "load8_u", [17511]), 0);

// memory_copy.wast:1968
assert_return(() => call($13, "load8_u", [17710]), 0);

// memory_copy.wast:1969
assert_return(() => call($13, "load8_u", [17909]), 0);

// memory_copy.wast:1970
assert_return(() => call($13, "load8_u", [18108]), 0);

// memory_copy.wast:1971
assert_return(() => call($13, "load8_u", [18307]), 0);

// memory_copy.wast:1972
assert_return(() => call($13, "load8_u", [18506]), 0);

// memory_copy.wast:1973
assert_return(() => call($13, "load8_u", [18705]), 0);

// memory_copy.wast:1974
assert_return(() => call($13, "load8_u", [18904]), 0);

// memory_copy.wast:1975
assert_return(() => call($13, "load8_u", [19103]), 0);

// memory_copy.wast:1976
assert_return(() => call($13, "load8_u", [19302]), 0);

// memory_copy.wast:1977
assert_return(() => call($13, "load8_u", [19501]), 0);

// memory_copy.wast:1978
assert_return(() => call($13, "load8_u", [19700]), 0);

// memory_copy.wast:1979
assert_return(() => call($13, "load8_u", [19899]), 0);

// memory_copy.wast:1980
assert_return(() => call($13, "load8_u", [20098]), 0);

// memory_copy.wast:1981
assert_return(() => call($13, "load8_u", [20297]), 0);

// memory_copy.wast:1982
assert_return(() => call($13, "load8_u", [20496]), 0);

// memory_copy.wast:1983
assert_return(() => call($13, "load8_u", [20695]), 0);

// memory_copy.wast:1984
assert_return(() => call($13, "load8_u", [20894]), 0);

// memory_copy.wast:1985
assert_return(() => call($13, "load8_u", [21093]), 0);

// memory_copy.wast:1986
assert_return(() => call($13, "load8_u", [21292]), 0);

// memory_copy.wast:1987
assert_return(() => call($13, "load8_u", [21491]), 0);

// memory_copy.wast:1988
assert_return(() => call($13, "load8_u", [21690]), 0);

// memory_copy.wast:1989
assert_return(() => call($13, "load8_u", [21889]), 0);

// memory_copy.wast:1990
assert_return(() => call($13, "load8_u", [22088]), 0);

// memory_copy.wast:1991
assert_return(() => call($13, "load8_u", [22287]), 0);

// memory_copy.wast:1992
assert_return(() => call($13, "load8_u", [22486]), 0);

// memory_copy.wast:1993
assert_return(() => call($13, "load8_u", [22685]), 0);

// memory_copy.wast:1994
assert_return(() => call($13, "load8_u", [22884]), 0);

// memory_copy.wast:1995
assert_return(() => call($13, "load8_u", [23083]), 0);

// memory_copy.wast:1996
assert_return(() => call($13, "load8_u", [23282]), 0);

// memory_copy.wast:1997
assert_return(() => call($13, "load8_u", [23481]), 0);

// memory_copy.wast:1998
assert_return(() => call($13, "load8_u", [23680]), 0);

// memory_copy.wast:1999
assert_return(() => call($13, "load8_u", [23879]), 0);

// memory_copy.wast:2000
assert_return(() => call($13, "load8_u", [24078]), 0);

// memory_copy.wast:2001
assert_return(() => call($13, "load8_u", [24277]), 0);

// memory_copy.wast:2002
assert_return(() => call($13, "load8_u", [24476]), 0);

// memory_copy.wast:2003
assert_return(() => call($13, "load8_u", [24675]), 0);

// memory_copy.wast:2004
assert_return(() => call($13, "load8_u", [24874]), 0);

// memory_copy.wast:2005
assert_return(() => call($13, "load8_u", [25073]), 0);

// memory_copy.wast:2006
assert_return(() => call($13, "load8_u", [25272]), 0);

// memory_copy.wast:2007
assert_return(() => call($13, "load8_u", [25471]), 0);

// memory_copy.wast:2008
assert_return(() => call($13, "load8_u", [25670]), 0);

// memory_copy.wast:2009
assert_return(() => call($13, "load8_u", [25869]), 0);

// memory_copy.wast:2010
assert_return(() => call($13, "load8_u", [26068]), 0);

// memory_copy.wast:2011
assert_return(() => call($13, "load8_u", [26267]), 0);

// memory_copy.wast:2012
assert_return(() => call($13, "load8_u", [26466]), 0);

// memory_copy.wast:2013
assert_return(() => call($13, "load8_u", [26665]), 0);

// memory_copy.wast:2014
assert_return(() => call($13, "load8_u", [26864]), 0);

// memory_copy.wast:2015
assert_return(() => call($13, "load8_u", [27063]), 0);

// memory_copy.wast:2016
assert_return(() => call($13, "load8_u", [27262]), 0);

// memory_copy.wast:2017
assert_return(() => call($13, "load8_u", [27461]), 0);

// memory_copy.wast:2018
assert_return(() => call($13, "load8_u", [27660]), 0);

// memory_copy.wast:2019
assert_return(() => call($13, "load8_u", [27859]), 0);

// memory_copy.wast:2020
assert_return(() => call($13, "load8_u", [28058]), 0);

// memory_copy.wast:2021
assert_return(() => call($13, "load8_u", [28257]), 0);

// memory_copy.wast:2022
assert_return(() => call($13, "load8_u", [28456]), 0);

// memory_copy.wast:2023
assert_return(() => call($13, "load8_u", [28655]), 0);

// memory_copy.wast:2024
assert_return(() => call($13, "load8_u", [28854]), 0);

// memory_copy.wast:2025
assert_return(() => call($13, "load8_u", [29053]), 0);

// memory_copy.wast:2026
assert_return(() => call($13, "load8_u", [29252]), 0);

// memory_copy.wast:2027
assert_return(() => call($13, "load8_u", [29451]), 0);

// memory_copy.wast:2028
assert_return(() => call($13, "load8_u", [29650]), 0);

// memory_copy.wast:2029
assert_return(() => call($13, "load8_u", [29849]), 0);

// memory_copy.wast:2030
assert_return(() => call($13, "load8_u", [30048]), 0);

// memory_copy.wast:2031
assert_return(() => call($13, "load8_u", [30247]), 0);

// memory_copy.wast:2032
assert_return(() => call($13, "load8_u", [30446]), 0);

// memory_copy.wast:2033
assert_return(() => call($13, "load8_u", [30645]), 0);

// memory_copy.wast:2034
assert_return(() => call($13, "load8_u", [30844]), 0);

// memory_copy.wast:2035
assert_return(() => call($13, "load8_u", [31043]), 0);

// memory_copy.wast:2036
assert_return(() => call($13, "load8_u", [31242]), 0);

// memory_copy.wast:2037
assert_return(() => call($13, "load8_u", [31441]), 0);

// memory_copy.wast:2038
assert_return(() => call($13, "load8_u", [31640]), 0);

// memory_copy.wast:2039
assert_return(() => call($13, "load8_u", [31839]), 0);

// memory_copy.wast:2040
assert_return(() => call($13, "load8_u", [32038]), 0);

// memory_copy.wast:2041
assert_return(() => call($13, "load8_u", [32237]), 0);

// memory_copy.wast:2042
assert_return(() => call($13, "load8_u", [32436]), 0);

// memory_copy.wast:2043
assert_return(() => call($13, "load8_u", [32635]), 0);

// memory_copy.wast:2044
assert_return(() => call($13, "load8_u", [32834]), 0);

// memory_copy.wast:2045
assert_return(() => call($13, "load8_u", [33033]), 0);

// memory_copy.wast:2046
assert_return(() => call($13, "load8_u", [33232]), 0);

// memory_copy.wast:2047
assert_return(() => call($13, "load8_u", [33431]), 0);

// memory_copy.wast:2048
assert_return(() => call($13, "load8_u", [33630]), 0);

// memory_copy.wast:2049
assert_return(() => call($13, "load8_u", [33829]), 0);

// memory_copy.wast:2050
assert_return(() => call($13, "load8_u", [34028]), 0);

// memory_copy.wast:2051
assert_return(() => call($13, "load8_u", [34227]), 0);

// memory_copy.wast:2052
assert_return(() => call($13, "load8_u", [34426]), 0);

// memory_copy.wast:2053
assert_return(() => call($13, "load8_u", [34625]), 0);

// memory_copy.wast:2054
assert_return(() => call($13, "load8_u", [34824]), 0);

// memory_copy.wast:2055
assert_return(() => call($13, "load8_u", [35023]), 0);

// memory_copy.wast:2056
assert_return(() => call($13, "load8_u", [35222]), 0);

// memory_copy.wast:2057
assert_return(() => call($13, "load8_u", [35421]), 0);

// memory_copy.wast:2058
assert_return(() => call($13, "load8_u", [35620]), 0);

// memory_copy.wast:2059
assert_return(() => call($13, "load8_u", [35819]), 0);

// memory_copy.wast:2060
assert_return(() => call($13, "load8_u", [36018]), 0);

// memory_copy.wast:2061
assert_return(() => call($13, "load8_u", [36217]), 0);

// memory_copy.wast:2062
assert_return(() => call($13, "load8_u", [36416]), 0);

// memory_copy.wast:2063
assert_return(() => call($13, "load8_u", [36615]), 0);

// memory_copy.wast:2064
assert_return(() => call($13, "load8_u", [36814]), 0);

// memory_copy.wast:2065
assert_return(() => call($13, "load8_u", [37013]), 0);

// memory_copy.wast:2066
assert_return(() => call($13, "load8_u", [37212]), 0);

// memory_copy.wast:2067
assert_return(() => call($13, "load8_u", [37411]), 0);

// memory_copy.wast:2068
assert_return(() => call($13, "load8_u", [37610]), 0);

// memory_copy.wast:2069
assert_return(() => call($13, "load8_u", [37809]), 0);

// memory_copy.wast:2070
assert_return(() => call($13, "load8_u", [38008]), 0);

// memory_copy.wast:2071
assert_return(() => call($13, "load8_u", [38207]), 0);

// memory_copy.wast:2072
assert_return(() => call($13, "load8_u", [38406]), 0);

// memory_copy.wast:2073
assert_return(() => call($13, "load8_u", [38605]), 0);

// memory_copy.wast:2074
assert_return(() => call($13, "load8_u", [38804]), 0);

// memory_copy.wast:2075
assert_return(() => call($13, "load8_u", [39003]), 0);

// memory_copy.wast:2076
assert_return(() => call($13, "load8_u", [39202]), 0);

// memory_copy.wast:2077
assert_return(() => call($13, "load8_u", [39401]), 0);

// memory_copy.wast:2078
assert_return(() => call($13, "load8_u", [39600]), 0);

// memory_copy.wast:2079
assert_return(() => call($13, "load8_u", [39799]), 0);

// memory_copy.wast:2080
assert_return(() => call($13, "load8_u", [39998]), 0);

// memory_copy.wast:2081
assert_return(() => call($13, "load8_u", [40197]), 0);

// memory_copy.wast:2082
assert_return(() => call($13, "load8_u", [40396]), 0);

// memory_copy.wast:2083
assert_return(() => call($13, "load8_u", [40595]), 0);

// memory_copy.wast:2084
assert_return(() => call($13, "load8_u", [40794]), 0);

// memory_copy.wast:2085
assert_return(() => call($13, "load8_u", [40993]), 0);

// memory_copy.wast:2086
assert_return(() => call($13, "load8_u", [41192]), 0);

// memory_copy.wast:2087
assert_return(() => call($13, "load8_u", [41391]), 0);

// memory_copy.wast:2088
assert_return(() => call($13, "load8_u", [41590]), 0);

// memory_copy.wast:2089
assert_return(() => call($13, "load8_u", [41789]), 0);

// memory_copy.wast:2090
assert_return(() => call($13, "load8_u", [41988]), 0);

// memory_copy.wast:2091
assert_return(() => call($13, "load8_u", [42187]), 0);

// memory_copy.wast:2092
assert_return(() => call($13, "load8_u", [42386]), 0);

// memory_copy.wast:2093
assert_return(() => call($13, "load8_u", [42585]), 0);

// memory_copy.wast:2094
assert_return(() => call($13, "load8_u", [42784]), 0);

// memory_copy.wast:2095
assert_return(() => call($13, "load8_u", [42983]), 0);

// memory_copy.wast:2096
assert_return(() => call($13, "load8_u", [43182]), 0);

// memory_copy.wast:2097
assert_return(() => call($13, "load8_u", [43381]), 0);

// memory_copy.wast:2098
assert_return(() => call($13, "load8_u", [43580]), 0);

// memory_copy.wast:2099
assert_return(() => call($13, "load8_u", [43779]), 0);

// memory_copy.wast:2100
assert_return(() => call($13, "load8_u", [43978]), 0);

// memory_copy.wast:2101
assert_return(() => call($13, "load8_u", [44177]), 0);

// memory_copy.wast:2102
assert_return(() => call($13, "load8_u", [44376]), 0);

// memory_copy.wast:2103
assert_return(() => call($13, "load8_u", [44575]), 0);

// memory_copy.wast:2104
assert_return(() => call($13, "load8_u", [44774]), 0);

// memory_copy.wast:2105
assert_return(() => call($13, "load8_u", [44973]), 0);

// memory_copy.wast:2106
assert_return(() => call($13, "load8_u", [45172]), 0);

// memory_copy.wast:2107
assert_return(() => call($13, "load8_u", [45371]), 0);

// memory_copy.wast:2108
assert_return(() => call($13, "load8_u", [45570]), 0);

// memory_copy.wast:2109
assert_return(() => call($13, "load8_u", [45769]), 0);

// memory_copy.wast:2110
assert_return(() => call($13, "load8_u", [45968]), 0);

// memory_copy.wast:2111
assert_return(() => call($13, "load8_u", [46167]), 0);

// memory_copy.wast:2112
assert_return(() => call($13, "load8_u", [46366]), 0);

// memory_copy.wast:2113
assert_return(() => call($13, "load8_u", [46565]), 0);

// memory_copy.wast:2114
assert_return(() => call($13, "load8_u", [46764]), 0);

// memory_copy.wast:2115
assert_return(() => call($13, "load8_u", [46963]), 0);

// memory_copy.wast:2116
assert_return(() => call($13, "load8_u", [47162]), 0);

// memory_copy.wast:2117
assert_return(() => call($13, "load8_u", [47361]), 0);

// memory_copy.wast:2118
assert_return(() => call($13, "load8_u", [47560]), 0);

// memory_copy.wast:2119
assert_return(() => call($13, "load8_u", [47759]), 0);

// memory_copy.wast:2120
assert_return(() => call($13, "load8_u", [47958]), 0);

// memory_copy.wast:2121
assert_return(() => call($13, "load8_u", [48157]), 0);

// memory_copy.wast:2122
assert_return(() => call($13, "load8_u", [48356]), 0);

// memory_copy.wast:2123
assert_return(() => call($13, "load8_u", [48555]), 0);

// memory_copy.wast:2124
assert_return(() => call($13, "load8_u", [48754]), 0);

// memory_copy.wast:2125
assert_return(() => call($13, "load8_u", [48953]), 0);

// memory_copy.wast:2126
assert_return(() => call($13, "load8_u", [49152]), 0);

// memory_copy.wast:2127
assert_return(() => call($13, "load8_u", [49351]), 0);

// memory_copy.wast:2128
assert_return(() => call($13, "load8_u", [49550]), 0);

// memory_copy.wast:2129
assert_return(() => call($13, "load8_u", [49749]), 0);

// memory_copy.wast:2130
assert_return(() => call($13, "load8_u", [49948]), 0);

// memory_copy.wast:2131
assert_return(() => call($13, "load8_u", [50147]), 0);

// memory_copy.wast:2132
assert_return(() => call($13, "load8_u", [50346]), 0);

// memory_copy.wast:2133
assert_return(() => call($13, "load8_u", [50545]), 0);

// memory_copy.wast:2134
assert_return(() => call($13, "load8_u", [50744]), 0);

// memory_copy.wast:2135
assert_return(() => call($13, "load8_u", [50943]), 0);

// memory_copy.wast:2136
assert_return(() => call($13, "load8_u", [51142]), 0);

// memory_copy.wast:2137
assert_return(() => call($13, "load8_u", [51341]), 0);

// memory_copy.wast:2138
assert_return(() => call($13, "load8_u", [51540]), 0);

// memory_copy.wast:2139
assert_return(() => call($13, "load8_u", [51739]), 0);

// memory_copy.wast:2140
assert_return(() => call($13, "load8_u", [51938]), 0);

// memory_copy.wast:2141
assert_return(() => call($13, "load8_u", [52137]), 0);

// memory_copy.wast:2142
assert_return(() => call($13, "load8_u", [52336]), 0);

// memory_copy.wast:2143
assert_return(() => call($13, "load8_u", [52535]), 0);

// memory_copy.wast:2144
assert_return(() => call($13, "load8_u", [52734]), 0);

// memory_copy.wast:2145
assert_return(() => call($13, "load8_u", [52933]), 0);

// memory_copy.wast:2146
assert_return(() => call($13, "load8_u", [53132]), 0);

// memory_copy.wast:2147
assert_return(() => call($13, "load8_u", [53331]), 0);

// memory_copy.wast:2148
assert_return(() => call($13, "load8_u", [53530]), 0);

// memory_copy.wast:2149
assert_return(() => call($13, "load8_u", [53729]), 0);

// memory_copy.wast:2150
assert_return(() => call($13, "load8_u", [53928]), 0);

// memory_copy.wast:2151
assert_return(() => call($13, "load8_u", [54127]), 0);

// memory_copy.wast:2152
assert_return(() => call($13, "load8_u", [54326]), 0);

// memory_copy.wast:2153
assert_return(() => call($13, "load8_u", [54525]), 0);

// memory_copy.wast:2154
assert_return(() => call($13, "load8_u", [54724]), 0);

// memory_copy.wast:2155
assert_return(() => call($13, "load8_u", [54923]), 0);

// memory_copy.wast:2156
assert_return(() => call($13, "load8_u", [55122]), 0);

// memory_copy.wast:2157
assert_return(() => call($13, "load8_u", [55321]), 0);

// memory_copy.wast:2158
assert_return(() => call($13, "load8_u", [55520]), 0);

// memory_copy.wast:2159
assert_return(() => call($13, "load8_u", [55719]), 0);

// memory_copy.wast:2160
assert_return(() => call($13, "load8_u", [55918]), 0);

// memory_copy.wast:2161
assert_return(() => call($13, "load8_u", [56117]), 0);

// memory_copy.wast:2162
assert_return(() => call($13, "load8_u", [56316]), 0);

// memory_copy.wast:2163
assert_return(() => call($13, "load8_u", [56515]), 0);

// memory_copy.wast:2164
assert_return(() => call($13, "load8_u", [56714]), 0);

// memory_copy.wast:2165
assert_return(() => call($13, "load8_u", [56913]), 0);

// memory_copy.wast:2166
assert_return(() => call($13, "load8_u", [57112]), 0);

// memory_copy.wast:2167
assert_return(() => call($13, "load8_u", [57311]), 0);

// memory_copy.wast:2168
assert_return(() => call($13, "load8_u", [57510]), 0);

// memory_copy.wast:2169
assert_return(() => call($13, "load8_u", [57709]), 0);

// memory_copy.wast:2170
assert_return(() => call($13, "load8_u", [57908]), 0);

// memory_copy.wast:2171
assert_return(() => call($13, "load8_u", [58107]), 0);

// memory_copy.wast:2172
assert_return(() => call($13, "load8_u", [58306]), 0);

// memory_copy.wast:2173
assert_return(() => call($13, "load8_u", [58505]), 0);

// memory_copy.wast:2174
assert_return(() => call($13, "load8_u", [58704]), 0);

// memory_copy.wast:2175
assert_return(() => call($13, "load8_u", [58903]), 0);

// memory_copy.wast:2176
assert_return(() => call($13, "load8_u", [59102]), 0);

// memory_copy.wast:2177
assert_return(() => call($13, "load8_u", [59301]), 0);

// memory_copy.wast:2178
assert_return(() => call($13, "load8_u", [59500]), 0);

// memory_copy.wast:2179
assert_return(() => call($13, "load8_u", [59699]), 0);

// memory_copy.wast:2180
assert_return(() => call($13, "load8_u", [59898]), 0);

// memory_copy.wast:2181
assert_return(() => call($13, "load8_u", [60097]), 0);

// memory_copy.wast:2182
assert_return(() => call($13, "load8_u", [60296]), 0);

// memory_copy.wast:2183
assert_return(() => call($13, "load8_u", [60495]), 0);

// memory_copy.wast:2184
assert_return(() => call($13, "load8_u", [60694]), 0);

// memory_copy.wast:2185
assert_return(() => call($13, "load8_u", [60893]), 0);

// memory_copy.wast:2186
assert_return(() => call($13, "load8_u", [61092]), 0);

// memory_copy.wast:2187
assert_return(() => call($13, "load8_u", [61291]), 0);

// memory_copy.wast:2188
assert_return(() => call($13, "load8_u", [61490]), 0);

// memory_copy.wast:2189
assert_return(() => call($13, "load8_u", [61689]), 0);

// memory_copy.wast:2190
assert_return(() => call($13, "load8_u", [61888]), 0);

// memory_copy.wast:2191
assert_return(() => call($13, "load8_u", [62087]), 0);

// memory_copy.wast:2192
assert_return(() => call($13, "load8_u", [62286]), 0);

// memory_copy.wast:2193
assert_return(() => call($13, "load8_u", [62485]), 0);

// memory_copy.wast:2194
assert_return(() => call($13, "load8_u", [62684]), 0);

// memory_copy.wast:2195
assert_return(() => call($13, "load8_u", [62883]), 0);

// memory_copy.wast:2196
assert_return(() => call($13, "load8_u", [63082]), 0);

// memory_copy.wast:2197
assert_return(() => call($13, "load8_u", [63281]), 0);

// memory_copy.wast:2198
assert_return(() => call($13, "load8_u", [63480]), 0);

// memory_copy.wast:2199
assert_return(() => call($13, "load8_u", [63679]), 0);

// memory_copy.wast:2200
assert_return(() => call($13, "load8_u", [63878]), 0);

// memory_copy.wast:2201
assert_return(() => call($13, "load8_u", [64077]), 0);

// memory_copy.wast:2202
assert_return(() => call($13, "load8_u", [64276]), 0);

// memory_copy.wast:2203
assert_return(() => call($13, "load8_u", [64475]), 0);

// memory_copy.wast:2204
assert_return(() => call($13, "load8_u", [64674]), 0);

// memory_copy.wast:2205
assert_return(() => call($13, "load8_u", [64873]), 0);

// memory_copy.wast:2206
assert_return(() => call($13, "load8_u", [65072]), 0);

// memory_copy.wast:2207
assert_return(() => call($13, "load8_u", [65271]), 0);

// memory_copy.wast:2208
assert_return(() => call($13, "load8_u", [65470]), 0);

// memory_copy.wast:2209
assert_return(() => call($13, "load8_u", [65486]), 0);

// memory_copy.wast:2210
assert_return(() => call($13, "load8_u", [65487]), 1);

// memory_copy.wast:2211
assert_return(() => call($13, "load8_u", [65488]), 2);

// memory_copy.wast:2212
assert_return(() => call($13, "load8_u", [65489]), 3);

// memory_copy.wast:2213
assert_return(() => call($13, "load8_u", [65490]), 4);

// memory_copy.wast:2214
assert_return(() => call($13, "load8_u", [65491]), 5);

// memory_copy.wast:2215
assert_return(() => call($13, "load8_u", [65492]), 6);

// memory_copy.wast:2216
assert_return(() => call($13, "load8_u", [65493]), 7);

// memory_copy.wast:2217
assert_return(() => call($13, "load8_u", [65494]), 8);

// memory_copy.wast:2218
assert_return(() => call($13, "load8_u", [65495]), 9);

// memory_copy.wast:2219
assert_return(() => call($13, "load8_u", [65496]), 10);

// memory_copy.wast:2220
assert_return(() => call($13, "load8_u", [65497]), 11);

// memory_copy.wast:2221
assert_return(() => call($13, "load8_u", [65498]), 12);

// memory_copy.wast:2222
assert_return(() => call($13, "load8_u", [65499]), 13);

// memory_copy.wast:2223
assert_return(() => call($13, "load8_u", [65500]), 14);

// memory_copy.wast:2224
assert_return(() => call($13, "load8_u", [65501]), 15);

// memory_copy.wast:2225
assert_return(() => call($13, "load8_u", [65502]), 16);

// memory_copy.wast:2226
assert_return(() => call($13, "load8_u", [65503]), 17);

// memory_copy.wast:2227
assert_return(() => call($13, "load8_u", [65504]), 18);

// memory_copy.wast:2228
assert_return(() => call($13, "load8_u", [65505]), 19);

// memory_copy.wast:2230
let $14 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xec\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:2238
assert_trap(() => call($14, "run", [65486, 65516, 40]));

// memory_copy.wast:2241
assert_return(() => call($14, "load8_u", [198]), 0);

// memory_copy.wast:2242
assert_return(() => call($14, "load8_u", [397]), 0);

// memory_copy.wast:2243
assert_return(() => call($14, "load8_u", [596]), 0);

// memory_copy.wast:2244
assert_return(() => call($14, "load8_u", [795]), 0);

// memory_copy.wast:2245
assert_return(() => call($14, "load8_u", [994]), 0);

// memory_copy.wast:2246
assert_return(() => call($14, "load8_u", [1193]), 0);

// memory_copy.wast:2247
assert_return(() => call($14, "load8_u", [1392]), 0);

// memory_copy.wast:2248
assert_return(() => call($14, "load8_u", [1591]), 0);

// memory_copy.wast:2249
assert_return(() => call($14, "load8_u", [1790]), 0);

// memory_copy.wast:2250
assert_return(() => call($14, "load8_u", [1989]), 0);

// memory_copy.wast:2251
assert_return(() => call($14, "load8_u", [2188]), 0);

// memory_copy.wast:2252
assert_return(() => call($14, "load8_u", [2387]), 0);

// memory_copy.wast:2253
assert_return(() => call($14, "load8_u", [2586]), 0);

// memory_copy.wast:2254
assert_return(() => call($14, "load8_u", [2785]), 0);

// memory_copy.wast:2255
assert_return(() => call($14, "load8_u", [2984]), 0);

// memory_copy.wast:2256
assert_return(() => call($14, "load8_u", [3183]), 0);

// memory_copy.wast:2257
assert_return(() => call($14, "load8_u", [3382]), 0);

// memory_copy.wast:2258
assert_return(() => call($14, "load8_u", [3581]), 0);

// memory_copy.wast:2259
assert_return(() => call($14, "load8_u", [3780]), 0);

// memory_copy.wast:2260
assert_return(() => call($14, "load8_u", [3979]), 0);

// memory_copy.wast:2261
assert_return(() => call($14, "load8_u", [4178]), 0);

// memory_copy.wast:2262
assert_return(() => call($14, "load8_u", [4377]), 0);

// memory_copy.wast:2263
assert_return(() => call($14, "load8_u", [4576]), 0);

// memory_copy.wast:2264
assert_return(() => call($14, "load8_u", [4775]), 0);

// memory_copy.wast:2265
assert_return(() => call($14, "load8_u", [4974]), 0);

// memory_copy.wast:2266
assert_return(() => call($14, "load8_u", [5173]), 0);

// memory_copy.wast:2267
assert_return(() => call($14, "load8_u", [5372]), 0);

// memory_copy.wast:2268
assert_return(() => call($14, "load8_u", [5571]), 0);

// memory_copy.wast:2269
assert_return(() => call($14, "load8_u", [5770]), 0);

// memory_copy.wast:2270
assert_return(() => call($14, "load8_u", [5969]), 0);

// memory_copy.wast:2271
assert_return(() => call($14, "load8_u", [6168]), 0);

// memory_copy.wast:2272
assert_return(() => call($14, "load8_u", [6367]), 0);

// memory_copy.wast:2273
assert_return(() => call($14, "load8_u", [6566]), 0);

// memory_copy.wast:2274
assert_return(() => call($14, "load8_u", [6765]), 0);

// memory_copy.wast:2275
assert_return(() => call($14, "load8_u", [6964]), 0);

// memory_copy.wast:2276
assert_return(() => call($14, "load8_u", [7163]), 0);

// memory_copy.wast:2277
assert_return(() => call($14, "load8_u", [7362]), 0);

// memory_copy.wast:2278
assert_return(() => call($14, "load8_u", [7561]), 0);

// memory_copy.wast:2279
assert_return(() => call($14, "load8_u", [7760]), 0);

// memory_copy.wast:2280
assert_return(() => call($14, "load8_u", [7959]), 0);

// memory_copy.wast:2281
assert_return(() => call($14, "load8_u", [8158]), 0);

// memory_copy.wast:2282
assert_return(() => call($14, "load8_u", [8357]), 0);

// memory_copy.wast:2283
assert_return(() => call($14, "load8_u", [8556]), 0);

// memory_copy.wast:2284
assert_return(() => call($14, "load8_u", [8755]), 0);

// memory_copy.wast:2285
assert_return(() => call($14, "load8_u", [8954]), 0);

// memory_copy.wast:2286
assert_return(() => call($14, "load8_u", [9153]), 0);

// memory_copy.wast:2287
assert_return(() => call($14, "load8_u", [9352]), 0);

// memory_copy.wast:2288
assert_return(() => call($14, "load8_u", [9551]), 0);

// memory_copy.wast:2289
assert_return(() => call($14, "load8_u", [9750]), 0);

// memory_copy.wast:2290
assert_return(() => call($14, "load8_u", [9949]), 0);

// memory_copy.wast:2291
assert_return(() => call($14, "load8_u", [10148]), 0);

// memory_copy.wast:2292
assert_return(() => call($14, "load8_u", [10347]), 0);

// memory_copy.wast:2293
assert_return(() => call($14, "load8_u", [10546]), 0);

// memory_copy.wast:2294
assert_return(() => call($14, "load8_u", [10745]), 0);

// memory_copy.wast:2295
assert_return(() => call($14, "load8_u", [10944]), 0);

// memory_copy.wast:2296
assert_return(() => call($14, "load8_u", [11143]), 0);

// memory_copy.wast:2297
assert_return(() => call($14, "load8_u", [11342]), 0);

// memory_copy.wast:2298
assert_return(() => call($14, "load8_u", [11541]), 0);

// memory_copy.wast:2299
assert_return(() => call($14, "load8_u", [11740]), 0);

// memory_copy.wast:2300
assert_return(() => call($14, "load8_u", [11939]), 0);

// memory_copy.wast:2301
assert_return(() => call($14, "load8_u", [12138]), 0);

// memory_copy.wast:2302
assert_return(() => call($14, "load8_u", [12337]), 0);

// memory_copy.wast:2303
assert_return(() => call($14, "load8_u", [12536]), 0);

// memory_copy.wast:2304
assert_return(() => call($14, "load8_u", [12735]), 0);

// memory_copy.wast:2305
assert_return(() => call($14, "load8_u", [12934]), 0);

// memory_copy.wast:2306
assert_return(() => call($14, "load8_u", [13133]), 0);

// memory_copy.wast:2307
assert_return(() => call($14, "load8_u", [13332]), 0);

// memory_copy.wast:2308
assert_return(() => call($14, "load8_u", [13531]), 0);

// memory_copy.wast:2309
assert_return(() => call($14, "load8_u", [13730]), 0);

// memory_copy.wast:2310
assert_return(() => call($14, "load8_u", [13929]), 0);

// memory_copy.wast:2311
assert_return(() => call($14, "load8_u", [14128]), 0);

// memory_copy.wast:2312
assert_return(() => call($14, "load8_u", [14327]), 0);

// memory_copy.wast:2313
assert_return(() => call($14, "load8_u", [14526]), 0);

// memory_copy.wast:2314
assert_return(() => call($14, "load8_u", [14725]), 0);

// memory_copy.wast:2315
assert_return(() => call($14, "load8_u", [14924]), 0);

// memory_copy.wast:2316
assert_return(() => call($14, "load8_u", [15123]), 0);

// memory_copy.wast:2317
assert_return(() => call($14, "load8_u", [15322]), 0);

// memory_copy.wast:2318
assert_return(() => call($14, "load8_u", [15521]), 0);

// memory_copy.wast:2319
assert_return(() => call($14, "load8_u", [15720]), 0);

// memory_copy.wast:2320
assert_return(() => call($14, "load8_u", [15919]), 0);

// memory_copy.wast:2321
assert_return(() => call($14, "load8_u", [16118]), 0);

// memory_copy.wast:2322
assert_return(() => call($14, "load8_u", [16317]), 0);

// memory_copy.wast:2323
assert_return(() => call($14, "load8_u", [16516]), 0);

// memory_copy.wast:2324
assert_return(() => call($14, "load8_u", [16715]), 0);

// memory_copy.wast:2325
assert_return(() => call($14, "load8_u", [16914]), 0);

// memory_copy.wast:2326
assert_return(() => call($14, "load8_u", [17113]), 0);

// memory_copy.wast:2327
assert_return(() => call($14, "load8_u", [17312]), 0);

// memory_copy.wast:2328
assert_return(() => call($14, "load8_u", [17511]), 0);

// memory_copy.wast:2329
assert_return(() => call($14, "load8_u", [17710]), 0);

// memory_copy.wast:2330
assert_return(() => call($14, "load8_u", [17909]), 0);

// memory_copy.wast:2331
assert_return(() => call($14, "load8_u", [18108]), 0);

// memory_copy.wast:2332
assert_return(() => call($14, "load8_u", [18307]), 0);

// memory_copy.wast:2333
assert_return(() => call($14, "load8_u", [18506]), 0);

// memory_copy.wast:2334
assert_return(() => call($14, "load8_u", [18705]), 0);

// memory_copy.wast:2335
assert_return(() => call($14, "load8_u", [18904]), 0);

// memory_copy.wast:2336
assert_return(() => call($14, "load8_u", [19103]), 0);

// memory_copy.wast:2337
assert_return(() => call($14, "load8_u", [19302]), 0);

// memory_copy.wast:2338
assert_return(() => call($14, "load8_u", [19501]), 0);

// memory_copy.wast:2339
assert_return(() => call($14, "load8_u", [19700]), 0);

// memory_copy.wast:2340
assert_return(() => call($14, "load8_u", [19899]), 0);

// memory_copy.wast:2341
assert_return(() => call($14, "load8_u", [20098]), 0);

// memory_copy.wast:2342
assert_return(() => call($14, "load8_u", [20297]), 0);

// memory_copy.wast:2343
assert_return(() => call($14, "load8_u", [20496]), 0);

// memory_copy.wast:2344
assert_return(() => call($14, "load8_u", [20695]), 0);

// memory_copy.wast:2345
assert_return(() => call($14, "load8_u", [20894]), 0);

// memory_copy.wast:2346
assert_return(() => call($14, "load8_u", [21093]), 0);

// memory_copy.wast:2347
assert_return(() => call($14, "load8_u", [21292]), 0);

// memory_copy.wast:2348
assert_return(() => call($14, "load8_u", [21491]), 0);

// memory_copy.wast:2349
assert_return(() => call($14, "load8_u", [21690]), 0);

// memory_copy.wast:2350
assert_return(() => call($14, "load8_u", [21889]), 0);

// memory_copy.wast:2351
assert_return(() => call($14, "load8_u", [22088]), 0);

// memory_copy.wast:2352
assert_return(() => call($14, "load8_u", [22287]), 0);

// memory_copy.wast:2353
assert_return(() => call($14, "load8_u", [22486]), 0);

// memory_copy.wast:2354
assert_return(() => call($14, "load8_u", [22685]), 0);

// memory_copy.wast:2355
assert_return(() => call($14, "load8_u", [22884]), 0);

// memory_copy.wast:2356
assert_return(() => call($14, "load8_u", [23083]), 0);

// memory_copy.wast:2357
assert_return(() => call($14, "load8_u", [23282]), 0);

// memory_copy.wast:2358
assert_return(() => call($14, "load8_u", [23481]), 0);

// memory_copy.wast:2359
assert_return(() => call($14, "load8_u", [23680]), 0);

// memory_copy.wast:2360
assert_return(() => call($14, "load8_u", [23879]), 0);

// memory_copy.wast:2361
assert_return(() => call($14, "load8_u", [24078]), 0);

// memory_copy.wast:2362
assert_return(() => call($14, "load8_u", [24277]), 0);

// memory_copy.wast:2363
assert_return(() => call($14, "load8_u", [24476]), 0);

// memory_copy.wast:2364
assert_return(() => call($14, "load8_u", [24675]), 0);

// memory_copy.wast:2365
assert_return(() => call($14, "load8_u", [24874]), 0);

// memory_copy.wast:2366
assert_return(() => call($14, "load8_u", [25073]), 0);

// memory_copy.wast:2367
assert_return(() => call($14, "load8_u", [25272]), 0);

// memory_copy.wast:2368
assert_return(() => call($14, "load8_u", [25471]), 0);

// memory_copy.wast:2369
assert_return(() => call($14, "load8_u", [25670]), 0);

// memory_copy.wast:2370
assert_return(() => call($14, "load8_u", [25869]), 0);

// memory_copy.wast:2371
assert_return(() => call($14, "load8_u", [26068]), 0);

// memory_copy.wast:2372
assert_return(() => call($14, "load8_u", [26267]), 0);

// memory_copy.wast:2373
assert_return(() => call($14, "load8_u", [26466]), 0);

// memory_copy.wast:2374
assert_return(() => call($14, "load8_u", [26665]), 0);

// memory_copy.wast:2375
assert_return(() => call($14, "load8_u", [26864]), 0);

// memory_copy.wast:2376
assert_return(() => call($14, "load8_u", [27063]), 0);

// memory_copy.wast:2377
assert_return(() => call($14, "load8_u", [27262]), 0);

// memory_copy.wast:2378
assert_return(() => call($14, "load8_u", [27461]), 0);

// memory_copy.wast:2379
assert_return(() => call($14, "load8_u", [27660]), 0);

// memory_copy.wast:2380
assert_return(() => call($14, "load8_u", [27859]), 0);

// memory_copy.wast:2381
assert_return(() => call($14, "load8_u", [28058]), 0);

// memory_copy.wast:2382
assert_return(() => call($14, "load8_u", [28257]), 0);

// memory_copy.wast:2383
assert_return(() => call($14, "load8_u", [28456]), 0);

// memory_copy.wast:2384
assert_return(() => call($14, "load8_u", [28655]), 0);

// memory_copy.wast:2385
assert_return(() => call($14, "load8_u", [28854]), 0);

// memory_copy.wast:2386
assert_return(() => call($14, "load8_u", [29053]), 0);

// memory_copy.wast:2387
assert_return(() => call($14, "load8_u", [29252]), 0);

// memory_copy.wast:2388
assert_return(() => call($14, "load8_u", [29451]), 0);

// memory_copy.wast:2389
assert_return(() => call($14, "load8_u", [29650]), 0);

// memory_copy.wast:2390
assert_return(() => call($14, "load8_u", [29849]), 0);

// memory_copy.wast:2391
assert_return(() => call($14, "load8_u", [30048]), 0);

// memory_copy.wast:2392
assert_return(() => call($14, "load8_u", [30247]), 0);

// memory_copy.wast:2393
assert_return(() => call($14, "load8_u", [30446]), 0);

// memory_copy.wast:2394
assert_return(() => call($14, "load8_u", [30645]), 0);

// memory_copy.wast:2395
assert_return(() => call($14, "load8_u", [30844]), 0);

// memory_copy.wast:2396
assert_return(() => call($14, "load8_u", [31043]), 0);

// memory_copy.wast:2397
assert_return(() => call($14, "load8_u", [31242]), 0);

// memory_copy.wast:2398
assert_return(() => call($14, "load8_u", [31441]), 0);

// memory_copy.wast:2399
assert_return(() => call($14, "load8_u", [31640]), 0);

// memory_copy.wast:2400
assert_return(() => call($14, "load8_u", [31839]), 0);

// memory_copy.wast:2401
assert_return(() => call($14, "load8_u", [32038]), 0);

// memory_copy.wast:2402
assert_return(() => call($14, "load8_u", [32237]), 0);

// memory_copy.wast:2403
assert_return(() => call($14, "load8_u", [32436]), 0);

// memory_copy.wast:2404
assert_return(() => call($14, "load8_u", [32635]), 0);

// memory_copy.wast:2405
assert_return(() => call($14, "load8_u", [32834]), 0);

// memory_copy.wast:2406
assert_return(() => call($14, "load8_u", [33033]), 0);

// memory_copy.wast:2407
assert_return(() => call($14, "load8_u", [33232]), 0);

// memory_copy.wast:2408
assert_return(() => call($14, "load8_u", [33431]), 0);

// memory_copy.wast:2409
assert_return(() => call($14, "load8_u", [33630]), 0);

// memory_copy.wast:2410
assert_return(() => call($14, "load8_u", [33829]), 0);

// memory_copy.wast:2411
assert_return(() => call($14, "load8_u", [34028]), 0);

// memory_copy.wast:2412
assert_return(() => call($14, "load8_u", [34227]), 0);

// memory_copy.wast:2413
assert_return(() => call($14, "load8_u", [34426]), 0);

// memory_copy.wast:2414
assert_return(() => call($14, "load8_u", [34625]), 0);

// memory_copy.wast:2415
assert_return(() => call($14, "load8_u", [34824]), 0);

// memory_copy.wast:2416
assert_return(() => call($14, "load8_u", [35023]), 0);

// memory_copy.wast:2417
assert_return(() => call($14, "load8_u", [35222]), 0);

// memory_copy.wast:2418
assert_return(() => call($14, "load8_u", [35421]), 0);

// memory_copy.wast:2419
assert_return(() => call($14, "load8_u", [35620]), 0);

// memory_copy.wast:2420
assert_return(() => call($14, "load8_u", [35819]), 0);

// memory_copy.wast:2421
assert_return(() => call($14, "load8_u", [36018]), 0);

// memory_copy.wast:2422
assert_return(() => call($14, "load8_u", [36217]), 0);

// memory_copy.wast:2423
assert_return(() => call($14, "load8_u", [36416]), 0);

// memory_copy.wast:2424
assert_return(() => call($14, "load8_u", [36615]), 0);

// memory_copy.wast:2425
assert_return(() => call($14, "load8_u", [36814]), 0);

// memory_copy.wast:2426
assert_return(() => call($14, "load8_u", [37013]), 0);

// memory_copy.wast:2427
assert_return(() => call($14, "load8_u", [37212]), 0);

// memory_copy.wast:2428
assert_return(() => call($14, "load8_u", [37411]), 0);

// memory_copy.wast:2429
assert_return(() => call($14, "load8_u", [37610]), 0);

// memory_copy.wast:2430
assert_return(() => call($14, "load8_u", [37809]), 0);

// memory_copy.wast:2431
assert_return(() => call($14, "load8_u", [38008]), 0);

// memory_copy.wast:2432
assert_return(() => call($14, "load8_u", [38207]), 0);

// memory_copy.wast:2433
assert_return(() => call($14, "load8_u", [38406]), 0);

// memory_copy.wast:2434
assert_return(() => call($14, "load8_u", [38605]), 0);

// memory_copy.wast:2435
assert_return(() => call($14, "load8_u", [38804]), 0);

// memory_copy.wast:2436
assert_return(() => call($14, "load8_u", [39003]), 0);

// memory_copy.wast:2437
assert_return(() => call($14, "load8_u", [39202]), 0);

// memory_copy.wast:2438
assert_return(() => call($14, "load8_u", [39401]), 0);

// memory_copy.wast:2439
assert_return(() => call($14, "load8_u", [39600]), 0);

// memory_copy.wast:2440
assert_return(() => call($14, "load8_u", [39799]), 0);

// memory_copy.wast:2441
assert_return(() => call($14, "load8_u", [39998]), 0);

// memory_copy.wast:2442
assert_return(() => call($14, "load8_u", [40197]), 0);

// memory_copy.wast:2443
assert_return(() => call($14, "load8_u", [40396]), 0);

// memory_copy.wast:2444
assert_return(() => call($14, "load8_u", [40595]), 0);

// memory_copy.wast:2445
assert_return(() => call($14, "load8_u", [40794]), 0);

// memory_copy.wast:2446
assert_return(() => call($14, "load8_u", [40993]), 0);

// memory_copy.wast:2447
assert_return(() => call($14, "load8_u", [41192]), 0);

// memory_copy.wast:2448
assert_return(() => call($14, "load8_u", [41391]), 0);

// memory_copy.wast:2449
assert_return(() => call($14, "load8_u", [41590]), 0);

// memory_copy.wast:2450
assert_return(() => call($14, "load8_u", [41789]), 0);

// memory_copy.wast:2451
assert_return(() => call($14, "load8_u", [41988]), 0);

// memory_copy.wast:2452
assert_return(() => call($14, "load8_u", [42187]), 0);

// memory_copy.wast:2453
assert_return(() => call($14, "load8_u", [42386]), 0);

// memory_copy.wast:2454
assert_return(() => call($14, "load8_u", [42585]), 0);

// memory_copy.wast:2455
assert_return(() => call($14, "load8_u", [42784]), 0);

// memory_copy.wast:2456
assert_return(() => call($14, "load8_u", [42983]), 0);

// memory_copy.wast:2457
assert_return(() => call($14, "load8_u", [43182]), 0);

// memory_copy.wast:2458
assert_return(() => call($14, "load8_u", [43381]), 0);

// memory_copy.wast:2459
assert_return(() => call($14, "load8_u", [43580]), 0);

// memory_copy.wast:2460
assert_return(() => call($14, "load8_u", [43779]), 0);

// memory_copy.wast:2461
assert_return(() => call($14, "load8_u", [43978]), 0);

// memory_copy.wast:2462
assert_return(() => call($14, "load8_u", [44177]), 0);

// memory_copy.wast:2463
assert_return(() => call($14, "load8_u", [44376]), 0);

// memory_copy.wast:2464
assert_return(() => call($14, "load8_u", [44575]), 0);

// memory_copy.wast:2465
assert_return(() => call($14, "load8_u", [44774]), 0);

// memory_copy.wast:2466
assert_return(() => call($14, "load8_u", [44973]), 0);

// memory_copy.wast:2467
assert_return(() => call($14, "load8_u", [45172]), 0);

// memory_copy.wast:2468
assert_return(() => call($14, "load8_u", [45371]), 0);

// memory_copy.wast:2469
assert_return(() => call($14, "load8_u", [45570]), 0);

// memory_copy.wast:2470
assert_return(() => call($14, "load8_u", [45769]), 0);

// memory_copy.wast:2471
assert_return(() => call($14, "load8_u", [45968]), 0);

// memory_copy.wast:2472
assert_return(() => call($14, "load8_u", [46167]), 0);

// memory_copy.wast:2473
assert_return(() => call($14, "load8_u", [46366]), 0);

// memory_copy.wast:2474
assert_return(() => call($14, "load8_u", [46565]), 0);

// memory_copy.wast:2475
assert_return(() => call($14, "load8_u", [46764]), 0);

// memory_copy.wast:2476
assert_return(() => call($14, "load8_u", [46963]), 0);

// memory_copy.wast:2477
assert_return(() => call($14, "load8_u", [47162]), 0);

// memory_copy.wast:2478
assert_return(() => call($14, "load8_u", [47361]), 0);

// memory_copy.wast:2479
assert_return(() => call($14, "load8_u", [47560]), 0);

// memory_copy.wast:2480
assert_return(() => call($14, "load8_u", [47759]), 0);

// memory_copy.wast:2481
assert_return(() => call($14, "load8_u", [47958]), 0);

// memory_copy.wast:2482
assert_return(() => call($14, "load8_u", [48157]), 0);

// memory_copy.wast:2483
assert_return(() => call($14, "load8_u", [48356]), 0);

// memory_copy.wast:2484
assert_return(() => call($14, "load8_u", [48555]), 0);

// memory_copy.wast:2485
assert_return(() => call($14, "load8_u", [48754]), 0);

// memory_copy.wast:2486
assert_return(() => call($14, "load8_u", [48953]), 0);

// memory_copy.wast:2487
assert_return(() => call($14, "load8_u", [49152]), 0);

// memory_copy.wast:2488
assert_return(() => call($14, "load8_u", [49351]), 0);

// memory_copy.wast:2489
assert_return(() => call($14, "load8_u", [49550]), 0);

// memory_copy.wast:2490
assert_return(() => call($14, "load8_u", [49749]), 0);

// memory_copy.wast:2491
assert_return(() => call($14, "load8_u", [49948]), 0);

// memory_copy.wast:2492
assert_return(() => call($14, "load8_u", [50147]), 0);

// memory_copy.wast:2493
assert_return(() => call($14, "load8_u", [50346]), 0);

// memory_copy.wast:2494
assert_return(() => call($14, "load8_u", [50545]), 0);

// memory_copy.wast:2495
assert_return(() => call($14, "load8_u", [50744]), 0);

// memory_copy.wast:2496
assert_return(() => call($14, "load8_u", [50943]), 0);

// memory_copy.wast:2497
assert_return(() => call($14, "load8_u", [51142]), 0);

// memory_copy.wast:2498
assert_return(() => call($14, "load8_u", [51341]), 0);

// memory_copy.wast:2499
assert_return(() => call($14, "load8_u", [51540]), 0);

// memory_copy.wast:2500
assert_return(() => call($14, "load8_u", [51739]), 0);

// memory_copy.wast:2501
assert_return(() => call($14, "load8_u", [51938]), 0);

// memory_copy.wast:2502
assert_return(() => call($14, "load8_u", [52137]), 0);

// memory_copy.wast:2503
assert_return(() => call($14, "load8_u", [52336]), 0);

// memory_copy.wast:2504
assert_return(() => call($14, "load8_u", [52535]), 0);

// memory_copy.wast:2505
assert_return(() => call($14, "load8_u", [52734]), 0);

// memory_copy.wast:2506
assert_return(() => call($14, "load8_u", [52933]), 0);

// memory_copy.wast:2507
assert_return(() => call($14, "load8_u", [53132]), 0);

// memory_copy.wast:2508
assert_return(() => call($14, "load8_u", [53331]), 0);

// memory_copy.wast:2509
assert_return(() => call($14, "load8_u", [53530]), 0);

// memory_copy.wast:2510
assert_return(() => call($14, "load8_u", [53729]), 0);

// memory_copy.wast:2511
assert_return(() => call($14, "load8_u", [53928]), 0);

// memory_copy.wast:2512
assert_return(() => call($14, "load8_u", [54127]), 0);

// memory_copy.wast:2513
assert_return(() => call($14, "load8_u", [54326]), 0);

// memory_copy.wast:2514
assert_return(() => call($14, "load8_u", [54525]), 0);

// memory_copy.wast:2515
assert_return(() => call($14, "load8_u", [54724]), 0);

// memory_copy.wast:2516
assert_return(() => call($14, "load8_u", [54923]), 0);

// memory_copy.wast:2517
assert_return(() => call($14, "load8_u", [55122]), 0);

// memory_copy.wast:2518
assert_return(() => call($14, "load8_u", [55321]), 0);

// memory_copy.wast:2519
assert_return(() => call($14, "load8_u", [55520]), 0);

// memory_copy.wast:2520
assert_return(() => call($14, "load8_u", [55719]), 0);

// memory_copy.wast:2521
assert_return(() => call($14, "load8_u", [55918]), 0);

// memory_copy.wast:2522
assert_return(() => call($14, "load8_u", [56117]), 0);

// memory_copy.wast:2523
assert_return(() => call($14, "load8_u", [56316]), 0);

// memory_copy.wast:2524
assert_return(() => call($14, "load8_u", [56515]), 0);

// memory_copy.wast:2525
assert_return(() => call($14, "load8_u", [56714]), 0);

// memory_copy.wast:2526
assert_return(() => call($14, "load8_u", [56913]), 0);

// memory_copy.wast:2527
assert_return(() => call($14, "load8_u", [57112]), 0);

// memory_copy.wast:2528
assert_return(() => call($14, "load8_u", [57311]), 0);

// memory_copy.wast:2529
assert_return(() => call($14, "load8_u", [57510]), 0);

// memory_copy.wast:2530
assert_return(() => call($14, "load8_u", [57709]), 0);

// memory_copy.wast:2531
assert_return(() => call($14, "load8_u", [57908]), 0);

// memory_copy.wast:2532
assert_return(() => call($14, "load8_u", [58107]), 0);

// memory_copy.wast:2533
assert_return(() => call($14, "load8_u", [58306]), 0);

// memory_copy.wast:2534
assert_return(() => call($14, "load8_u", [58505]), 0);

// memory_copy.wast:2535
assert_return(() => call($14, "load8_u", [58704]), 0);

// memory_copy.wast:2536
assert_return(() => call($14, "load8_u", [58903]), 0);

// memory_copy.wast:2537
assert_return(() => call($14, "load8_u", [59102]), 0);

// memory_copy.wast:2538
assert_return(() => call($14, "load8_u", [59301]), 0);

// memory_copy.wast:2539
assert_return(() => call($14, "load8_u", [59500]), 0);

// memory_copy.wast:2540
assert_return(() => call($14, "load8_u", [59699]), 0);

// memory_copy.wast:2541
assert_return(() => call($14, "load8_u", [59898]), 0);

// memory_copy.wast:2542
assert_return(() => call($14, "load8_u", [60097]), 0);

// memory_copy.wast:2543
assert_return(() => call($14, "load8_u", [60296]), 0);

// memory_copy.wast:2544
assert_return(() => call($14, "load8_u", [60495]), 0);

// memory_copy.wast:2545
assert_return(() => call($14, "load8_u", [60694]), 0);

// memory_copy.wast:2546
assert_return(() => call($14, "load8_u", [60893]), 0);

// memory_copy.wast:2547
assert_return(() => call($14, "load8_u", [61092]), 0);

// memory_copy.wast:2548
assert_return(() => call($14, "load8_u", [61291]), 0);

// memory_copy.wast:2549
assert_return(() => call($14, "load8_u", [61490]), 0);

// memory_copy.wast:2550
assert_return(() => call($14, "load8_u", [61689]), 0);

// memory_copy.wast:2551
assert_return(() => call($14, "load8_u", [61888]), 0);

// memory_copy.wast:2552
assert_return(() => call($14, "load8_u", [62087]), 0);

// memory_copy.wast:2553
assert_return(() => call($14, "load8_u", [62286]), 0);

// memory_copy.wast:2554
assert_return(() => call($14, "load8_u", [62485]), 0);

// memory_copy.wast:2555
assert_return(() => call($14, "load8_u", [62684]), 0);

// memory_copy.wast:2556
assert_return(() => call($14, "load8_u", [62883]), 0);

// memory_copy.wast:2557
assert_return(() => call($14, "load8_u", [63082]), 0);

// memory_copy.wast:2558
assert_return(() => call($14, "load8_u", [63281]), 0);

// memory_copy.wast:2559
assert_return(() => call($14, "load8_u", [63480]), 0);

// memory_copy.wast:2560
assert_return(() => call($14, "load8_u", [63679]), 0);

// memory_copy.wast:2561
assert_return(() => call($14, "load8_u", [63878]), 0);

// memory_copy.wast:2562
assert_return(() => call($14, "load8_u", [64077]), 0);

// memory_copy.wast:2563
assert_return(() => call($14, "load8_u", [64276]), 0);

// memory_copy.wast:2564
assert_return(() => call($14, "load8_u", [64475]), 0);

// memory_copy.wast:2565
assert_return(() => call($14, "load8_u", [64674]), 0);

// memory_copy.wast:2566
assert_return(() => call($14, "load8_u", [64873]), 0);

// memory_copy.wast:2567
assert_return(() => call($14, "load8_u", [65072]), 0);

// memory_copy.wast:2568
assert_return(() => call($14, "load8_u", [65271]), 0);

// memory_copy.wast:2569
assert_return(() => call($14, "load8_u", [65470]), 0);

// memory_copy.wast:2570
assert_return(() => call($14, "load8_u", [65486]), 0);

// memory_copy.wast:2571
assert_return(() => call($14, "load8_u", [65487]), 1);

// memory_copy.wast:2572
assert_return(() => call($14, "load8_u", [65488]), 2);

// memory_copy.wast:2573
assert_return(() => call($14, "load8_u", [65489]), 3);

// memory_copy.wast:2574
assert_return(() => call($14, "load8_u", [65490]), 4);

// memory_copy.wast:2575
assert_return(() => call($14, "load8_u", [65491]), 5);

// memory_copy.wast:2576
assert_return(() => call($14, "load8_u", [65492]), 6);

// memory_copy.wast:2577
assert_return(() => call($14, "load8_u", [65493]), 7);

// memory_copy.wast:2578
assert_return(() => call($14, "load8_u", [65494]), 8);

// memory_copy.wast:2579
assert_return(() => call($14, "load8_u", [65495]), 9);

// memory_copy.wast:2580
assert_return(() => call($14, "load8_u", [65496]), 10);

// memory_copy.wast:2581
assert_return(() => call($14, "load8_u", [65497]), 11);

// memory_copy.wast:2582
assert_return(() => call($14, "load8_u", [65498]), 12);

// memory_copy.wast:2583
assert_return(() => call($14, "load8_u", [65499]), 13);

// memory_copy.wast:2584
assert_return(() => call($14, "load8_u", [65500]), 14);

// memory_copy.wast:2585
assert_return(() => call($14, "load8_u", [65501]), 15);

// memory_copy.wast:2586
assert_return(() => call($14, "load8_u", [65502]), 16);

// memory_copy.wast:2587
assert_return(() => call($14, "load8_u", [65503]), 17);

// memory_copy.wast:2588
assert_return(() => call($14, "load8_u", [65504]), 18);

// memory_copy.wast:2589
assert_return(() => call($14, "load8_u", [65505]), 19);

// memory_copy.wast:2590
assert_return(() => call($14, "load8_u", [65516]), 0);

// memory_copy.wast:2591
assert_return(() => call($14, "load8_u", [65517]), 1);

// memory_copy.wast:2592
assert_return(() => call($14, "load8_u", [65518]), 2);

// memory_copy.wast:2593
assert_return(() => call($14, "load8_u", [65519]), 3);

// memory_copy.wast:2594
assert_return(() => call($14, "load8_u", [65520]), 4);

// memory_copy.wast:2595
assert_return(() => call($14, "load8_u", [65521]), 5);

// memory_copy.wast:2596
assert_return(() => call($14, "load8_u", [65522]), 6);

// memory_copy.wast:2597
assert_return(() => call($14, "load8_u", [65523]), 7);

// memory_copy.wast:2598
assert_return(() => call($14, "load8_u", [65524]), 8);

// memory_copy.wast:2599
assert_return(() => call($14, "load8_u", [65525]), 9);

// memory_copy.wast:2600
assert_return(() => call($14, "load8_u", [65526]), 10);

// memory_copy.wast:2601
assert_return(() => call($14, "load8_u", [65527]), 11);

// memory_copy.wast:2602
assert_return(() => call($14, "load8_u", [65528]), 12);

// memory_copy.wast:2603
assert_return(() => call($14, "load8_u", [65529]), 13);

// memory_copy.wast:2604
assert_return(() => call($14, "load8_u", [65530]), 14);

// memory_copy.wast:2605
assert_return(() => call($14, "load8_u", [65531]), 15);

// memory_copy.wast:2606
assert_return(() => call($14, "load8_u", [65532]), 16);

// memory_copy.wast:2607
assert_return(() => call($14, "load8_u", [65533]), 17);

// memory_copy.wast:2608
assert_return(() => call($14, "load8_u", [65534]), 18);

// memory_copy.wast:2609
assert_return(() => call($14, "load8_u", [65535]), 19);

// memory_copy.wast:2611
let $15 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xe2\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:2619
assert_trap(() => call($15, "run", [65516, 65506, 40]));

// memory_copy.wast:2622
assert_return(() => call($15, "load8_u", [198]), 0);

// memory_copy.wast:2623
assert_return(() => call($15, "load8_u", [397]), 0);

// memory_copy.wast:2624
assert_return(() => call($15, "load8_u", [596]), 0);

// memory_copy.wast:2625
assert_return(() => call($15, "load8_u", [795]), 0);

// memory_copy.wast:2626
assert_return(() => call($15, "load8_u", [994]), 0);

// memory_copy.wast:2627
assert_return(() => call($15, "load8_u", [1193]), 0);

// memory_copy.wast:2628
assert_return(() => call($15, "load8_u", [1392]), 0);

// memory_copy.wast:2629
assert_return(() => call($15, "load8_u", [1591]), 0);

// memory_copy.wast:2630
assert_return(() => call($15, "load8_u", [1790]), 0);

// memory_copy.wast:2631
assert_return(() => call($15, "load8_u", [1989]), 0);

// memory_copy.wast:2632
assert_return(() => call($15, "load8_u", [2188]), 0);

// memory_copy.wast:2633
assert_return(() => call($15, "load8_u", [2387]), 0);

// memory_copy.wast:2634
assert_return(() => call($15, "load8_u", [2586]), 0);

// memory_copy.wast:2635
assert_return(() => call($15, "load8_u", [2785]), 0);

// memory_copy.wast:2636
assert_return(() => call($15, "load8_u", [2984]), 0);

// memory_copy.wast:2637
assert_return(() => call($15, "load8_u", [3183]), 0);

// memory_copy.wast:2638
assert_return(() => call($15, "load8_u", [3382]), 0);

// memory_copy.wast:2639
assert_return(() => call($15, "load8_u", [3581]), 0);

// memory_copy.wast:2640
assert_return(() => call($15, "load8_u", [3780]), 0);

// memory_copy.wast:2641
assert_return(() => call($15, "load8_u", [3979]), 0);

// memory_copy.wast:2642
assert_return(() => call($15, "load8_u", [4178]), 0);

// memory_copy.wast:2643
assert_return(() => call($15, "load8_u", [4377]), 0);

// memory_copy.wast:2644
assert_return(() => call($15, "load8_u", [4576]), 0);

// memory_copy.wast:2645
assert_return(() => call($15, "load8_u", [4775]), 0);

// memory_copy.wast:2646
assert_return(() => call($15, "load8_u", [4974]), 0);

// memory_copy.wast:2647
assert_return(() => call($15, "load8_u", [5173]), 0);

// memory_copy.wast:2648
assert_return(() => call($15, "load8_u", [5372]), 0);

// memory_copy.wast:2649
assert_return(() => call($15, "load8_u", [5571]), 0);

// memory_copy.wast:2650
assert_return(() => call($15, "load8_u", [5770]), 0);

// memory_copy.wast:2651
assert_return(() => call($15, "load8_u", [5969]), 0);

// memory_copy.wast:2652
assert_return(() => call($15, "load8_u", [6168]), 0);

// memory_copy.wast:2653
assert_return(() => call($15, "load8_u", [6367]), 0);

// memory_copy.wast:2654
assert_return(() => call($15, "load8_u", [6566]), 0);

// memory_copy.wast:2655
assert_return(() => call($15, "load8_u", [6765]), 0);

// memory_copy.wast:2656
assert_return(() => call($15, "load8_u", [6964]), 0);

// memory_copy.wast:2657
assert_return(() => call($15, "load8_u", [7163]), 0);

// memory_copy.wast:2658
assert_return(() => call($15, "load8_u", [7362]), 0);

// memory_copy.wast:2659
assert_return(() => call($15, "load8_u", [7561]), 0);

// memory_copy.wast:2660
assert_return(() => call($15, "load8_u", [7760]), 0);

// memory_copy.wast:2661
assert_return(() => call($15, "load8_u", [7959]), 0);

// memory_copy.wast:2662
assert_return(() => call($15, "load8_u", [8158]), 0);

// memory_copy.wast:2663
assert_return(() => call($15, "load8_u", [8357]), 0);

// memory_copy.wast:2664
assert_return(() => call($15, "load8_u", [8556]), 0);

// memory_copy.wast:2665
assert_return(() => call($15, "load8_u", [8755]), 0);

// memory_copy.wast:2666
assert_return(() => call($15, "load8_u", [8954]), 0);

// memory_copy.wast:2667
assert_return(() => call($15, "load8_u", [9153]), 0);

// memory_copy.wast:2668
assert_return(() => call($15, "load8_u", [9352]), 0);

// memory_copy.wast:2669
assert_return(() => call($15, "load8_u", [9551]), 0);

// memory_copy.wast:2670
assert_return(() => call($15, "load8_u", [9750]), 0);

// memory_copy.wast:2671
assert_return(() => call($15, "load8_u", [9949]), 0);

// memory_copy.wast:2672
assert_return(() => call($15, "load8_u", [10148]), 0);

// memory_copy.wast:2673
assert_return(() => call($15, "load8_u", [10347]), 0);

// memory_copy.wast:2674
assert_return(() => call($15, "load8_u", [10546]), 0);

// memory_copy.wast:2675
assert_return(() => call($15, "load8_u", [10745]), 0);

// memory_copy.wast:2676
assert_return(() => call($15, "load8_u", [10944]), 0);

// memory_copy.wast:2677
assert_return(() => call($15, "load8_u", [11143]), 0);

// memory_copy.wast:2678
assert_return(() => call($15, "load8_u", [11342]), 0);

// memory_copy.wast:2679
assert_return(() => call($15, "load8_u", [11541]), 0);

// memory_copy.wast:2680
assert_return(() => call($15, "load8_u", [11740]), 0);

// memory_copy.wast:2681
assert_return(() => call($15, "load8_u", [11939]), 0);

// memory_copy.wast:2682
assert_return(() => call($15, "load8_u", [12138]), 0);

// memory_copy.wast:2683
assert_return(() => call($15, "load8_u", [12337]), 0);

// memory_copy.wast:2684
assert_return(() => call($15, "load8_u", [12536]), 0);

// memory_copy.wast:2685
assert_return(() => call($15, "load8_u", [12735]), 0);

// memory_copy.wast:2686
assert_return(() => call($15, "load8_u", [12934]), 0);

// memory_copy.wast:2687
assert_return(() => call($15, "load8_u", [13133]), 0);

// memory_copy.wast:2688
assert_return(() => call($15, "load8_u", [13332]), 0);

// memory_copy.wast:2689
assert_return(() => call($15, "load8_u", [13531]), 0);

// memory_copy.wast:2690
assert_return(() => call($15, "load8_u", [13730]), 0);

// memory_copy.wast:2691
assert_return(() => call($15, "load8_u", [13929]), 0);

// memory_copy.wast:2692
assert_return(() => call($15, "load8_u", [14128]), 0);

// memory_copy.wast:2693
assert_return(() => call($15, "load8_u", [14327]), 0);

// memory_copy.wast:2694
assert_return(() => call($15, "load8_u", [14526]), 0);

// memory_copy.wast:2695
assert_return(() => call($15, "load8_u", [14725]), 0);

// memory_copy.wast:2696
assert_return(() => call($15, "load8_u", [14924]), 0);

// memory_copy.wast:2697
assert_return(() => call($15, "load8_u", [15123]), 0);

// memory_copy.wast:2698
assert_return(() => call($15, "load8_u", [15322]), 0);

// memory_copy.wast:2699
assert_return(() => call($15, "load8_u", [15521]), 0);

// memory_copy.wast:2700
assert_return(() => call($15, "load8_u", [15720]), 0);

// memory_copy.wast:2701
assert_return(() => call($15, "load8_u", [15919]), 0);

// memory_copy.wast:2702
assert_return(() => call($15, "load8_u", [16118]), 0);

// memory_copy.wast:2703
assert_return(() => call($15, "load8_u", [16317]), 0);

// memory_copy.wast:2704
assert_return(() => call($15, "load8_u", [16516]), 0);

// memory_copy.wast:2705
assert_return(() => call($15, "load8_u", [16715]), 0);

// memory_copy.wast:2706
assert_return(() => call($15, "load8_u", [16914]), 0);

// memory_copy.wast:2707
assert_return(() => call($15, "load8_u", [17113]), 0);

// memory_copy.wast:2708
assert_return(() => call($15, "load8_u", [17312]), 0);

// memory_copy.wast:2709
assert_return(() => call($15, "load8_u", [17511]), 0);

// memory_copy.wast:2710
assert_return(() => call($15, "load8_u", [17710]), 0);

// memory_copy.wast:2711
assert_return(() => call($15, "load8_u", [17909]), 0);

// memory_copy.wast:2712
assert_return(() => call($15, "load8_u", [18108]), 0);

// memory_copy.wast:2713
assert_return(() => call($15, "load8_u", [18307]), 0);

// memory_copy.wast:2714
assert_return(() => call($15, "load8_u", [18506]), 0);

// memory_copy.wast:2715
assert_return(() => call($15, "load8_u", [18705]), 0);

// memory_copy.wast:2716
assert_return(() => call($15, "load8_u", [18904]), 0);

// memory_copy.wast:2717
assert_return(() => call($15, "load8_u", [19103]), 0);

// memory_copy.wast:2718
assert_return(() => call($15, "load8_u", [19302]), 0);

// memory_copy.wast:2719
assert_return(() => call($15, "load8_u", [19501]), 0);

// memory_copy.wast:2720
assert_return(() => call($15, "load8_u", [19700]), 0);

// memory_copy.wast:2721
assert_return(() => call($15, "load8_u", [19899]), 0);

// memory_copy.wast:2722
assert_return(() => call($15, "load8_u", [20098]), 0);

// memory_copy.wast:2723
assert_return(() => call($15, "load8_u", [20297]), 0);

// memory_copy.wast:2724
assert_return(() => call($15, "load8_u", [20496]), 0);

// memory_copy.wast:2725
assert_return(() => call($15, "load8_u", [20695]), 0);

// memory_copy.wast:2726
assert_return(() => call($15, "load8_u", [20894]), 0);

// memory_copy.wast:2727
assert_return(() => call($15, "load8_u", [21093]), 0);

// memory_copy.wast:2728
assert_return(() => call($15, "load8_u", [21292]), 0);

// memory_copy.wast:2729
assert_return(() => call($15, "load8_u", [21491]), 0);

// memory_copy.wast:2730
assert_return(() => call($15, "load8_u", [21690]), 0);

// memory_copy.wast:2731
assert_return(() => call($15, "load8_u", [21889]), 0);

// memory_copy.wast:2732
assert_return(() => call($15, "load8_u", [22088]), 0);

// memory_copy.wast:2733
assert_return(() => call($15, "load8_u", [22287]), 0);

// memory_copy.wast:2734
assert_return(() => call($15, "load8_u", [22486]), 0);

// memory_copy.wast:2735
assert_return(() => call($15, "load8_u", [22685]), 0);

// memory_copy.wast:2736
assert_return(() => call($15, "load8_u", [22884]), 0);

// memory_copy.wast:2737
assert_return(() => call($15, "load8_u", [23083]), 0);

// memory_copy.wast:2738
assert_return(() => call($15, "load8_u", [23282]), 0);

// memory_copy.wast:2739
assert_return(() => call($15, "load8_u", [23481]), 0);

// memory_copy.wast:2740
assert_return(() => call($15, "load8_u", [23680]), 0);

// memory_copy.wast:2741
assert_return(() => call($15, "load8_u", [23879]), 0);

// memory_copy.wast:2742
assert_return(() => call($15, "load8_u", [24078]), 0);

// memory_copy.wast:2743
assert_return(() => call($15, "load8_u", [24277]), 0);

// memory_copy.wast:2744
assert_return(() => call($15, "load8_u", [24476]), 0);

// memory_copy.wast:2745
assert_return(() => call($15, "load8_u", [24675]), 0);

// memory_copy.wast:2746
assert_return(() => call($15, "load8_u", [24874]), 0);

// memory_copy.wast:2747
assert_return(() => call($15, "load8_u", [25073]), 0);

// memory_copy.wast:2748
assert_return(() => call($15, "load8_u", [25272]), 0);

// memory_copy.wast:2749
assert_return(() => call($15, "load8_u", [25471]), 0);

// memory_copy.wast:2750
assert_return(() => call($15, "load8_u", [25670]), 0);

// memory_copy.wast:2751
assert_return(() => call($15, "load8_u", [25869]), 0);

// memory_copy.wast:2752
assert_return(() => call($15, "load8_u", [26068]), 0);

// memory_copy.wast:2753
assert_return(() => call($15, "load8_u", [26267]), 0);

// memory_copy.wast:2754
assert_return(() => call($15, "load8_u", [26466]), 0);

// memory_copy.wast:2755
assert_return(() => call($15, "load8_u", [26665]), 0);

// memory_copy.wast:2756
assert_return(() => call($15, "load8_u", [26864]), 0);

// memory_copy.wast:2757
assert_return(() => call($15, "load8_u", [27063]), 0);

// memory_copy.wast:2758
assert_return(() => call($15, "load8_u", [27262]), 0);

// memory_copy.wast:2759
assert_return(() => call($15, "load8_u", [27461]), 0);

// memory_copy.wast:2760
assert_return(() => call($15, "load8_u", [27660]), 0);

// memory_copy.wast:2761
assert_return(() => call($15, "load8_u", [27859]), 0);

// memory_copy.wast:2762
assert_return(() => call($15, "load8_u", [28058]), 0);

// memory_copy.wast:2763
assert_return(() => call($15, "load8_u", [28257]), 0);

// memory_copy.wast:2764
assert_return(() => call($15, "load8_u", [28456]), 0);

// memory_copy.wast:2765
assert_return(() => call($15, "load8_u", [28655]), 0);

// memory_copy.wast:2766
assert_return(() => call($15, "load8_u", [28854]), 0);

// memory_copy.wast:2767
assert_return(() => call($15, "load8_u", [29053]), 0);

// memory_copy.wast:2768
assert_return(() => call($15, "load8_u", [29252]), 0);

// memory_copy.wast:2769
assert_return(() => call($15, "load8_u", [29451]), 0);

// memory_copy.wast:2770
assert_return(() => call($15, "load8_u", [29650]), 0);

// memory_copy.wast:2771
assert_return(() => call($15, "load8_u", [29849]), 0);

// memory_copy.wast:2772
assert_return(() => call($15, "load8_u", [30048]), 0);

// memory_copy.wast:2773
assert_return(() => call($15, "load8_u", [30247]), 0);

// memory_copy.wast:2774
assert_return(() => call($15, "load8_u", [30446]), 0);

// memory_copy.wast:2775
assert_return(() => call($15, "load8_u", [30645]), 0);

// memory_copy.wast:2776
assert_return(() => call($15, "load8_u", [30844]), 0);

// memory_copy.wast:2777
assert_return(() => call($15, "load8_u", [31043]), 0);

// memory_copy.wast:2778
assert_return(() => call($15, "load8_u", [31242]), 0);

// memory_copy.wast:2779
assert_return(() => call($15, "load8_u", [31441]), 0);

// memory_copy.wast:2780
assert_return(() => call($15, "load8_u", [31640]), 0);

// memory_copy.wast:2781
assert_return(() => call($15, "load8_u", [31839]), 0);

// memory_copy.wast:2782
assert_return(() => call($15, "load8_u", [32038]), 0);

// memory_copy.wast:2783
assert_return(() => call($15, "load8_u", [32237]), 0);

// memory_copy.wast:2784
assert_return(() => call($15, "load8_u", [32436]), 0);

// memory_copy.wast:2785
assert_return(() => call($15, "load8_u", [32635]), 0);

// memory_copy.wast:2786
assert_return(() => call($15, "load8_u", [32834]), 0);

// memory_copy.wast:2787
assert_return(() => call($15, "load8_u", [33033]), 0);

// memory_copy.wast:2788
assert_return(() => call($15, "load8_u", [33232]), 0);

// memory_copy.wast:2789
assert_return(() => call($15, "load8_u", [33431]), 0);

// memory_copy.wast:2790
assert_return(() => call($15, "load8_u", [33630]), 0);

// memory_copy.wast:2791
assert_return(() => call($15, "load8_u", [33829]), 0);

// memory_copy.wast:2792
assert_return(() => call($15, "load8_u", [34028]), 0);

// memory_copy.wast:2793
assert_return(() => call($15, "load8_u", [34227]), 0);

// memory_copy.wast:2794
assert_return(() => call($15, "load8_u", [34426]), 0);

// memory_copy.wast:2795
assert_return(() => call($15, "load8_u", [34625]), 0);

// memory_copy.wast:2796
assert_return(() => call($15, "load8_u", [34824]), 0);

// memory_copy.wast:2797
assert_return(() => call($15, "load8_u", [35023]), 0);

// memory_copy.wast:2798
assert_return(() => call($15, "load8_u", [35222]), 0);

// memory_copy.wast:2799
assert_return(() => call($15, "load8_u", [35421]), 0);

// memory_copy.wast:2800
assert_return(() => call($15, "load8_u", [35620]), 0);

// memory_copy.wast:2801
assert_return(() => call($15, "load8_u", [35819]), 0);

// memory_copy.wast:2802
assert_return(() => call($15, "load8_u", [36018]), 0);

// memory_copy.wast:2803
assert_return(() => call($15, "load8_u", [36217]), 0);

// memory_copy.wast:2804
assert_return(() => call($15, "load8_u", [36416]), 0);

// memory_copy.wast:2805
assert_return(() => call($15, "load8_u", [36615]), 0);

// memory_copy.wast:2806
assert_return(() => call($15, "load8_u", [36814]), 0);

// memory_copy.wast:2807
assert_return(() => call($15, "load8_u", [37013]), 0);

// memory_copy.wast:2808
assert_return(() => call($15, "load8_u", [37212]), 0);

// memory_copy.wast:2809
assert_return(() => call($15, "load8_u", [37411]), 0);

// memory_copy.wast:2810
assert_return(() => call($15, "load8_u", [37610]), 0);

// memory_copy.wast:2811
assert_return(() => call($15, "load8_u", [37809]), 0);

// memory_copy.wast:2812
assert_return(() => call($15, "load8_u", [38008]), 0);

// memory_copy.wast:2813
assert_return(() => call($15, "load8_u", [38207]), 0);

// memory_copy.wast:2814
assert_return(() => call($15, "load8_u", [38406]), 0);

// memory_copy.wast:2815
assert_return(() => call($15, "load8_u", [38605]), 0);

// memory_copy.wast:2816
assert_return(() => call($15, "load8_u", [38804]), 0);

// memory_copy.wast:2817
assert_return(() => call($15, "load8_u", [39003]), 0);

// memory_copy.wast:2818
assert_return(() => call($15, "load8_u", [39202]), 0);

// memory_copy.wast:2819
assert_return(() => call($15, "load8_u", [39401]), 0);

// memory_copy.wast:2820
assert_return(() => call($15, "load8_u", [39600]), 0);

// memory_copy.wast:2821
assert_return(() => call($15, "load8_u", [39799]), 0);

// memory_copy.wast:2822
assert_return(() => call($15, "load8_u", [39998]), 0);

// memory_copy.wast:2823
assert_return(() => call($15, "load8_u", [40197]), 0);

// memory_copy.wast:2824
assert_return(() => call($15, "load8_u", [40396]), 0);

// memory_copy.wast:2825
assert_return(() => call($15, "load8_u", [40595]), 0);

// memory_copy.wast:2826
assert_return(() => call($15, "load8_u", [40794]), 0);

// memory_copy.wast:2827
assert_return(() => call($15, "load8_u", [40993]), 0);

// memory_copy.wast:2828
assert_return(() => call($15, "load8_u", [41192]), 0);

// memory_copy.wast:2829
assert_return(() => call($15, "load8_u", [41391]), 0);

// memory_copy.wast:2830
assert_return(() => call($15, "load8_u", [41590]), 0);

// memory_copy.wast:2831
assert_return(() => call($15, "load8_u", [41789]), 0);

// memory_copy.wast:2832
assert_return(() => call($15, "load8_u", [41988]), 0);

// memory_copy.wast:2833
assert_return(() => call($15, "load8_u", [42187]), 0);

// memory_copy.wast:2834
assert_return(() => call($15, "load8_u", [42386]), 0);

// memory_copy.wast:2835
assert_return(() => call($15, "load8_u", [42585]), 0);

// memory_copy.wast:2836
assert_return(() => call($15, "load8_u", [42784]), 0);

// memory_copy.wast:2837
assert_return(() => call($15, "load8_u", [42983]), 0);

// memory_copy.wast:2838
assert_return(() => call($15, "load8_u", [43182]), 0);

// memory_copy.wast:2839
assert_return(() => call($15, "load8_u", [43381]), 0);

// memory_copy.wast:2840
assert_return(() => call($15, "load8_u", [43580]), 0);

// memory_copy.wast:2841
assert_return(() => call($15, "load8_u", [43779]), 0);

// memory_copy.wast:2842
assert_return(() => call($15, "load8_u", [43978]), 0);

// memory_copy.wast:2843
assert_return(() => call($15, "load8_u", [44177]), 0);

// memory_copy.wast:2844
assert_return(() => call($15, "load8_u", [44376]), 0);

// memory_copy.wast:2845
assert_return(() => call($15, "load8_u", [44575]), 0);

// memory_copy.wast:2846
assert_return(() => call($15, "load8_u", [44774]), 0);

// memory_copy.wast:2847
assert_return(() => call($15, "load8_u", [44973]), 0);

// memory_copy.wast:2848
assert_return(() => call($15, "load8_u", [45172]), 0);

// memory_copy.wast:2849
assert_return(() => call($15, "load8_u", [45371]), 0);

// memory_copy.wast:2850
assert_return(() => call($15, "load8_u", [45570]), 0);

// memory_copy.wast:2851
assert_return(() => call($15, "load8_u", [45769]), 0);

// memory_copy.wast:2852
assert_return(() => call($15, "load8_u", [45968]), 0);

// memory_copy.wast:2853
assert_return(() => call($15, "load8_u", [46167]), 0);

// memory_copy.wast:2854
assert_return(() => call($15, "load8_u", [46366]), 0);

// memory_copy.wast:2855
assert_return(() => call($15, "load8_u", [46565]), 0);

// memory_copy.wast:2856
assert_return(() => call($15, "load8_u", [46764]), 0);

// memory_copy.wast:2857
assert_return(() => call($15, "load8_u", [46963]), 0);

// memory_copy.wast:2858
assert_return(() => call($15, "load8_u", [47162]), 0);

// memory_copy.wast:2859
assert_return(() => call($15, "load8_u", [47361]), 0);

// memory_copy.wast:2860
assert_return(() => call($15, "load8_u", [47560]), 0);

// memory_copy.wast:2861
assert_return(() => call($15, "load8_u", [47759]), 0);

// memory_copy.wast:2862
assert_return(() => call($15, "load8_u", [47958]), 0);

// memory_copy.wast:2863
assert_return(() => call($15, "load8_u", [48157]), 0);

// memory_copy.wast:2864
assert_return(() => call($15, "load8_u", [48356]), 0);

// memory_copy.wast:2865
assert_return(() => call($15, "load8_u", [48555]), 0);

// memory_copy.wast:2866
assert_return(() => call($15, "load8_u", [48754]), 0);

// memory_copy.wast:2867
assert_return(() => call($15, "load8_u", [48953]), 0);

// memory_copy.wast:2868
assert_return(() => call($15, "load8_u", [49152]), 0);

// memory_copy.wast:2869
assert_return(() => call($15, "load8_u", [49351]), 0);

// memory_copy.wast:2870
assert_return(() => call($15, "load8_u", [49550]), 0);

// memory_copy.wast:2871
assert_return(() => call($15, "load8_u", [49749]), 0);

// memory_copy.wast:2872
assert_return(() => call($15, "load8_u", [49948]), 0);

// memory_copy.wast:2873
assert_return(() => call($15, "load8_u", [50147]), 0);

// memory_copy.wast:2874
assert_return(() => call($15, "load8_u", [50346]), 0);

// memory_copy.wast:2875
assert_return(() => call($15, "load8_u", [50545]), 0);

// memory_copy.wast:2876
assert_return(() => call($15, "load8_u", [50744]), 0);

// memory_copy.wast:2877
assert_return(() => call($15, "load8_u", [50943]), 0);

// memory_copy.wast:2878
assert_return(() => call($15, "load8_u", [51142]), 0);

// memory_copy.wast:2879
assert_return(() => call($15, "load8_u", [51341]), 0);

// memory_copy.wast:2880
assert_return(() => call($15, "load8_u", [51540]), 0);

// memory_copy.wast:2881
assert_return(() => call($15, "load8_u", [51739]), 0);

// memory_copy.wast:2882
assert_return(() => call($15, "load8_u", [51938]), 0);

// memory_copy.wast:2883
assert_return(() => call($15, "load8_u", [52137]), 0);

// memory_copy.wast:2884
assert_return(() => call($15, "load8_u", [52336]), 0);

// memory_copy.wast:2885
assert_return(() => call($15, "load8_u", [52535]), 0);

// memory_copy.wast:2886
assert_return(() => call($15, "load8_u", [52734]), 0);

// memory_copy.wast:2887
assert_return(() => call($15, "load8_u", [52933]), 0);

// memory_copy.wast:2888
assert_return(() => call($15, "load8_u", [53132]), 0);

// memory_copy.wast:2889
assert_return(() => call($15, "load8_u", [53331]), 0);

// memory_copy.wast:2890
assert_return(() => call($15, "load8_u", [53530]), 0);

// memory_copy.wast:2891
assert_return(() => call($15, "load8_u", [53729]), 0);

// memory_copy.wast:2892
assert_return(() => call($15, "load8_u", [53928]), 0);

// memory_copy.wast:2893
assert_return(() => call($15, "load8_u", [54127]), 0);

// memory_copy.wast:2894
assert_return(() => call($15, "load8_u", [54326]), 0);

// memory_copy.wast:2895
assert_return(() => call($15, "load8_u", [54525]), 0);

// memory_copy.wast:2896
assert_return(() => call($15, "load8_u", [54724]), 0);

// memory_copy.wast:2897
assert_return(() => call($15, "load8_u", [54923]), 0);

// memory_copy.wast:2898
assert_return(() => call($15, "load8_u", [55122]), 0);

// memory_copy.wast:2899
assert_return(() => call($15, "load8_u", [55321]), 0);

// memory_copy.wast:2900
assert_return(() => call($15, "load8_u", [55520]), 0);

// memory_copy.wast:2901
assert_return(() => call($15, "load8_u", [55719]), 0);

// memory_copy.wast:2902
assert_return(() => call($15, "load8_u", [55918]), 0);

// memory_copy.wast:2903
assert_return(() => call($15, "load8_u", [56117]), 0);

// memory_copy.wast:2904
assert_return(() => call($15, "load8_u", [56316]), 0);

// memory_copy.wast:2905
assert_return(() => call($15, "load8_u", [56515]), 0);

// memory_copy.wast:2906
assert_return(() => call($15, "load8_u", [56714]), 0);

// memory_copy.wast:2907
assert_return(() => call($15, "load8_u", [56913]), 0);

// memory_copy.wast:2908
assert_return(() => call($15, "load8_u", [57112]), 0);

// memory_copy.wast:2909
assert_return(() => call($15, "load8_u", [57311]), 0);

// memory_copy.wast:2910
assert_return(() => call($15, "load8_u", [57510]), 0);

// memory_copy.wast:2911
assert_return(() => call($15, "load8_u", [57709]), 0);

// memory_copy.wast:2912
assert_return(() => call($15, "load8_u", [57908]), 0);

// memory_copy.wast:2913
assert_return(() => call($15, "load8_u", [58107]), 0);

// memory_copy.wast:2914
assert_return(() => call($15, "load8_u", [58306]), 0);

// memory_copy.wast:2915
assert_return(() => call($15, "load8_u", [58505]), 0);

// memory_copy.wast:2916
assert_return(() => call($15, "load8_u", [58704]), 0);

// memory_copy.wast:2917
assert_return(() => call($15, "load8_u", [58903]), 0);

// memory_copy.wast:2918
assert_return(() => call($15, "load8_u", [59102]), 0);

// memory_copy.wast:2919
assert_return(() => call($15, "load8_u", [59301]), 0);

// memory_copy.wast:2920
assert_return(() => call($15, "load8_u", [59500]), 0);

// memory_copy.wast:2921
assert_return(() => call($15, "load8_u", [59699]), 0);

// memory_copy.wast:2922
assert_return(() => call($15, "load8_u", [59898]), 0);

// memory_copy.wast:2923
assert_return(() => call($15, "load8_u", [60097]), 0);

// memory_copy.wast:2924
assert_return(() => call($15, "load8_u", [60296]), 0);

// memory_copy.wast:2925
assert_return(() => call($15, "load8_u", [60495]), 0);

// memory_copy.wast:2926
assert_return(() => call($15, "load8_u", [60694]), 0);

// memory_copy.wast:2927
assert_return(() => call($15, "load8_u", [60893]), 0);

// memory_copy.wast:2928
assert_return(() => call($15, "load8_u", [61092]), 0);

// memory_copy.wast:2929
assert_return(() => call($15, "load8_u", [61291]), 0);

// memory_copy.wast:2930
assert_return(() => call($15, "load8_u", [61490]), 0);

// memory_copy.wast:2931
assert_return(() => call($15, "load8_u", [61689]), 0);

// memory_copy.wast:2932
assert_return(() => call($15, "load8_u", [61888]), 0);

// memory_copy.wast:2933
assert_return(() => call($15, "load8_u", [62087]), 0);

// memory_copy.wast:2934
assert_return(() => call($15, "load8_u", [62286]), 0);

// memory_copy.wast:2935
assert_return(() => call($15, "load8_u", [62485]), 0);

// memory_copy.wast:2936
assert_return(() => call($15, "load8_u", [62684]), 0);

// memory_copy.wast:2937
assert_return(() => call($15, "load8_u", [62883]), 0);

// memory_copy.wast:2938
assert_return(() => call($15, "load8_u", [63082]), 0);

// memory_copy.wast:2939
assert_return(() => call($15, "load8_u", [63281]), 0);

// memory_copy.wast:2940
assert_return(() => call($15, "load8_u", [63480]), 0);

// memory_copy.wast:2941
assert_return(() => call($15, "load8_u", [63679]), 0);

// memory_copy.wast:2942
assert_return(() => call($15, "load8_u", [63878]), 0);

// memory_copy.wast:2943
assert_return(() => call($15, "load8_u", [64077]), 0);

// memory_copy.wast:2944
assert_return(() => call($15, "load8_u", [64276]), 0);

// memory_copy.wast:2945
assert_return(() => call($15, "load8_u", [64475]), 0);

// memory_copy.wast:2946
assert_return(() => call($15, "load8_u", [64674]), 0);

// memory_copy.wast:2947
assert_return(() => call($15, "load8_u", [64873]), 0);

// memory_copy.wast:2948
assert_return(() => call($15, "load8_u", [65072]), 0);

// memory_copy.wast:2949
assert_return(() => call($15, "load8_u", [65271]), 0);

// memory_copy.wast:2950
assert_return(() => call($15, "load8_u", [65470]), 0);

// memory_copy.wast:2951
assert_return(() => call($15, "load8_u", [65506]), 0);

// memory_copy.wast:2952
assert_return(() => call($15, "load8_u", [65507]), 1);

// memory_copy.wast:2953
assert_return(() => call($15, "load8_u", [65508]), 2);

// memory_copy.wast:2954
assert_return(() => call($15, "load8_u", [65509]), 3);

// memory_copy.wast:2955
assert_return(() => call($15, "load8_u", [65510]), 4);

// memory_copy.wast:2956
assert_return(() => call($15, "load8_u", [65511]), 5);

// memory_copy.wast:2957
assert_return(() => call($15, "load8_u", [65512]), 6);

// memory_copy.wast:2958
assert_return(() => call($15, "load8_u", [65513]), 7);

// memory_copy.wast:2959
assert_return(() => call($15, "load8_u", [65514]), 8);

// memory_copy.wast:2960
assert_return(() => call($15, "load8_u", [65515]), 9);

// memory_copy.wast:2961
assert_return(() => call($15, "load8_u", [65516]), 10);

// memory_copy.wast:2962
assert_return(() => call($15, "load8_u", [65517]), 11);

// memory_copy.wast:2963
assert_return(() => call($15, "load8_u", [65518]), 12);

// memory_copy.wast:2964
assert_return(() => call($15, "load8_u", [65519]), 13);

// memory_copy.wast:2965
assert_return(() => call($15, "load8_u", [65520]), 14);

// memory_copy.wast:2966
assert_return(() => call($15, "load8_u", [65521]), 15);

// memory_copy.wast:2967
assert_return(() => call($15, "load8_u", [65522]), 16);

// memory_copy.wast:2968
assert_return(() => call($15, "load8_u", [65523]), 17);

// memory_copy.wast:2969
assert_return(() => call($15, "load8_u", [65524]), 18);

// memory_copy.wast:2970
assert_return(() => call($15, "load8_u", [65525]), 19);

// memory_copy.wast:2972
let $16 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xec\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:2980
assert_trap(() => call($16, "run", [65506, 65516, 40]));

// memory_copy.wast:2983
assert_return(() => call($16, "load8_u", [198]), 0);

// memory_copy.wast:2984
assert_return(() => call($16, "load8_u", [397]), 0);

// memory_copy.wast:2985
assert_return(() => call($16, "load8_u", [596]), 0);

// memory_copy.wast:2986
assert_return(() => call($16, "load8_u", [795]), 0);

// memory_copy.wast:2987
assert_return(() => call($16, "load8_u", [994]), 0);

// memory_copy.wast:2988
assert_return(() => call($16, "load8_u", [1193]), 0);

// memory_copy.wast:2989
assert_return(() => call($16, "load8_u", [1392]), 0);

// memory_copy.wast:2990
assert_return(() => call($16, "load8_u", [1591]), 0);

// memory_copy.wast:2991
assert_return(() => call($16, "load8_u", [1790]), 0);

// memory_copy.wast:2992
assert_return(() => call($16, "load8_u", [1989]), 0);

// memory_copy.wast:2993
assert_return(() => call($16, "load8_u", [2188]), 0);

// memory_copy.wast:2994
assert_return(() => call($16, "load8_u", [2387]), 0);

// memory_copy.wast:2995
assert_return(() => call($16, "load8_u", [2586]), 0);

// memory_copy.wast:2996
assert_return(() => call($16, "load8_u", [2785]), 0);

// memory_copy.wast:2997
assert_return(() => call($16, "load8_u", [2984]), 0);

// memory_copy.wast:2998
assert_return(() => call($16, "load8_u", [3183]), 0);

// memory_copy.wast:2999
assert_return(() => call($16, "load8_u", [3382]), 0);

// memory_copy.wast:3000
assert_return(() => call($16, "load8_u", [3581]), 0);

// memory_copy.wast:3001
assert_return(() => call($16, "load8_u", [3780]), 0);

// memory_copy.wast:3002
assert_return(() => call($16, "load8_u", [3979]), 0);

// memory_copy.wast:3003
assert_return(() => call($16, "load8_u", [4178]), 0);

// memory_copy.wast:3004
assert_return(() => call($16, "load8_u", [4377]), 0);

// memory_copy.wast:3005
assert_return(() => call($16, "load8_u", [4576]), 0);

// memory_copy.wast:3006
assert_return(() => call($16, "load8_u", [4775]), 0);

// memory_copy.wast:3007
assert_return(() => call($16, "load8_u", [4974]), 0);

// memory_copy.wast:3008
assert_return(() => call($16, "load8_u", [5173]), 0);

// memory_copy.wast:3009
assert_return(() => call($16, "load8_u", [5372]), 0);

// memory_copy.wast:3010
assert_return(() => call($16, "load8_u", [5571]), 0);

// memory_copy.wast:3011
assert_return(() => call($16, "load8_u", [5770]), 0);

// memory_copy.wast:3012
assert_return(() => call($16, "load8_u", [5969]), 0);

// memory_copy.wast:3013
assert_return(() => call($16, "load8_u", [6168]), 0);

// memory_copy.wast:3014
assert_return(() => call($16, "load8_u", [6367]), 0);

// memory_copy.wast:3015
assert_return(() => call($16, "load8_u", [6566]), 0);

// memory_copy.wast:3016
assert_return(() => call($16, "load8_u", [6765]), 0);

// memory_copy.wast:3017
assert_return(() => call($16, "load8_u", [6964]), 0);

// memory_copy.wast:3018
assert_return(() => call($16, "load8_u", [7163]), 0);

// memory_copy.wast:3019
assert_return(() => call($16, "load8_u", [7362]), 0);

// memory_copy.wast:3020
assert_return(() => call($16, "load8_u", [7561]), 0);

// memory_copy.wast:3021
assert_return(() => call($16, "load8_u", [7760]), 0);

// memory_copy.wast:3022
assert_return(() => call($16, "load8_u", [7959]), 0);

// memory_copy.wast:3023
assert_return(() => call($16, "load8_u", [8158]), 0);

// memory_copy.wast:3024
assert_return(() => call($16, "load8_u", [8357]), 0);

// memory_copy.wast:3025
assert_return(() => call($16, "load8_u", [8556]), 0);

// memory_copy.wast:3026
assert_return(() => call($16, "load8_u", [8755]), 0);

// memory_copy.wast:3027
assert_return(() => call($16, "load8_u", [8954]), 0);

// memory_copy.wast:3028
assert_return(() => call($16, "load8_u", [9153]), 0);

// memory_copy.wast:3029
assert_return(() => call($16, "load8_u", [9352]), 0);

// memory_copy.wast:3030
assert_return(() => call($16, "load8_u", [9551]), 0);

// memory_copy.wast:3031
assert_return(() => call($16, "load8_u", [9750]), 0);

// memory_copy.wast:3032
assert_return(() => call($16, "load8_u", [9949]), 0);

// memory_copy.wast:3033
assert_return(() => call($16, "load8_u", [10148]), 0);

// memory_copy.wast:3034
assert_return(() => call($16, "load8_u", [10347]), 0);

// memory_copy.wast:3035
assert_return(() => call($16, "load8_u", [10546]), 0);

// memory_copy.wast:3036
assert_return(() => call($16, "load8_u", [10745]), 0);

// memory_copy.wast:3037
assert_return(() => call($16, "load8_u", [10944]), 0);

// memory_copy.wast:3038
assert_return(() => call($16, "load8_u", [11143]), 0);

// memory_copy.wast:3039
assert_return(() => call($16, "load8_u", [11342]), 0);

// memory_copy.wast:3040
assert_return(() => call($16, "load8_u", [11541]), 0);

// memory_copy.wast:3041
assert_return(() => call($16, "load8_u", [11740]), 0);

// memory_copy.wast:3042
assert_return(() => call($16, "load8_u", [11939]), 0);

// memory_copy.wast:3043
assert_return(() => call($16, "load8_u", [12138]), 0);

// memory_copy.wast:3044
assert_return(() => call($16, "load8_u", [12337]), 0);

// memory_copy.wast:3045
assert_return(() => call($16, "load8_u", [12536]), 0);

// memory_copy.wast:3046
assert_return(() => call($16, "load8_u", [12735]), 0);

// memory_copy.wast:3047
assert_return(() => call($16, "load8_u", [12934]), 0);

// memory_copy.wast:3048
assert_return(() => call($16, "load8_u", [13133]), 0);

// memory_copy.wast:3049
assert_return(() => call($16, "load8_u", [13332]), 0);

// memory_copy.wast:3050
assert_return(() => call($16, "load8_u", [13531]), 0);

// memory_copy.wast:3051
assert_return(() => call($16, "load8_u", [13730]), 0);

// memory_copy.wast:3052
assert_return(() => call($16, "load8_u", [13929]), 0);

// memory_copy.wast:3053
assert_return(() => call($16, "load8_u", [14128]), 0);

// memory_copy.wast:3054
assert_return(() => call($16, "load8_u", [14327]), 0);

// memory_copy.wast:3055
assert_return(() => call($16, "load8_u", [14526]), 0);

// memory_copy.wast:3056
assert_return(() => call($16, "load8_u", [14725]), 0);

// memory_copy.wast:3057
assert_return(() => call($16, "load8_u", [14924]), 0);

// memory_copy.wast:3058
assert_return(() => call($16, "load8_u", [15123]), 0);

// memory_copy.wast:3059
assert_return(() => call($16, "load8_u", [15322]), 0);

// memory_copy.wast:3060
assert_return(() => call($16, "load8_u", [15521]), 0);

// memory_copy.wast:3061
assert_return(() => call($16, "load8_u", [15720]), 0);

// memory_copy.wast:3062
assert_return(() => call($16, "load8_u", [15919]), 0);

// memory_copy.wast:3063
assert_return(() => call($16, "load8_u", [16118]), 0);

// memory_copy.wast:3064
assert_return(() => call($16, "load8_u", [16317]), 0);

// memory_copy.wast:3065
assert_return(() => call($16, "load8_u", [16516]), 0);

// memory_copy.wast:3066
assert_return(() => call($16, "load8_u", [16715]), 0);

// memory_copy.wast:3067
assert_return(() => call($16, "load8_u", [16914]), 0);

// memory_copy.wast:3068
assert_return(() => call($16, "load8_u", [17113]), 0);

// memory_copy.wast:3069
assert_return(() => call($16, "load8_u", [17312]), 0);

// memory_copy.wast:3070
assert_return(() => call($16, "load8_u", [17511]), 0);

// memory_copy.wast:3071
assert_return(() => call($16, "load8_u", [17710]), 0);

// memory_copy.wast:3072
assert_return(() => call($16, "load8_u", [17909]), 0);

// memory_copy.wast:3073
assert_return(() => call($16, "load8_u", [18108]), 0);

// memory_copy.wast:3074
assert_return(() => call($16, "load8_u", [18307]), 0);

// memory_copy.wast:3075
assert_return(() => call($16, "load8_u", [18506]), 0);

// memory_copy.wast:3076
assert_return(() => call($16, "load8_u", [18705]), 0);

// memory_copy.wast:3077
assert_return(() => call($16, "load8_u", [18904]), 0);

// memory_copy.wast:3078
assert_return(() => call($16, "load8_u", [19103]), 0);

// memory_copy.wast:3079
assert_return(() => call($16, "load8_u", [19302]), 0);

// memory_copy.wast:3080
assert_return(() => call($16, "load8_u", [19501]), 0);

// memory_copy.wast:3081
assert_return(() => call($16, "load8_u", [19700]), 0);

// memory_copy.wast:3082
assert_return(() => call($16, "load8_u", [19899]), 0);

// memory_copy.wast:3083
assert_return(() => call($16, "load8_u", [20098]), 0);

// memory_copy.wast:3084
assert_return(() => call($16, "load8_u", [20297]), 0);

// memory_copy.wast:3085
assert_return(() => call($16, "load8_u", [20496]), 0);

// memory_copy.wast:3086
assert_return(() => call($16, "load8_u", [20695]), 0);

// memory_copy.wast:3087
assert_return(() => call($16, "load8_u", [20894]), 0);

// memory_copy.wast:3088
assert_return(() => call($16, "load8_u", [21093]), 0);

// memory_copy.wast:3089
assert_return(() => call($16, "load8_u", [21292]), 0);

// memory_copy.wast:3090
assert_return(() => call($16, "load8_u", [21491]), 0);

// memory_copy.wast:3091
assert_return(() => call($16, "load8_u", [21690]), 0);

// memory_copy.wast:3092
assert_return(() => call($16, "load8_u", [21889]), 0);

// memory_copy.wast:3093
assert_return(() => call($16, "load8_u", [22088]), 0);

// memory_copy.wast:3094
assert_return(() => call($16, "load8_u", [22287]), 0);

// memory_copy.wast:3095
assert_return(() => call($16, "load8_u", [22486]), 0);

// memory_copy.wast:3096
assert_return(() => call($16, "load8_u", [22685]), 0);

// memory_copy.wast:3097
assert_return(() => call($16, "load8_u", [22884]), 0);

// memory_copy.wast:3098
assert_return(() => call($16, "load8_u", [23083]), 0);

// memory_copy.wast:3099
assert_return(() => call($16, "load8_u", [23282]), 0);

// memory_copy.wast:3100
assert_return(() => call($16, "load8_u", [23481]), 0);

// memory_copy.wast:3101
assert_return(() => call($16, "load8_u", [23680]), 0);

// memory_copy.wast:3102
assert_return(() => call($16, "load8_u", [23879]), 0);

// memory_copy.wast:3103
assert_return(() => call($16, "load8_u", [24078]), 0);

// memory_copy.wast:3104
assert_return(() => call($16, "load8_u", [24277]), 0);

// memory_copy.wast:3105
assert_return(() => call($16, "load8_u", [24476]), 0);

// memory_copy.wast:3106
assert_return(() => call($16, "load8_u", [24675]), 0);

// memory_copy.wast:3107
assert_return(() => call($16, "load8_u", [24874]), 0);

// memory_copy.wast:3108
assert_return(() => call($16, "load8_u", [25073]), 0);

// memory_copy.wast:3109
assert_return(() => call($16, "load8_u", [25272]), 0);

// memory_copy.wast:3110
assert_return(() => call($16, "load8_u", [25471]), 0);

// memory_copy.wast:3111
assert_return(() => call($16, "load8_u", [25670]), 0);

// memory_copy.wast:3112
assert_return(() => call($16, "load8_u", [25869]), 0);

// memory_copy.wast:3113
assert_return(() => call($16, "load8_u", [26068]), 0);

// memory_copy.wast:3114
assert_return(() => call($16, "load8_u", [26267]), 0);

// memory_copy.wast:3115
assert_return(() => call($16, "load8_u", [26466]), 0);

// memory_copy.wast:3116
assert_return(() => call($16, "load8_u", [26665]), 0);

// memory_copy.wast:3117
assert_return(() => call($16, "load8_u", [26864]), 0);

// memory_copy.wast:3118
assert_return(() => call($16, "load8_u", [27063]), 0);

// memory_copy.wast:3119
assert_return(() => call($16, "load8_u", [27262]), 0);

// memory_copy.wast:3120
assert_return(() => call($16, "load8_u", [27461]), 0);

// memory_copy.wast:3121
assert_return(() => call($16, "load8_u", [27660]), 0);

// memory_copy.wast:3122
assert_return(() => call($16, "load8_u", [27859]), 0);

// memory_copy.wast:3123
assert_return(() => call($16, "load8_u", [28058]), 0);

// memory_copy.wast:3124
assert_return(() => call($16, "load8_u", [28257]), 0);

// memory_copy.wast:3125
assert_return(() => call($16, "load8_u", [28456]), 0);

// memory_copy.wast:3126
assert_return(() => call($16, "load8_u", [28655]), 0);

// memory_copy.wast:3127
assert_return(() => call($16, "load8_u", [28854]), 0);

// memory_copy.wast:3128
assert_return(() => call($16, "load8_u", [29053]), 0);

// memory_copy.wast:3129
assert_return(() => call($16, "load8_u", [29252]), 0);

// memory_copy.wast:3130
assert_return(() => call($16, "load8_u", [29451]), 0);

// memory_copy.wast:3131
assert_return(() => call($16, "load8_u", [29650]), 0);

// memory_copy.wast:3132
assert_return(() => call($16, "load8_u", [29849]), 0);

// memory_copy.wast:3133
assert_return(() => call($16, "load8_u", [30048]), 0);

// memory_copy.wast:3134
assert_return(() => call($16, "load8_u", [30247]), 0);

// memory_copy.wast:3135
assert_return(() => call($16, "load8_u", [30446]), 0);

// memory_copy.wast:3136
assert_return(() => call($16, "load8_u", [30645]), 0);

// memory_copy.wast:3137
assert_return(() => call($16, "load8_u", [30844]), 0);

// memory_copy.wast:3138
assert_return(() => call($16, "load8_u", [31043]), 0);

// memory_copy.wast:3139
assert_return(() => call($16, "load8_u", [31242]), 0);

// memory_copy.wast:3140
assert_return(() => call($16, "load8_u", [31441]), 0);

// memory_copy.wast:3141
assert_return(() => call($16, "load8_u", [31640]), 0);

// memory_copy.wast:3142
assert_return(() => call($16, "load8_u", [31839]), 0);

// memory_copy.wast:3143
assert_return(() => call($16, "load8_u", [32038]), 0);

// memory_copy.wast:3144
assert_return(() => call($16, "load8_u", [32237]), 0);

// memory_copy.wast:3145
assert_return(() => call($16, "load8_u", [32436]), 0);

// memory_copy.wast:3146
assert_return(() => call($16, "load8_u", [32635]), 0);

// memory_copy.wast:3147
assert_return(() => call($16, "load8_u", [32834]), 0);

// memory_copy.wast:3148
assert_return(() => call($16, "load8_u", [33033]), 0);

// memory_copy.wast:3149
assert_return(() => call($16, "load8_u", [33232]), 0);

// memory_copy.wast:3150
assert_return(() => call($16, "load8_u", [33431]), 0);

// memory_copy.wast:3151
assert_return(() => call($16, "load8_u", [33630]), 0);

// memory_copy.wast:3152
assert_return(() => call($16, "load8_u", [33829]), 0);

// memory_copy.wast:3153
assert_return(() => call($16, "load8_u", [34028]), 0);

// memory_copy.wast:3154
assert_return(() => call($16, "load8_u", [34227]), 0);

// memory_copy.wast:3155
assert_return(() => call($16, "load8_u", [34426]), 0);

// memory_copy.wast:3156
assert_return(() => call($16, "load8_u", [34625]), 0);

// memory_copy.wast:3157
assert_return(() => call($16, "load8_u", [34824]), 0);

// memory_copy.wast:3158
assert_return(() => call($16, "load8_u", [35023]), 0);

// memory_copy.wast:3159
assert_return(() => call($16, "load8_u", [35222]), 0);

// memory_copy.wast:3160
assert_return(() => call($16, "load8_u", [35421]), 0);

// memory_copy.wast:3161
assert_return(() => call($16, "load8_u", [35620]), 0);

// memory_copy.wast:3162
assert_return(() => call($16, "load8_u", [35819]), 0);

// memory_copy.wast:3163
assert_return(() => call($16, "load8_u", [36018]), 0);

// memory_copy.wast:3164
assert_return(() => call($16, "load8_u", [36217]), 0);

// memory_copy.wast:3165
assert_return(() => call($16, "load8_u", [36416]), 0);

// memory_copy.wast:3166
assert_return(() => call($16, "load8_u", [36615]), 0);

// memory_copy.wast:3167
assert_return(() => call($16, "load8_u", [36814]), 0);

// memory_copy.wast:3168
assert_return(() => call($16, "load8_u", [37013]), 0);

// memory_copy.wast:3169
assert_return(() => call($16, "load8_u", [37212]), 0);

// memory_copy.wast:3170
assert_return(() => call($16, "load8_u", [37411]), 0);

// memory_copy.wast:3171
assert_return(() => call($16, "load8_u", [37610]), 0);

// memory_copy.wast:3172
assert_return(() => call($16, "load8_u", [37809]), 0);

// memory_copy.wast:3173
assert_return(() => call($16, "load8_u", [38008]), 0);

// memory_copy.wast:3174
assert_return(() => call($16, "load8_u", [38207]), 0);

// memory_copy.wast:3175
assert_return(() => call($16, "load8_u", [38406]), 0);

// memory_copy.wast:3176
assert_return(() => call($16, "load8_u", [38605]), 0);

// memory_copy.wast:3177
assert_return(() => call($16, "load8_u", [38804]), 0);

// memory_copy.wast:3178
assert_return(() => call($16, "load8_u", [39003]), 0);

// memory_copy.wast:3179
assert_return(() => call($16, "load8_u", [39202]), 0);

// memory_copy.wast:3180
assert_return(() => call($16, "load8_u", [39401]), 0);

// memory_copy.wast:3181
assert_return(() => call($16, "load8_u", [39600]), 0);

// memory_copy.wast:3182
assert_return(() => call($16, "load8_u", [39799]), 0);

// memory_copy.wast:3183
assert_return(() => call($16, "load8_u", [39998]), 0);

// memory_copy.wast:3184
assert_return(() => call($16, "load8_u", [40197]), 0);

// memory_copy.wast:3185
assert_return(() => call($16, "load8_u", [40396]), 0);

// memory_copy.wast:3186
assert_return(() => call($16, "load8_u", [40595]), 0);

// memory_copy.wast:3187
assert_return(() => call($16, "load8_u", [40794]), 0);

// memory_copy.wast:3188
assert_return(() => call($16, "load8_u", [40993]), 0);

// memory_copy.wast:3189
assert_return(() => call($16, "load8_u", [41192]), 0);

// memory_copy.wast:3190
assert_return(() => call($16, "load8_u", [41391]), 0);

// memory_copy.wast:3191
assert_return(() => call($16, "load8_u", [41590]), 0);

// memory_copy.wast:3192
assert_return(() => call($16, "load8_u", [41789]), 0);

// memory_copy.wast:3193
assert_return(() => call($16, "load8_u", [41988]), 0);

// memory_copy.wast:3194
assert_return(() => call($16, "load8_u", [42187]), 0);

// memory_copy.wast:3195
assert_return(() => call($16, "load8_u", [42386]), 0);

// memory_copy.wast:3196
assert_return(() => call($16, "load8_u", [42585]), 0);

// memory_copy.wast:3197
assert_return(() => call($16, "load8_u", [42784]), 0);

// memory_copy.wast:3198
assert_return(() => call($16, "load8_u", [42983]), 0);

// memory_copy.wast:3199
assert_return(() => call($16, "load8_u", [43182]), 0);

// memory_copy.wast:3200
assert_return(() => call($16, "load8_u", [43381]), 0);

// memory_copy.wast:3201
assert_return(() => call($16, "load8_u", [43580]), 0);

// memory_copy.wast:3202
assert_return(() => call($16, "load8_u", [43779]), 0);

// memory_copy.wast:3203
assert_return(() => call($16, "load8_u", [43978]), 0);

// memory_copy.wast:3204
assert_return(() => call($16, "load8_u", [44177]), 0);

// memory_copy.wast:3205
assert_return(() => call($16, "load8_u", [44376]), 0);

// memory_copy.wast:3206
assert_return(() => call($16, "load8_u", [44575]), 0);

// memory_copy.wast:3207
assert_return(() => call($16, "load8_u", [44774]), 0);

// memory_copy.wast:3208
assert_return(() => call($16, "load8_u", [44973]), 0);

// memory_copy.wast:3209
assert_return(() => call($16, "load8_u", [45172]), 0);

// memory_copy.wast:3210
assert_return(() => call($16, "load8_u", [45371]), 0);

// memory_copy.wast:3211
assert_return(() => call($16, "load8_u", [45570]), 0);

// memory_copy.wast:3212
assert_return(() => call($16, "load8_u", [45769]), 0);

// memory_copy.wast:3213
assert_return(() => call($16, "load8_u", [45968]), 0);

// memory_copy.wast:3214
assert_return(() => call($16, "load8_u", [46167]), 0);

// memory_copy.wast:3215
assert_return(() => call($16, "load8_u", [46366]), 0);

// memory_copy.wast:3216
assert_return(() => call($16, "load8_u", [46565]), 0);

// memory_copy.wast:3217
assert_return(() => call($16, "load8_u", [46764]), 0);

// memory_copy.wast:3218
assert_return(() => call($16, "load8_u", [46963]), 0);

// memory_copy.wast:3219
assert_return(() => call($16, "load8_u", [47162]), 0);

// memory_copy.wast:3220
assert_return(() => call($16, "load8_u", [47361]), 0);

// memory_copy.wast:3221
assert_return(() => call($16, "load8_u", [47560]), 0);

// memory_copy.wast:3222
assert_return(() => call($16, "load8_u", [47759]), 0);

// memory_copy.wast:3223
assert_return(() => call($16, "load8_u", [47958]), 0);

// memory_copy.wast:3224
assert_return(() => call($16, "load8_u", [48157]), 0);

// memory_copy.wast:3225
assert_return(() => call($16, "load8_u", [48356]), 0);

// memory_copy.wast:3226
assert_return(() => call($16, "load8_u", [48555]), 0);

// memory_copy.wast:3227
assert_return(() => call($16, "load8_u", [48754]), 0);

// memory_copy.wast:3228
assert_return(() => call($16, "load8_u", [48953]), 0);

// memory_copy.wast:3229
assert_return(() => call($16, "load8_u", [49152]), 0);

// memory_copy.wast:3230
assert_return(() => call($16, "load8_u", [49351]), 0);

// memory_copy.wast:3231
assert_return(() => call($16, "load8_u", [49550]), 0);

// memory_copy.wast:3232
assert_return(() => call($16, "load8_u", [49749]), 0);

// memory_copy.wast:3233
assert_return(() => call($16, "load8_u", [49948]), 0);

// memory_copy.wast:3234
assert_return(() => call($16, "load8_u", [50147]), 0);

// memory_copy.wast:3235
assert_return(() => call($16, "load8_u", [50346]), 0);

// memory_copy.wast:3236
assert_return(() => call($16, "load8_u", [50545]), 0);

// memory_copy.wast:3237
assert_return(() => call($16, "load8_u", [50744]), 0);

// memory_copy.wast:3238
assert_return(() => call($16, "load8_u", [50943]), 0);

// memory_copy.wast:3239
assert_return(() => call($16, "load8_u", [51142]), 0);

// memory_copy.wast:3240
assert_return(() => call($16, "load8_u", [51341]), 0);

// memory_copy.wast:3241
assert_return(() => call($16, "load8_u", [51540]), 0);

// memory_copy.wast:3242
assert_return(() => call($16, "load8_u", [51739]), 0);

// memory_copy.wast:3243
assert_return(() => call($16, "load8_u", [51938]), 0);

// memory_copy.wast:3244
assert_return(() => call($16, "load8_u", [52137]), 0);

// memory_copy.wast:3245
assert_return(() => call($16, "load8_u", [52336]), 0);

// memory_copy.wast:3246
assert_return(() => call($16, "load8_u", [52535]), 0);

// memory_copy.wast:3247
assert_return(() => call($16, "load8_u", [52734]), 0);

// memory_copy.wast:3248
assert_return(() => call($16, "load8_u", [52933]), 0);

// memory_copy.wast:3249
assert_return(() => call($16, "load8_u", [53132]), 0);

// memory_copy.wast:3250
assert_return(() => call($16, "load8_u", [53331]), 0);

// memory_copy.wast:3251
assert_return(() => call($16, "load8_u", [53530]), 0);

// memory_copy.wast:3252
assert_return(() => call($16, "load8_u", [53729]), 0);

// memory_copy.wast:3253
assert_return(() => call($16, "load8_u", [53928]), 0);

// memory_copy.wast:3254
assert_return(() => call($16, "load8_u", [54127]), 0);

// memory_copy.wast:3255
assert_return(() => call($16, "load8_u", [54326]), 0);

// memory_copy.wast:3256
assert_return(() => call($16, "load8_u", [54525]), 0);

// memory_copy.wast:3257
assert_return(() => call($16, "load8_u", [54724]), 0);

// memory_copy.wast:3258
assert_return(() => call($16, "load8_u", [54923]), 0);

// memory_copy.wast:3259
assert_return(() => call($16, "load8_u", [55122]), 0);

// memory_copy.wast:3260
assert_return(() => call($16, "load8_u", [55321]), 0);

// memory_copy.wast:3261
assert_return(() => call($16, "load8_u", [55520]), 0);

// memory_copy.wast:3262
assert_return(() => call($16, "load8_u", [55719]), 0);

// memory_copy.wast:3263
assert_return(() => call($16, "load8_u", [55918]), 0);

// memory_copy.wast:3264
assert_return(() => call($16, "load8_u", [56117]), 0);

// memory_copy.wast:3265
assert_return(() => call($16, "load8_u", [56316]), 0);

// memory_copy.wast:3266
assert_return(() => call($16, "load8_u", [56515]), 0);

// memory_copy.wast:3267
assert_return(() => call($16, "load8_u", [56714]), 0);

// memory_copy.wast:3268
assert_return(() => call($16, "load8_u", [56913]), 0);

// memory_copy.wast:3269
assert_return(() => call($16, "load8_u", [57112]), 0);

// memory_copy.wast:3270
assert_return(() => call($16, "load8_u", [57311]), 0);

// memory_copy.wast:3271
assert_return(() => call($16, "load8_u", [57510]), 0);

// memory_copy.wast:3272
assert_return(() => call($16, "load8_u", [57709]), 0);

// memory_copy.wast:3273
assert_return(() => call($16, "load8_u", [57908]), 0);

// memory_copy.wast:3274
assert_return(() => call($16, "load8_u", [58107]), 0);

// memory_copy.wast:3275
assert_return(() => call($16, "load8_u", [58306]), 0);

// memory_copy.wast:3276
assert_return(() => call($16, "load8_u", [58505]), 0);

// memory_copy.wast:3277
assert_return(() => call($16, "load8_u", [58704]), 0);

// memory_copy.wast:3278
assert_return(() => call($16, "load8_u", [58903]), 0);

// memory_copy.wast:3279
assert_return(() => call($16, "load8_u", [59102]), 0);

// memory_copy.wast:3280
assert_return(() => call($16, "load8_u", [59301]), 0);

// memory_copy.wast:3281
assert_return(() => call($16, "load8_u", [59500]), 0);

// memory_copy.wast:3282
assert_return(() => call($16, "load8_u", [59699]), 0);

// memory_copy.wast:3283
assert_return(() => call($16, "load8_u", [59898]), 0);

// memory_copy.wast:3284
assert_return(() => call($16, "load8_u", [60097]), 0);

// memory_copy.wast:3285
assert_return(() => call($16, "load8_u", [60296]), 0);

// memory_copy.wast:3286
assert_return(() => call($16, "load8_u", [60495]), 0);

// memory_copy.wast:3287
assert_return(() => call($16, "load8_u", [60694]), 0);

// memory_copy.wast:3288
assert_return(() => call($16, "load8_u", [60893]), 0);

// memory_copy.wast:3289
assert_return(() => call($16, "load8_u", [61092]), 0);

// memory_copy.wast:3290
assert_return(() => call($16, "load8_u", [61291]), 0);

// memory_copy.wast:3291
assert_return(() => call($16, "load8_u", [61490]), 0);

// memory_copy.wast:3292
assert_return(() => call($16, "load8_u", [61689]), 0);

// memory_copy.wast:3293
assert_return(() => call($16, "load8_u", [61888]), 0);

// memory_copy.wast:3294
assert_return(() => call($16, "load8_u", [62087]), 0);

// memory_copy.wast:3295
assert_return(() => call($16, "load8_u", [62286]), 0);

// memory_copy.wast:3296
assert_return(() => call($16, "load8_u", [62485]), 0);

// memory_copy.wast:3297
assert_return(() => call($16, "load8_u", [62684]), 0);

// memory_copy.wast:3298
assert_return(() => call($16, "load8_u", [62883]), 0);

// memory_copy.wast:3299
assert_return(() => call($16, "load8_u", [63082]), 0);

// memory_copy.wast:3300
assert_return(() => call($16, "load8_u", [63281]), 0);

// memory_copy.wast:3301
assert_return(() => call($16, "load8_u", [63480]), 0);

// memory_copy.wast:3302
assert_return(() => call($16, "load8_u", [63679]), 0);

// memory_copy.wast:3303
assert_return(() => call($16, "load8_u", [63878]), 0);

// memory_copy.wast:3304
assert_return(() => call($16, "load8_u", [64077]), 0);

// memory_copy.wast:3305
assert_return(() => call($16, "load8_u", [64276]), 0);

// memory_copy.wast:3306
assert_return(() => call($16, "load8_u", [64475]), 0);

// memory_copy.wast:3307
assert_return(() => call($16, "load8_u", [64674]), 0);

// memory_copy.wast:3308
assert_return(() => call($16, "load8_u", [64873]), 0);

// memory_copy.wast:3309
assert_return(() => call($16, "load8_u", [65072]), 0);

// memory_copy.wast:3310
assert_return(() => call($16, "load8_u", [65271]), 0);

// memory_copy.wast:3311
assert_return(() => call($16, "load8_u", [65470]), 0);

// memory_copy.wast:3312
assert_return(() => call($16, "load8_u", [65506]), 0);

// memory_copy.wast:3313
assert_return(() => call($16, "load8_u", [65507]), 1);

// memory_copy.wast:3314
assert_return(() => call($16, "load8_u", [65508]), 2);

// memory_copy.wast:3315
assert_return(() => call($16, "load8_u", [65509]), 3);

// memory_copy.wast:3316
assert_return(() => call($16, "load8_u", [65510]), 4);

// memory_copy.wast:3317
assert_return(() => call($16, "load8_u", [65511]), 5);

// memory_copy.wast:3318
assert_return(() => call($16, "load8_u", [65512]), 6);

// memory_copy.wast:3319
assert_return(() => call($16, "load8_u", [65513]), 7);

// memory_copy.wast:3320
assert_return(() => call($16, "load8_u", [65514]), 8);

// memory_copy.wast:3321
assert_return(() => call($16, "load8_u", [65515]), 9);

// memory_copy.wast:3322
assert_return(() => call($16, "load8_u", [65516]), 10);

// memory_copy.wast:3323
assert_return(() => call($16, "load8_u", [65517]), 11);

// memory_copy.wast:3324
assert_return(() => call($16, "load8_u", [65518]), 12);

// memory_copy.wast:3325
assert_return(() => call($16, "load8_u", [65519]), 13);

// memory_copy.wast:3326
assert_return(() => call($16, "load8_u", [65520]), 14);

// memory_copy.wast:3327
assert_return(() => call($16, "load8_u", [65521]), 15);

// memory_copy.wast:3328
assert_return(() => call($16, "load8_u", [65522]), 16);

// memory_copy.wast:3329
assert_return(() => call($16, "load8_u", [65523]), 17);

// memory_copy.wast:3330
assert_return(() => call($16, "load8_u", [65524]), 18);

// memory_copy.wast:3331
assert_return(() => call($16, "load8_u", [65525]), 19);

// memory_copy.wast:3332
assert_return(() => call($16, "load8_u", [65526]), 10);

// memory_copy.wast:3333
assert_return(() => call($16, "load8_u", [65527]), 11);

// memory_copy.wast:3334
assert_return(() => call($16, "load8_u", [65528]), 12);

// memory_copy.wast:3335
assert_return(() => call($16, "load8_u", [65529]), 13);

// memory_copy.wast:3336
assert_return(() => call($16, "load8_u", [65530]), 14);

// memory_copy.wast:3337
assert_return(() => call($16, "load8_u", [65531]), 15);

// memory_copy.wast:3338
assert_return(() => call($16, "load8_u", [65532]), 16);

// memory_copy.wast:3339
assert_return(() => call($16, "load8_u", [65533]), 17);

// memory_copy.wast:3340
assert_return(() => call($16, "load8_u", [65534]), 18);

// memory_copy.wast:3341
assert_return(() => call($16, "load8_u", [65535]), 19);

// memory_copy.wast:3343
let $17 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xec\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:3351
assert_trap(() => call($17, "run", [65516, 65516, 40]));

// memory_copy.wast:3354
assert_return(() => call($17, "load8_u", [198]), 0);

// memory_copy.wast:3355
assert_return(() => call($17, "load8_u", [397]), 0);

// memory_copy.wast:3356
assert_return(() => call($17, "load8_u", [596]), 0);

// memory_copy.wast:3357
assert_return(() => call($17, "load8_u", [795]), 0);

// memory_copy.wast:3358
assert_return(() => call($17, "load8_u", [994]), 0);

// memory_copy.wast:3359
assert_return(() => call($17, "load8_u", [1193]), 0);

// memory_copy.wast:3360
assert_return(() => call($17, "load8_u", [1392]), 0);

// memory_copy.wast:3361
assert_return(() => call($17, "load8_u", [1591]), 0);

// memory_copy.wast:3362
assert_return(() => call($17, "load8_u", [1790]), 0);

// memory_copy.wast:3363
assert_return(() => call($17, "load8_u", [1989]), 0);

// memory_copy.wast:3364
assert_return(() => call($17, "load8_u", [2188]), 0);

// memory_copy.wast:3365
assert_return(() => call($17, "load8_u", [2387]), 0);

// memory_copy.wast:3366
assert_return(() => call($17, "load8_u", [2586]), 0);

// memory_copy.wast:3367
assert_return(() => call($17, "load8_u", [2785]), 0);

// memory_copy.wast:3368
assert_return(() => call($17, "load8_u", [2984]), 0);

// memory_copy.wast:3369
assert_return(() => call($17, "load8_u", [3183]), 0);

// memory_copy.wast:3370
assert_return(() => call($17, "load8_u", [3382]), 0);

// memory_copy.wast:3371
assert_return(() => call($17, "load8_u", [3581]), 0);

// memory_copy.wast:3372
assert_return(() => call($17, "load8_u", [3780]), 0);

// memory_copy.wast:3373
assert_return(() => call($17, "load8_u", [3979]), 0);

// memory_copy.wast:3374
assert_return(() => call($17, "load8_u", [4178]), 0);

// memory_copy.wast:3375
assert_return(() => call($17, "load8_u", [4377]), 0);

// memory_copy.wast:3376
assert_return(() => call($17, "load8_u", [4576]), 0);

// memory_copy.wast:3377
assert_return(() => call($17, "load8_u", [4775]), 0);

// memory_copy.wast:3378
assert_return(() => call($17, "load8_u", [4974]), 0);

// memory_copy.wast:3379
assert_return(() => call($17, "load8_u", [5173]), 0);

// memory_copy.wast:3380
assert_return(() => call($17, "load8_u", [5372]), 0);

// memory_copy.wast:3381
assert_return(() => call($17, "load8_u", [5571]), 0);

// memory_copy.wast:3382
assert_return(() => call($17, "load8_u", [5770]), 0);

// memory_copy.wast:3383
assert_return(() => call($17, "load8_u", [5969]), 0);

// memory_copy.wast:3384
assert_return(() => call($17, "load8_u", [6168]), 0);

// memory_copy.wast:3385
assert_return(() => call($17, "load8_u", [6367]), 0);

// memory_copy.wast:3386
assert_return(() => call($17, "load8_u", [6566]), 0);

// memory_copy.wast:3387
assert_return(() => call($17, "load8_u", [6765]), 0);

// memory_copy.wast:3388
assert_return(() => call($17, "load8_u", [6964]), 0);

// memory_copy.wast:3389
assert_return(() => call($17, "load8_u", [7163]), 0);

// memory_copy.wast:3390
assert_return(() => call($17, "load8_u", [7362]), 0);

// memory_copy.wast:3391
assert_return(() => call($17, "load8_u", [7561]), 0);

// memory_copy.wast:3392
assert_return(() => call($17, "load8_u", [7760]), 0);

// memory_copy.wast:3393
assert_return(() => call($17, "load8_u", [7959]), 0);

// memory_copy.wast:3394
assert_return(() => call($17, "load8_u", [8158]), 0);

// memory_copy.wast:3395
assert_return(() => call($17, "load8_u", [8357]), 0);

// memory_copy.wast:3396
assert_return(() => call($17, "load8_u", [8556]), 0);

// memory_copy.wast:3397
assert_return(() => call($17, "load8_u", [8755]), 0);

// memory_copy.wast:3398
assert_return(() => call($17, "load8_u", [8954]), 0);

// memory_copy.wast:3399
assert_return(() => call($17, "load8_u", [9153]), 0);

// memory_copy.wast:3400
assert_return(() => call($17, "load8_u", [9352]), 0);

// memory_copy.wast:3401
assert_return(() => call($17, "load8_u", [9551]), 0);

// memory_copy.wast:3402
assert_return(() => call($17, "load8_u", [9750]), 0);

// memory_copy.wast:3403
assert_return(() => call($17, "load8_u", [9949]), 0);

// memory_copy.wast:3404
assert_return(() => call($17, "load8_u", [10148]), 0);

// memory_copy.wast:3405
assert_return(() => call($17, "load8_u", [10347]), 0);

// memory_copy.wast:3406
assert_return(() => call($17, "load8_u", [10546]), 0);

// memory_copy.wast:3407
assert_return(() => call($17, "load8_u", [10745]), 0);

// memory_copy.wast:3408
assert_return(() => call($17, "load8_u", [10944]), 0);

// memory_copy.wast:3409
assert_return(() => call($17, "load8_u", [11143]), 0);

// memory_copy.wast:3410
assert_return(() => call($17, "load8_u", [11342]), 0);

// memory_copy.wast:3411
assert_return(() => call($17, "load8_u", [11541]), 0);

// memory_copy.wast:3412
assert_return(() => call($17, "load8_u", [11740]), 0);

// memory_copy.wast:3413
assert_return(() => call($17, "load8_u", [11939]), 0);

// memory_copy.wast:3414
assert_return(() => call($17, "load8_u", [12138]), 0);

// memory_copy.wast:3415
assert_return(() => call($17, "load8_u", [12337]), 0);

// memory_copy.wast:3416
assert_return(() => call($17, "load8_u", [12536]), 0);

// memory_copy.wast:3417
assert_return(() => call($17, "load8_u", [12735]), 0);

// memory_copy.wast:3418
assert_return(() => call($17, "load8_u", [12934]), 0);

// memory_copy.wast:3419
assert_return(() => call($17, "load8_u", [13133]), 0);

// memory_copy.wast:3420
assert_return(() => call($17, "load8_u", [13332]), 0);

// memory_copy.wast:3421
assert_return(() => call($17, "load8_u", [13531]), 0);

// memory_copy.wast:3422
assert_return(() => call($17, "load8_u", [13730]), 0);

// memory_copy.wast:3423
assert_return(() => call($17, "load8_u", [13929]), 0);

// memory_copy.wast:3424
assert_return(() => call($17, "load8_u", [14128]), 0);

// memory_copy.wast:3425
assert_return(() => call($17, "load8_u", [14327]), 0);

// memory_copy.wast:3426
assert_return(() => call($17, "load8_u", [14526]), 0);

// memory_copy.wast:3427
assert_return(() => call($17, "load8_u", [14725]), 0);

// memory_copy.wast:3428
assert_return(() => call($17, "load8_u", [14924]), 0);

// memory_copy.wast:3429
assert_return(() => call($17, "load8_u", [15123]), 0);

// memory_copy.wast:3430
assert_return(() => call($17, "load8_u", [15322]), 0);

// memory_copy.wast:3431
assert_return(() => call($17, "load8_u", [15521]), 0);

// memory_copy.wast:3432
assert_return(() => call($17, "load8_u", [15720]), 0);

// memory_copy.wast:3433
assert_return(() => call($17, "load8_u", [15919]), 0);

// memory_copy.wast:3434
assert_return(() => call($17, "load8_u", [16118]), 0);

// memory_copy.wast:3435
assert_return(() => call($17, "load8_u", [16317]), 0);

// memory_copy.wast:3436
assert_return(() => call($17, "load8_u", [16516]), 0);

// memory_copy.wast:3437
assert_return(() => call($17, "load8_u", [16715]), 0);

// memory_copy.wast:3438
assert_return(() => call($17, "load8_u", [16914]), 0);

// memory_copy.wast:3439
assert_return(() => call($17, "load8_u", [17113]), 0);

// memory_copy.wast:3440
assert_return(() => call($17, "load8_u", [17312]), 0);

// memory_copy.wast:3441
assert_return(() => call($17, "load8_u", [17511]), 0);

// memory_copy.wast:3442
assert_return(() => call($17, "load8_u", [17710]), 0);

// memory_copy.wast:3443
assert_return(() => call($17, "load8_u", [17909]), 0);

// memory_copy.wast:3444
assert_return(() => call($17, "load8_u", [18108]), 0);

// memory_copy.wast:3445
assert_return(() => call($17, "load8_u", [18307]), 0);

// memory_copy.wast:3446
assert_return(() => call($17, "load8_u", [18506]), 0);

// memory_copy.wast:3447
assert_return(() => call($17, "load8_u", [18705]), 0);

// memory_copy.wast:3448
assert_return(() => call($17, "load8_u", [18904]), 0);

// memory_copy.wast:3449
assert_return(() => call($17, "load8_u", [19103]), 0);

// memory_copy.wast:3450
assert_return(() => call($17, "load8_u", [19302]), 0);

// memory_copy.wast:3451
assert_return(() => call($17, "load8_u", [19501]), 0);

// memory_copy.wast:3452
assert_return(() => call($17, "load8_u", [19700]), 0);

// memory_copy.wast:3453
assert_return(() => call($17, "load8_u", [19899]), 0);

// memory_copy.wast:3454
assert_return(() => call($17, "load8_u", [20098]), 0);

// memory_copy.wast:3455
assert_return(() => call($17, "load8_u", [20297]), 0);

// memory_copy.wast:3456
assert_return(() => call($17, "load8_u", [20496]), 0);

// memory_copy.wast:3457
assert_return(() => call($17, "load8_u", [20695]), 0);

// memory_copy.wast:3458
assert_return(() => call($17, "load8_u", [20894]), 0);

// memory_copy.wast:3459
assert_return(() => call($17, "load8_u", [21093]), 0);

// memory_copy.wast:3460
assert_return(() => call($17, "load8_u", [21292]), 0);

// memory_copy.wast:3461
assert_return(() => call($17, "load8_u", [21491]), 0);

// memory_copy.wast:3462
assert_return(() => call($17, "load8_u", [21690]), 0);

// memory_copy.wast:3463
assert_return(() => call($17, "load8_u", [21889]), 0);

// memory_copy.wast:3464
assert_return(() => call($17, "load8_u", [22088]), 0);

// memory_copy.wast:3465
assert_return(() => call($17, "load8_u", [22287]), 0);

// memory_copy.wast:3466
assert_return(() => call($17, "load8_u", [22486]), 0);

// memory_copy.wast:3467
assert_return(() => call($17, "load8_u", [22685]), 0);

// memory_copy.wast:3468
assert_return(() => call($17, "load8_u", [22884]), 0);

// memory_copy.wast:3469
assert_return(() => call($17, "load8_u", [23083]), 0);

// memory_copy.wast:3470
assert_return(() => call($17, "load8_u", [23282]), 0);

// memory_copy.wast:3471
assert_return(() => call($17, "load8_u", [23481]), 0);

// memory_copy.wast:3472
assert_return(() => call($17, "load8_u", [23680]), 0);

// memory_copy.wast:3473
assert_return(() => call($17, "load8_u", [23879]), 0);

// memory_copy.wast:3474
assert_return(() => call($17, "load8_u", [24078]), 0);

// memory_copy.wast:3475
assert_return(() => call($17, "load8_u", [24277]), 0);

// memory_copy.wast:3476
assert_return(() => call($17, "load8_u", [24476]), 0);

// memory_copy.wast:3477
assert_return(() => call($17, "load8_u", [24675]), 0);

// memory_copy.wast:3478
assert_return(() => call($17, "load8_u", [24874]), 0);

// memory_copy.wast:3479
assert_return(() => call($17, "load8_u", [25073]), 0);

// memory_copy.wast:3480
assert_return(() => call($17, "load8_u", [25272]), 0);

// memory_copy.wast:3481
assert_return(() => call($17, "load8_u", [25471]), 0);

// memory_copy.wast:3482
assert_return(() => call($17, "load8_u", [25670]), 0);

// memory_copy.wast:3483
assert_return(() => call($17, "load8_u", [25869]), 0);

// memory_copy.wast:3484
assert_return(() => call($17, "load8_u", [26068]), 0);

// memory_copy.wast:3485
assert_return(() => call($17, "load8_u", [26267]), 0);

// memory_copy.wast:3486
assert_return(() => call($17, "load8_u", [26466]), 0);

// memory_copy.wast:3487
assert_return(() => call($17, "load8_u", [26665]), 0);

// memory_copy.wast:3488
assert_return(() => call($17, "load8_u", [26864]), 0);

// memory_copy.wast:3489
assert_return(() => call($17, "load8_u", [27063]), 0);

// memory_copy.wast:3490
assert_return(() => call($17, "load8_u", [27262]), 0);

// memory_copy.wast:3491
assert_return(() => call($17, "load8_u", [27461]), 0);

// memory_copy.wast:3492
assert_return(() => call($17, "load8_u", [27660]), 0);

// memory_copy.wast:3493
assert_return(() => call($17, "load8_u", [27859]), 0);

// memory_copy.wast:3494
assert_return(() => call($17, "load8_u", [28058]), 0);

// memory_copy.wast:3495
assert_return(() => call($17, "load8_u", [28257]), 0);

// memory_copy.wast:3496
assert_return(() => call($17, "load8_u", [28456]), 0);

// memory_copy.wast:3497
assert_return(() => call($17, "load8_u", [28655]), 0);

// memory_copy.wast:3498
assert_return(() => call($17, "load8_u", [28854]), 0);

// memory_copy.wast:3499
assert_return(() => call($17, "load8_u", [29053]), 0);

// memory_copy.wast:3500
assert_return(() => call($17, "load8_u", [29252]), 0);

// memory_copy.wast:3501
assert_return(() => call($17, "load8_u", [29451]), 0);

// memory_copy.wast:3502
assert_return(() => call($17, "load8_u", [29650]), 0);

// memory_copy.wast:3503
assert_return(() => call($17, "load8_u", [29849]), 0);

// memory_copy.wast:3504
assert_return(() => call($17, "load8_u", [30048]), 0);

// memory_copy.wast:3505
assert_return(() => call($17, "load8_u", [30247]), 0);

// memory_copy.wast:3506
assert_return(() => call($17, "load8_u", [30446]), 0);

// memory_copy.wast:3507
assert_return(() => call($17, "load8_u", [30645]), 0);

// memory_copy.wast:3508
assert_return(() => call($17, "load8_u", [30844]), 0);

// memory_copy.wast:3509
assert_return(() => call($17, "load8_u", [31043]), 0);

// memory_copy.wast:3510
assert_return(() => call($17, "load8_u", [31242]), 0);

// memory_copy.wast:3511
assert_return(() => call($17, "load8_u", [31441]), 0);

// memory_copy.wast:3512
assert_return(() => call($17, "load8_u", [31640]), 0);

// memory_copy.wast:3513
assert_return(() => call($17, "load8_u", [31839]), 0);

// memory_copy.wast:3514
assert_return(() => call($17, "load8_u", [32038]), 0);

// memory_copy.wast:3515
assert_return(() => call($17, "load8_u", [32237]), 0);

// memory_copy.wast:3516
assert_return(() => call($17, "load8_u", [32436]), 0);

// memory_copy.wast:3517
assert_return(() => call($17, "load8_u", [32635]), 0);

// memory_copy.wast:3518
assert_return(() => call($17, "load8_u", [32834]), 0);

// memory_copy.wast:3519
assert_return(() => call($17, "load8_u", [33033]), 0);

// memory_copy.wast:3520
assert_return(() => call($17, "load8_u", [33232]), 0);

// memory_copy.wast:3521
assert_return(() => call($17, "load8_u", [33431]), 0);

// memory_copy.wast:3522
assert_return(() => call($17, "load8_u", [33630]), 0);

// memory_copy.wast:3523
assert_return(() => call($17, "load8_u", [33829]), 0);

// memory_copy.wast:3524
assert_return(() => call($17, "load8_u", [34028]), 0);

// memory_copy.wast:3525
assert_return(() => call($17, "load8_u", [34227]), 0);

// memory_copy.wast:3526
assert_return(() => call($17, "load8_u", [34426]), 0);

// memory_copy.wast:3527
assert_return(() => call($17, "load8_u", [34625]), 0);

// memory_copy.wast:3528
assert_return(() => call($17, "load8_u", [34824]), 0);

// memory_copy.wast:3529
assert_return(() => call($17, "load8_u", [35023]), 0);

// memory_copy.wast:3530
assert_return(() => call($17, "load8_u", [35222]), 0);

// memory_copy.wast:3531
assert_return(() => call($17, "load8_u", [35421]), 0);

// memory_copy.wast:3532
assert_return(() => call($17, "load8_u", [35620]), 0);

// memory_copy.wast:3533
assert_return(() => call($17, "load8_u", [35819]), 0);

// memory_copy.wast:3534
assert_return(() => call($17, "load8_u", [36018]), 0);

// memory_copy.wast:3535
assert_return(() => call($17, "load8_u", [36217]), 0);

// memory_copy.wast:3536
assert_return(() => call($17, "load8_u", [36416]), 0);

// memory_copy.wast:3537
assert_return(() => call($17, "load8_u", [36615]), 0);

// memory_copy.wast:3538
assert_return(() => call($17, "load8_u", [36814]), 0);

// memory_copy.wast:3539
assert_return(() => call($17, "load8_u", [37013]), 0);

// memory_copy.wast:3540
assert_return(() => call($17, "load8_u", [37212]), 0);

// memory_copy.wast:3541
assert_return(() => call($17, "load8_u", [37411]), 0);

// memory_copy.wast:3542
assert_return(() => call($17, "load8_u", [37610]), 0);

// memory_copy.wast:3543
assert_return(() => call($17, "load8_u", [37809]), 0);

// memory_copy.wast:3544
assert_return(() => call($17, "load8_u", [38008]), 0);

// memory_copy.wast:3545
assert_return(() => call($17, "load8_u", [38207]), 0);

// memory_copy.wast:3546
assert_return(() => call($17, "load8_u", [38406]), 0);

// memory_copy.wast:3547
assert_return(() => call($17, "load8_u", [38605]), 0);

// memory_copy.wast:3548
assert_return(() => call($17, "load8_u", [38804]), 0);

// memory_copy.wast:3549
assert_return(() => call($17, "load8_u", [39003]), 0);

// memory_copy.wast:3550
assert_return(() => call($17, "load8_u", [39202]), 0);

// memory_copy.wast:3551
assert_return(() => call($17, "load8_u", [39401]), 0);

// memory_copy.wast:3552
assert_return(() => call($17, "load8_u", [39600]), 0);

// memory_copy.wast:3553
assert_return(() => call($17, "load8_u", [39799]), 0);

// memory_copy.wast:3554
assert_return(() => call($17, "load8_u", [39998]), 0);

// memory_copy.wast:3555
assert_return(() => call($17, "load8_u", [40197]), 0);

// memory_copy.wast:3556
assert_return(() => call($17, "load8_u", [40396]), 0);

// memory_copy.wast:3557
assert_return(() => call($17, "load8_u", [40595]), 0);

// memory_copy.wast:3558
assert_return(() => call($17, "load8_u", [40794]), 0);

// memory_copy.wast:3559
assert_return(() => call($17, "load8_u", [40993]), 0);

// memory_copy.wast:3560
assert_return(() => call($17, "load8_u", [41192]), 0);

// memory_copy.wast:3561
assert_return(() => call($17, "load8_u", [41391]), 0);

// memory_copy.wast:3562
assert_return(() => call($17, "load8_u", [41590]), 0);

// memory_copy.wast:3563
assert_return(() => call($17, "load8_u", [41789]), 0);

// memory_copy.wast:3564
assert_return(() => call($17, "load8_u", [41988]), 0);

// memory_copy.wast:3565
assert_return(() => call($17, "load8_u", [42187]), 0);

// memory_copy.wast:3566
assert_return(() => call($17, "load8_u", [42386]), 0);

// memory_copy.wast:3567
assert_return(() => call($17, "load8_u", [42585]), 0);

// memory_copy.wast:3568
assert_return(() => call($17, "load8_u", [42784]), 0);

// memory_copy.wast:3569
assert_return(() => call($17, "load8_u", [42983]), 0);

// memory_copy.wast:3570
assert_return(() => call($17, "load8_u", [43182]), 0);

// memory_copy.wast:3571
assert_return(() => call($17, "load8_u", [43381]), 0);

// memory_copy.wast:3572
assert_return(() => call($17, "load8_u", [43580]), 0);

// memory_copy.wast:3573
assert_return(() => call($17, "load8_u", [43779]), 0);

// memory_copy.wast:3574
assert_return(() => call($17, "load8_u", [43978]), 0);

// memory_copy.wast:3575
assert_return(() => call($17, "load8_u", [44177]), 0);

// memory_copy.wast:3576
assert_return(() => call($17, "load8_u", [44376]), 0);

// memory_copy.wast:3577
assert_return(() => call($17, "load8_u", [44575]), 0);

// memory_copy.wast:3578
assert_return(() => call($17, "load8_u", [44774]), 0);

// memory_copy.wast:3579
assert_return(() => call($17, "load8_u", [44973]), 0);

// memory_copy.wast:3580
assert_return(() => call($17, "load8_u", [45172]), 0);

// memory_copy.wast:3581
assert_return(() => call($17, "load8_u", [45371]), 0);

// memory_copy.wast:3582
assert_return(() => call($17, "load8_u", [45570]), 0);

// memory_copy.wast:3583
assert_return(() => call($17, "load8_u", [45769]), 0);

// memory_copy.wast:3584
assert_return(() => call($17, "load8_u", [45968]), 0);

// memory_copy.wast:3585
assert_return(() => call($17, "load8_u", [46167]), 0);

// memory_copy.wast:3586
assert_return(() => call($17, "load8_u", [46366]), 0);

// memory_copy.wast:3587
assert_return(() => call($17, "load8_u", [46565]), 0);

// memory_copy.wast:3588
assert_return(() => call($17, "load8_u", [46764]), 0);

// memory_copy.wast:3589
assert_return(() => call($17, "load8_u", [46963]), 0);

// memory_copy.wast:3590
assert_return(() => call($17, "load8_u", [47162]), 0);

// memory_copy.wast:3591
assert_return(() => call($17, "load8_u", [47361]), 0);

// memory_copy.wast:3592
assert_return(() => call($17, "load8_u", [47560]), 0);

// memory_copy.wast:3593
assert_return(() => call($17, "load8_u", [47759]), 0);

// memory_copy.wast:3594
assert_return(() => call($17, "load8_u", [47958]), 0);

// memory_copy.wast:3595
assert_return(() => call($17, "load8_u", [48157]), 0);

// memory_copy.wast:3596
assert_return(() => call($17, "load8_u", [48356]), 0);

// memory_copy.wast:3597
assert_return(() => call($17, "load8_u", [48555]), 0);

// memory_copy.wast:3598
assert_return(() => call($17, "load8_u", [48754]), 0);

// memory_copy.wast:3599
assert_return(() => call($17, "load8_u", [48953]), 0);

// memory_copy.wast:3600
assert_return(() => call($17, "load8_u", [49152]), 0);

// memory_copy.wast:3601
assert_return(() => call($17, "load8_u", [49351]), 0);

// memory_copy.wast:3602
assert_return(() => call($17, "load8_u", [49550]), 0);

// memory_copy.wast:3603
assert_return(() => call($17, "load8_u", [49749]), 0);

// memory_copy.wast:3604
assert_return(() => call($17, "load8_u", [49948]), 0);

// memory_copy.wast:3605
assert_return(() => call($17, "load8_u", [50147]), 0);

// memory_copy.wast:3606
assert_return(() => call($17, "load8_u", [50346]), 0);

// memory_copy.wast:3607
assert_return(() => call($17, "load8_u", [50545]), 0);

// memory_copy.wast:3608
assert_return(() => call($17, "load8_u", [50744]), 0);

// memory_copy.wast:3609
assert_return(() => call($17, "load8_u", [50943]), 0);

// memory_copy.wast:3610
assert_return(() => call($17, "load8_u", [51142]), 0);

// memory_copy.wast:3611
assert_return(() => call($17, "load8_u", [51341]), 0);

// memory_copy.wast:3612
assert_return(() => call($17, "load8_u", [51540]), 0);

// memory_copy.wast:3613
assert_return(() => call($17, "load8_u", [51739]), 0);

// memory_copy.wast:3614
assert_return(() => call($17, "load8_u", [51938]), 0);

// memory_copy.wast:3615
assert_return(() => call($17, "load8_u", [52137]), 0);

// memory_copy.wast:3616
assert_return(() => call($17, "load8_u", [52336]), 0);

// memory_copy.wast:3617
assert_return(() => call($17, "load8_u", [52535]), 0);

// memory_copy.wast:3618
assert_return(() => call($17, "load8_u", [52734]), 0);

// memory_copy.wast:3619
assert_return(() => call($17, "load8_u", [52933]), 0);

// memory_copy.wast:3620
assert_return(() => call($17, "load8_u", [53132]), 0);

// memory_copy.wast:3621
assert_return(() => call($17, "load8_u", [53331]), 0);

// memory_copy.wast:3622
assert_return(() => call($17, "load8_u", [53530]), 0);

// memory_copy.wast:3623
assert_return(() => call($17, "load8_u", [53729]), 0);

// memory_copy.wast:3624
assert_return(() => call($17, "load8_u", [53928]), 0);

// memory_copy.wast:3625
assert_return(() => call($17, "load8_u", [54127]), 0);

// memory_copy.wast:3626
assert_return(() => call($17, "load8_u", [54326]), 0);

// memory_copy.wast:3627
assert_return(() => call($17, "load8_u", [54525]), 0);

// memory_copy.wast:3628
assert_return(() => call($17, "load8_u", [54724]), 0);

// memory_copy.wast:3629
assert_return(() => call($17, "load8_u", [54923]), 0);

// memory_copy.wast:3630
assert_return(() => call($17, "load8_u", [55122]), 0);

// memory_copy.wast:3631
assert_return(() => call($17, "load8_u", [55321]), 0);

// memory_copy.wast:3632
assert_return(() => call($17, "load8_u", [55520]), 0);

// memory_copy.wast:3633
assert_return(() => call($17, "load8_u", [55719]), 0);

// memory_copy.wast:3634
assert_return(() => call($17, "load8_u", [55918]), 0);

// memory_copy.wast:3635
assert_return(() => call($17, "load8_u", [56117]), 0);

// memory_copy.wast:3636
assert_return(() => call($17, "load8_u", [56316]), 0);

// memory_copy.wast:3637
assert_return(() => call($17, "load8_u", [56515]), 0);

// memory_copy.wast:3638
assert_return(() => call($17, "load8_u", [56714]), 0);

// memory_copy.wast:3639
assert_return(() => call($17, "load8_u", [56913]), 0);

// memory_copy.wast:3640
assert_return(() => call($17, "load8_u", [57112]), 0);

// memory_copy.wast:3641
assert_return(() => call($17, "load8_u", [57311]), 0);

// memory_copy.wast:3642
assert_return(() => call($17, "load8_u", [57510]), 0);

// memory_copy.wast:3643
assert_return(() => call($17, "load8_u", [57709]), 0);

// memory_copy.wast:3644
assert_return(() => call($17, "load8_u", [57908]), 0);

// memory_copy.wast:3645
assert_return(() => call($17, "load8_u", [58107]), 0);

// memory_copy.wast:3646
assert_return(() => call($17, "load8_u", [58306]), 0);

// memory_copy.wast:3647
assert_return(() => call($17, "load8_u", [58505]), 0);

// memory_copy.wast:3648
assert_return(() => call($17, "load8_u", [58704]), 0);

// memory_copy.wast:3649
assert_return(() => call($17, "load8_u", [58903]), 0);

// memory_copy.wast:3650
assert_return(() => call($17, "load8_u", [59102]), 0);

// memory_copy.wast:3651
assert_return(() => call($17, "load8_u", [59301]), 0);

// memory_copy.wast:3652
assert_return(() => call($17, "load8_u", [59500]), 0);

// memory_copy.wast:3653
assert_return(() => call($17, "load8_u", [59699]), 0);

// memory_copy.wast:3654
assert_return(() => call($17, "load8_u", [59898]), 0);

// memory_copy.wast:3655
assert_return(() => call($17, "load8_u", [60097]), 0);

// memory_copy.wast:3656
assert_return(() => call($17, "load8_u", [60296]), 0);

// memory_copy.wast:3657
assert_return(() => call($17, "load8_u", [60495]), 0);

// memory_copy.wast:3658
assert_return(() => call($17, "load8_u", [60694]), 0);

// memory_copy.wast:3659
assert_return(() => call($17, "load8_u", [60893]), 0);

// memory_copy.wast:3660
assert_return(() => call($17, "load8_u", [61092]), 0);

// memory_copy.wast:3661
assert_return(() => call($17, "load8_u", [61291]), 0);

// memory_copy.wast:3662
assert_return(() => call($17, "load8_u", [61490]), 0);

// memory_copy.wast:3663
assert_return(() => call($17, "load8_u", [61689]), 0);

// memory_copy.wast:3664
assert_return(() => call($17, "load8_u", [61888]), 0);

// memory_copy.wast:3665
assert_return(() => call($17, "load8_u", [62087]), 0);

// memory_copy.wast:3666
assert_return(() => call($17, "load8_u", [62286]), 0);

// memory_copy.wast:3667
assert_return(() => call($17, "load8_u", [62485]), 0);

// memory_copy.wast:3668
assert_return(() => call($17, "load8_u", [62684]), 0);

// memory_copy.wast:3669
assert_return(() => call($17, "load8_u", [62883]), 0);

// memory_copy.wast:3670
assert_return(() => call($17, "load8_u", [63082]), 0);

// memory_copy.wast:3671
assert_return(() => call($17, "load8_u", [63281]), 0);

// memory_copy.wast:3672
assert_return(() => call($17, "load8_u", [63480]), 0);

// memory_copy.wast:3673
assert_return(() => call($17, "load8_u", [63679]), 0);

// memory_copy.wast:3674
assert_return(() => call($17, "load8_u", [63878]), 0);

// memory_copy.wast:3675
assert_return(() => call($17, "load8_u", [64077]), 0);

// memory_copy.wast:3676
assert_return(() => call($17, "load8_u", [64276]), 0);

// memory_copy.wast:3677
assert_return(() => call($17, "load8_u", [64475]), 0);

// memory_copy.wast:3678
assert_return(() => call($17, "load8_u", [64674]), 0);

// memory_copy.wast:3679
assert_return(() => call($17, "load8_u", [64873]), 0);

// memory_copy.wast:3680
assert_return(() => call($17, "load8_u", [65072]), 0);

// memory_copy.wast:3681
assert_return(() => call($17, "load8_u", [65271]), 0);

// memory_copy.wast:3682
assert_return(() => call($17, "load8_u", [65470]), 0);

// memory_copy.wast:3683
assert_return(() => call($17, "load8_u", [65516]), 0);

// memory_copy.wast:3684
assert_return(() => call($17, "load8_u", [65517]), 1);

// memory_copy.wast:3685
assert_return(() => call($17, "load8_u", [65518]), 2);

// memory_copy.wast:3686
assert_return(() => call($17, "load8_u", [65519]), 3);

// memory_copy.wast:3687
assert_return(() => call($17, "load8_u", [65520]), 4);

// memory_copy.wast:3688
assert_return(() => call($17, "load8_u", [65521]), 5);

// memory_copy.wast:3689
assert_return(() => call($17, "load8_u", [65522]), 6);

// memory_copy.wast:3690
assert_return(() => call($17, "load8_u", [65523]), 7);

// memory_copy.wast:3691
assert_return(() => call($17, "load8_u", [65524]), 8);

// memory_copy.wast:3692
assert_return(() => call($17, "load8_u", [65525]), 9);

// memory_copy.wast:3693
assert_return(() => call($17, "load8_u", [65526]), 10);

// memory_copy.wast:3694
assert_return(() => call($17, "load8_u", [65527]), 11);

// memory_copy.wast:3695
assert_return(() => call($17, "load8_u", [65528]), 12);

// memory_copy.wast:3696
assert_return(() => call($17, "load8_u", [65529]), 13);

// memory_copy.wast:3697
assert_return(() => call($17, "load8_u", [65530]), 14);

// memory_copy.wast:3698
assert_return(() => call($17, "load8_u", [65531]), 15);

// memory_copy.wast:3699
assert_return(() => call($17, "load8_u", [65532]), 16);

// memory_copy.wast:3700
assert_return(() => call($17, "load8_u", [65533]), 17);

// memory_copy.wast:3701
assert_return(() => call($17, "load8_u", [65534]), 18);

// memory_copy.wast:3702
assert_return(() => call($17, "load8_u", [65535]), 19);

// memory_copy.wast:3704
let $18 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\xec\xff\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:3712
assert_trap(() => call($18, "run", [0, 65516, -4096]));

// memory_copy.wast:3715
assert_return(() => call($18, "load8_u", [0]), 0);

// memory_copy.wast:3716
assert_return(() => call($18, "load8_u", [1]), 1);

// memory_copy.wast:3717
assert_return(() => call($18, "load8_u", [2]), 2);

// memory_copy.wast:3718
assert_return(() => call($18, "load8_u", [3]), 3);

// memory_copy.wast:3719
assert_return(() => call($18, "load8_u", [4]), 4);

// memory_copy.wast:3720
assert_return(() => call($18, "load8_u", [5]), 5);

// memory_copy.wast:3721
assert_return(() => call($18, "load8_u", [6]), 6);

// memory_copy.wast:3722
assert_return(() => call($18, "load8_u", [7]), 7);

// memory_copy.wast:3723
assert_return(() => call($18, "load8_u", [8]), 8);

// memory_copy.wast:3724
assert_return(() => call($18, "load8_u", [9]), 9);

// memory_copy.wast:3725
assert_return(() => call($18, "load8_u", [10]), 10);

// memory_copy.wast:3726
assert_return(() => call($18, "load8_u", [11]), 11);

// memory_copy.wast:3727
assert_return(() => call($18, "load8_u", [12]), 12);

// memory_copy.wast:3728
assert_return(() => call($18, "load8_u", [13]), 13);

// memory_copy.wast:3729
assert_return(() => call($18, "load8_u", [14]), 14);

// memory_copy.wast:3730
assert_return(() => call($18, "load8_u", [15]), 15);

// memory_copy.wast:3731
assert_return(() => call($18, "load8_u", [16]), 16);

// memory_copy.wast:3732
assert_return(() => call($18, "load8_u", [17]), 17);

// memory_copy.wast:3733
assert_return(() => call($18, "load8_u", [18]), 18);

// memory_copy.wast:3734
assert_return(() => call($18, "load8_u", [19]), 19);

// memory_copy.wast:3735
assert_return(() => call($18, "load8_u", [218]), 0);

// memory_copy.wast:3736
assert_return(() => call($18, "load8_u", [417]), 0);

// memory_copy.wast:3737
assert_return(() => call($18, "load8_u", [616]), 0);

// memory_copy.wast:3738
assert_return(() => call($18, "load8_u", [815]), 0);

// memory_copy.wast:3739
assert_return(() => call($18, "load8_u", [1014]), 0);

// memory_copy.wast:3740
assert_return(() => call($18, "load8_u", [1213]), 0);

// memory_copy.wast:3741
assert_return(() => call($18, "load8_u", [1412]), 0);

// memory_copy.wast:3742
assert_return(() => call($18, "load8_u", [1611]), 0);

// memory_copy.wast:3743
assert_return(() => call($18, "load8_u", [1810]), 0);

// memory_copy.wast:3744
assert_return(() => call($18, "load8_u", [2009]), 0);

// memory_copy.wast:3745
assert_return(() => call($18, "load8_u", [2208]), 0);

// memory_copy.wast:3746
assert_return(() => call($18, "load8_u", [2407]), 0);

// memory_copy.wast:3747
assert_return(() => call($18, "load8_u", [2606]), 0);

// memory_copy.wast:3748
assert_return(() => call($18, "load8_u", [2805]), 0);

// memory_copy.wast:3749
assert_return(() => call($18, "load8_u", [3004]), 0);

// memory_copy.wast:3750
assert_return(() => call($18, "load8_u", [3203]), 0);

// memory_copy.wast:3751
assert_return(() => call($18, "load8_u", [3402]), 0);

// memory_copy.wast:3752
assert_return(() => call($18, "load8_u", [3601]), 0);

// memory_copy.wast:3753
assert_return(() => call($18, "load8_u", [3800]), 0);

// memory_copy.wast:3754
assert_return(() => call($18, "load8_u", [3999]), 0);

// memory_copy.wast:3755
assert_return(() => call($18, "load8_u", [4198]), 0);

// memory_copy.wast:3756
assert_return(() => call($18, "load8_u", [4397]), 0);

// memory_copy.wast:3757
assert_return(() => call($18, "load8_u", [4596]), 0);

// memory_copy.wast:3758
assert_return(() => call($18, "load8_u", [4795]), 0);

// memory_copy.wast:3759
assert_return(() => call($18, "load8_u", [4994]), 0);

// memory_copy.wast:3760
assert_return(() => call($18, "load8_u", [5193]), 0);

// memory_copy.wast:3761
assert_return(() => call($18, "load8_u", [5392]), 0);

// memory_copy.wast:3762
assert_return(() => call($18, "load8_u", [5591]), 0);

// memory_copy.wast:3763
assert_return(() => call($18, "load8_u", [5790]), 0);

// memory_copy.wast:3764
assert_return(() => call($18, "load8_u", [5989]), 0);

// memory_copy.wast:3765
assert_return(() => call($18, "load8_u", [6188]), 0);

// memory_copy.wast:3766
assert_return(() => call($18, "load8_u", [6387]), 0);

// memory_copy.wast:3767
assert_return(() => call($18, "load8_u", [6586]), 0);

// memory_copy.wast:3768
assert_return(() => call($18, "load8_u", [6785]), 0);

// memory_copy.wast:3769
assert_return(() => call($18, "load8_u", [6984]), 0);

// memory_copy.wast:3770
assert_return(() => call($18, "load8_u", [7183]), 0);

// memory_copy.wast:3771
assert_return(() => call($18, "load8_u", [7382]), 0);

// memory_copy.wast:3772
assert_return(() => call($18, "load8_u", [7581]), 0);

// memory_copy.wast:3773
assert_return(() => call($18, "load8_u", [7780]), 0);

// memory_copy.wast:3774
assert_return(() => call($18, "load8_u", [7979]), 0);

// memory_copy.wast:3775
assert_return(() => call($18, "load8_u", [8178]), 0);

// memory_copy.wast:3776
assert_return(() => call($18, "load8_u", [8377]), 0);

// memory_copy.wast:3777
assert_return(() => call($18, "load8_u", [8576]), 0);

// memory_copy.wast:3778
assert_return(() => call($18, "load8_u", [8775]), 0);

// memory_copy.wast:3779
assert_return(() => call($18, "load8_u", [8974]), 0);

// memory_copy.wast:3780
assert_return(() => call($18, "load8_u", [9173]), 0);

// memory_copy.wast:3781
assert_return(() => call($18, "load8_u", [9372]), 0);

// memory_copy.wast:3782
assert_return(() => call($18, "load8_u", [9571]), 0);

// memory_copy.wast:3783
assert_return(() => call($18, "load8_u", [9770]), 0);

// memory_copy.wast:3784
assert_return(() => call($18, "load8_u", [9969]), 0);

// memory_copy.wast:3785
assert_return(() => call($18, "load8_u", [10168]), 0);

// memory_copy.wast:3786
assert_return(() => call($18, "load8_u", [10367]), 0);

// memory_copy.wast:3787
assert_return(() => call($18, "load8_u", [10566]), 0);

// memory_copy.wast:3788
assert_return(() => call($18, "load8_u", [10765]), 0);

// memory_copy.wast:3789
assert_return(() => call($18, "load8_u", [10964]), 0);

// memory_copy.wast:3790
assert_return(() => call($18, "load8_u", [11163]), 0);

// memory_copy.wast:3791
assert_return(() => call($18, "load8_u", [11362]), 0);

// memory_copy.wast:3792
assert_return(() => call($18, "load8_u", [11561]), 0);

// memory_copy.wast:3793
assert_return(() => call($18, "load8_u", [11760]), 0);

// memory_copy.wast:3794
assert_return(() => call($18, "load8_u", [11959]), 0);

// memory_copy.wast:3795
assert_return(() => call($18, "load8_u", [12158]), 0);

// memory_copy.wast:3796
assert_return(() => call($18, "load8_u", [12357]), 0);

// memory_copy.wast:3797
assert_return(() => call($18, "load8_u", [12556]), 0);

// memory_copy.wast:3798
assert_return(() => call($18, "load8_u", [12755]), 0);

// memory_copy.wast:3799
assert_return(() => call($18, "load8_u", [12954]), 0);

// memory_copy.wast:3800
assert_return(() => call($18, "load8_u", [13153]), 0);

// memory_copy.wast:3801
assert_return(() => call($18, "load8_u", [13352]), 0);

// memory_copy.wast:3802
assert_return(() => call($18, "load8_u", [13551]), 0);

// memory_copy.wast:3803
assert_return(() => call($18, "load8_u", [13750]), 0);

// memory_copy.wast:3804
assert_return(() => call($18, "load8_u", [13949]), 0);

// memory_copy.wast:3805
assert_return(() => call($18, "load8_u", [14148]), 0);

// memory_copy.wast:3806
assert_return(() => call($18, "load8_u", [14347]), 0);

// memory_copy.wast:3807
assert_return(() => call($18, "load8_u", [14546]), 0);

// memory_copy.wast:3808
assert_return(() => call($18, "load8_u", [14745]), 0);

// memory_copy.wast:3809
assert_return(() => call($18, "load8_u", [14944]), 0);

// memory_copy.wast:3810
assert_return(() => call($18, "load8_u", [15143]), 0);

// memory_copy.wast:3811
assert_return(() => call($18, "load8_u", [15342]), 0);

// memory_copy.wast:3812
assert_return(() => call($18, "load8_u", [15541]), 0);

// memory_copy.wast:3813
assert_return(() => call($18, "load8_u", [15740]), 0);

// memory_copy.wast:3814
assert_return(() => call($18, "load8_u", [15939]), 0);

// memory_copy.wast:3815
assert_return(() => call($18, "load8_u", [16138]), 0);

// memory_copy.wast:3816
assert_return(() => call($18, "load8_u", [16337]), 0);

// memory_copy.wast:3817
assert_return(() => call($18, "load8_u", [16536]), 0);

// memory_copy.wast:3818
assert_return(() => call($18, "load8_u", [16735]), 0);

// memory_copy.wast:3819
assert_return(() => call($18, "load8_u", [16934]), 0);

// memory_copy.wast:3820
assert_return(() => call($18, "load8_u", [17133]), 0);

// memory_copy.wast:3821
assert_return(() => call($18, "load8_u", [17332]), 0);

// memory_copy.wast:3822
assert_return(() => call($18, "load8_u", [17531]), 0);

// memory_copy.wast:3823
assert_return(() => call($18, "load8_u", [17730]), 0);

// memory_copy.wast:3824
assert_return(() => call($18, "load8_u", [17929]), 0);

// memory_copy.wast:3825
assert_return(() => call($18, "load8_u", [18128]), 0);

// memory_copy.wast:3826
assert_return(() => call($18, "load8_u", [18327]), 0);

// memory_copy.wast:3827
assert_return(() => call($18, "load8_u", [18526]), 0);

// memory_copy.wast:3828
assert_return(() => call($18, "load8_u", [18725]), 0);

// memory_copy.wast:3829
assert_return(() => call($18, "load8_u", [18924]), 0);

// memory_copy.wast:3830
assert_return(() => call($18, "load8_u", [19123]), 0);

// memory_copy.wast:3831
assert_return(() => call($18, "load8_u", [19322]), 0);

// memory_copy.wast:3832
assert_return(() => call($18, "load8_u", [19521]), 0);

// memory_copy.wast:3833
assert_return(() => call($18, "load8_u", [19720]), 0);

// memory_copy.wast:3834
assert_return(() => call($18, "load8_u", [19919]), 0);

// memory_copy.wast:3835
assert_return(() => call($18, "load8_u", [20118]), 0);

// memory_copy.wast:3836
assert_return(() => call($18, "load8_u", [20317]), 0);

// memory_copy.wast:3837
assert_return(() => call($18, "load8_u", [20516]), 0);

// memory_copy.wast:3838
assert_return(() => call($18, "load8_u", [20715]), 0);

// memory_copy.wast:3839
assert_return(() => call($18, "load8_u", [20914]), 0);

// memory_copy.wast:3840
assert_return(() => call($18, "load8_u", [21113]), 0);

// memory_copy.wast:3841
assert_return(() => call($18, "load8_u", [21312]), 0);

// memory_copy.wast:3842
assert_return(() => call($18, "load8_u", [21511]), 0);

// memory_copy.wast:3843
assert_return(() => call($18, "load8_u", [21710]), 0);

// memory_copy.wast:3844
assert_return(() => call($18, "load8_u", [21909]), 0);

// memory_copy.wast:3845
assert_return(() => call($18, "load8_u", [22108]), 0);

// memory_copy.wast:3846
assert_return(() => call($18, "load8_u", [22307]), 0);

// memory_copy.wast:3847
assert_return(() => call($18, "load8_u", [22506]), 0);

// memory_copy.wast:3848
assert_return(() => call($18, "load8_u", [22705]), 0);

// memory_copy.wast:3849
assert_return(() => call($18, "load8_u", [22904]), 0);

// memory_copy.wast:3850
assert_return(() => call($18, "load8_u", [23103]), 0);

// memory_copy.wast:3851
assert_return(() => call($18, "load8_u", [23302]), 0);

// memory_copy.wast:3852
assert_return(() => call($18, "load8_u", [23501]), 0);

// memory_copy.wast:3853
assert_return(() => call($18, "load8_u", [23700]), 0);

// memory_copy.wast:3854
assert_return(() => call($18, "load8_u", [23899]), 0);

// memory_copy.wast:3855
assert_return(() => call($18, "load8_u", [24098]), 0);

// memory_copy.wast:3856
assert_return(() => call($18, "load8_u", [24297]), 0);

// memory_copy.wast:3857
assert_return(() => call($18, "load8_u", [24496]), 0);

// memory_copy.wast:3858
assert_return(() => call($18, "load8_u", [24695]), 0);

// memory_copy.wast:3859
assert_return(() => call($18, "load8_u", [24894]), 0);

// memory_copy.wast:3860
assert_return(() => call($18, "load8_u", [25093]), 0);

// memory_copy.wast:3861
assert_return(() => call($18, "load8_u", [25292]), 0);

// memory_copy.wast:3862
assert_return(() => call($18, "load8_u", [25491]), 0);

// memory_copy.wast:3863
assert_return(() => call($18, "load8_u", [25690]), 0);

// memory_copy.wast:3864
assert_return(() => call($18, "load8_u", [25889]), 0);

// memory_copy.wast:3865
assert_return(() => call($18, "load8_u", [26088]), 0);

// memory_copy.wast:3866
assert_return(() => call($18, "load8_u", [26287]), 0);

// memory_copy.wast:3867
assert_return(() => call($18, "load8_u", [26486]), 0);

// memory_copy.wast:3868
assert_return(() => call($18, "load8_u", [26685]), 0);

// memory_copy.wast:3869
assert_return(() => call($18, "load8_u", [26884]), 0);

// memory_copy.wast:3870
assert_return(() => call($18, "load8_u", [27083]), 0);

// memory_copy.wast:3871
assert_return(() => call($18, "load8_u", [27282]), 0);

// memory_copy.wast:3872
assert_return(() => call($18, "load8_u", [27481]), 0);

// memory_copy.wast:3873
assert_return(() => call($18, "load8_u", [27680]), 0);

// memory_copy.wast:3874
assert_return(() => call($18, "load8_u", [27879]), 0);

// memory_copy.wast:3875
assert_return(() => call($18, "load8_u", [28078]), 0);

// memory_copy.wast:3876
assert_return(() => call($18, "load8_u", [28277]), 0);

// memory_copy.wast:3877
assert_return(() => call($18, "load8_u", [28476]), 0);

// memory_copy.wast:3878
assert_return(() => call($18, "load8_u", [28675]), 0);

// memory_copy.wast:3879
assert_return(() => call($18, "load8_u", [28874]), 0);

// memory_copy.wast:3880
assert_return(() => call($18, "load8_u", [29073]), 0);

// memory_copy.wast:3881
assert_return(() => call($18, "load8_u", [29272]), 0);

// memory_copy.wast:3882
assert_return(() => call($18, "load8_u", [29471]), 0);

// memory_copy.wast:3883
assert_return(() => call($18, "load8_u", [29670]), 0);

// memory_copy.wast:3884
assert_return(() => call($18, "load8_u", [29869]), 0);

// memory_copy.wast:3885
assert_return(() => call($18, "load8_u", [30068]), 0);

// memory_copy.wast:3886
assert_return(() => call($18, "load8_u", [30267]), 0);

// memory_copy.wast:3887
assert_return(() => call($18, "load8_u", [30466]), 0);

// memory_copy.wast:3888
assert_return(() => call($18, "load8_u", [30665]), 0);

// memory_copy.wast:3889
assert_return(() => call($18, "load8_u", [30864]), 0);

// memory_copy.wast:3890
assert_return(() => call($18, "load8_u", [31063]), 0);

// memory_copy.wast:3891
assert_return(() => call($18, "load8_u", [31262]), 0);

// memory_copy.wast:3892
assert_return(() => call($18, "load8_u", [31461]), 0);

// memory_copy.wast:3893
assert_return(() => call($18, "load8_u", [31660]), 0);

// memory_copy.wast:3894
assert_return(() => call($18, "load8_u", [31859]), 0);

// memory_copy.wast:3895
assert_return(() => call($18, "load8_u", [32058]), 0);

// memory_copy.wast:3896
assert_return(() => call($18, "load8_u", [32257]), 0);

// memory_copy.wast:3897
assert_return(() => call($18, "load8_u", [32456]), 0);

// memory_copy.wast:3898
assert_return(() => call($18, "load8_u", [32655]), 0);

// memory_copy.wast:3899
assert_return(() => call($18, "load8_u", [32854]), 0);

// memory_copy.wast:3900
assert_return(() => call($18, "load8_u", [33053]), 0);

// memory_copy.wast:3901
assert_return(() => call($18, "load8_u", [33252]), 0);

// memory_copy.wast:3902
assert_return(() => call($18, "load8_u", [33451]), 0);

// memory_copy.wast:3903
assert_return(() => call($18, "load8_u", [33650]), 0);

// memory_copy.wast:3904
assert_return(() => call($18, "load8_u", [33849]), 0);

// memory_copy.wast:3905
assert_return(() => call($18, "load8_u", [34048]), 0);

// memory_copy.wast:3906
assert_return(() => call($18, "load8_u", [34247]), 0);

// memory_copy.wast:3907
assert_return(() => call($18, "load8_u", [34446]), 0);

// memory_copy.wast:3908
assert_return(() => call($18, "load8_u", [34645]), 0);

// memory_copy.wast:3909
assert_return(() => call($18, "load8_u", [34844]), 0);

// memory_copy.wast:3910
assert_return(() => call($18, "load8_u", [35043]), 0);

// memory_copy.wast:3911
assert_return(() => call($18, "load8_u", [35242]), 0);

// memory_copy.wast:3912
assert_return(() => call($18, "load8_u", [35441]), 0);

// memory_copy.wast:3913
assert_return(() => call($18, "load8_u", [35640]), 0);

// memory_copy.wast:3914
assert_return(() => call($18, "load8_u", [35839]), 0);

// memory_copy.wast:3915
assert_return(() => call($18, "load8_u", [36038]), 0);

// memory_copy.wast:3916
assert_return(() => call($18, "load8_u", [36237]), 0);

// memory_copy.wast:3917
assert_return(() => call($18, "load8_u", [36436]), 0);

// memory_copy.wast:3918
assert_return(() => call($18, "load8_u", [36635]), 0);

// memory_copy.wast:3919
assert_return(() => call($18, "load8_u", [36834]), 0);

// memory_copy.wast:3920
assert_return(() => call($18, "load8_u", [37033]), 0);

// memory_copy.wast:3921
assert_return(() => call($18, "load8_u", [37232]), 0);

// memory_copy.wast:3922
assert_return(() => call($18, "load8_u", [37431]), 0);

// memory_copy.wast:3923
assert_return(() => call($18, "load8_u", [37630]), 0);

// memory_copy.wast:3924
assert_return(() => call($18, "load8_u", [37829]), 0);

// memory_copy.wast:3925
assert_return(() => call($18, "load8_u", [38028]), 0);

// memory_copy.wast:3926
assert_return(() => call($18, "load8_u", [38227]), 0);

// memory_copy.wast:3927
assert_return(() => call($18, "load8_u", [38426]), 0);

// memory_copy.wast:3928
assert_return(() => call($18, "load8_u", [38625]), 0);

// memory_copy.wast:3929
assert_return(() => call($18, "load8_u", [38824]), 0);

// memory_copy.wast:3930
assert_return(() => call($18, "load8_u", [39023]), 0);

// memory_copy.wast:3931
assert_return(() => call($18, "load8_u", [39222]), 0);

// memory_copy.wast:3932
assert_return(() => call($18, "load8_u", [39421]), 0);

// memory_copy.wast:3933
assert_return(() => call($18, "load8_u", [39620]), 0);

// memory_copy.wast:3934
assert_return(() => call($18, "load8_u", [39819]), 0);

// memory_copy.wast:3935
assert_return(() => call($18, "load8_u", [40018]), 0);

// memory_copy.wast:3936
assert_return(() => call($18, "load8_u", [40217]), 0);

// memory_copy.wast:3937
assert_return(() => call($18, "load8_u", [40416]), 0);

// memory_copy.wast:3938
assert_return(() => call($18, "load8_u", [40615]), 0);

// memory_copy.wast:3939
assert_return(() => call($18, "load8_u", [40814]), 0);

// memory_copy.wast:3940
assert_return(() => call($18, "load8_u", [41013]), 0);

// memory_copy.wast:3941
assert_return(() => call($18, "load8_u", [41212]), 0);

// memory_copy.wast:3942
assert_return(() => call($18, "load8_u", [41411]), 0);

// memory_copy.wast:3943
assert_return(() => call($18, "load8_u", [41610]), 0);

// memory_copy.wast:3944
assert_return(() => call($18, "load8_u", [41809]), 0);

// memory_copy.wast:3945
assert_return(() => call($18, "load8_u", [42008]), 0);

// memory_copy.wast:3946
assert_return(() => call($18, "load8_u", [42207]), 0);

// memory_copy.wast:3947
assert_return(() => call($18, "load8_u", [42406]), 0);

// memory_copy.wast:3948
assert_return(() => call($18, "load8_u", [42605]), 0);

// memory_copy.wast:3949
assert_return(() => call($18, "load8_u", [42804]), 0);

// memory_copy.wast:3950
assert_return(() => call($18, "load8_u", [43003]), 0);

// memory_copy.wast:3951
assert_return(() => call($18, "load8_u", [43202]), 0);

// memory_copy.wast:3952
assert_return(() => call($18, "load8_u", [43401]), 0);

// memory_copy.wast:3953
assert_return(() => call($18, "load8_u", [43600]), 0);

// memory_copy.wast:3954
assert_return(() => call($18, "load8_u", [43799]), 0);

// memory_copy.wast:3955
assert_return(() => call($18, "load8_u", [43998]), 0);

// memory_copy.wast:3956
assert_return(() => call($18, "load8_u", [44197]), 0);

// memory_copy.wast:3957
assert_return(() => call($18, "load8_u", [44396]), 0);

// memory_copy.wast:3958
assert_return(() => call($18, "load8_u", [44595]), 0);

// memory_copy.wast:3959
assert_return(() => call($18, "load8_u", [44794]), 0);

// memory_copy.wast:3960
assert_return(() => call($18, "load8_u", [44993]), 0);

// memory_copy.wast:3961
assert_return(() => call($18, "load8_u", [45192]), 0);

// memory_copy.wast:3962
assert_return(() => call($18, "load8_u", [45391]), 0);

// memory_copy.wast:3963
assert_return(() => call($18, "load8_u", [45590]), 0);

// memory_copy.wast:3964
assert_return(() => call($18, "load8_u", [45789]), 0);

// memory_copy.wast:3965
assert_return(() => call($18, "load8_u", [45988]), 0);

// memory_copy.wast:3966
assert_return(() => call($18, "load8_u", [46187]), 0);

// memory_copy.wast:3967
assert_return(() => call($18, "load8_u", [46386]), 0);

// memory_copy.wast:3968
assert_return(() => call($18, "load8_u", [46585]), 0);

// memory_copy.wast:3969
assert_return(() => call($18, "load8_u", [46784]), 0);

// memory_copy.wast:3970
assert_return(() => call($18, "load8_u", [46983]), 0);

// memory_copy.wast:3971
assert_return(() => call($18, "load8_u", [47182]), 0);

// memory_copy.wast:3972
assert_return(() => call($18, "load8_u", [47381]), 0);

// memory_copy.wast:3973
assert_return(() => call($18, "load8_u", [47580]), 0);

// memory_copy.wast:3974
assert_return(() => call($18, "load8_u", [47779]), 0);

// memory_copy.wast:3975
assert_return(() => call($18, "load8_u", [47978]), 0);

// memory_copy.wast:3976
assert_return(() => call($18, "load8_u", [48177]), 0);

// memory_copy.wast:3977
assert_return(() => call($18, "load8_u", [48376]), 0);

// memory_copy.wast:3978
assert_return(() => call($18, "load8_u", [48575]), 0);

// memory_copy.wast:3979
assert_return(() => call($18, "load8_u", [48774]), 0);

// memory_copy.wast:3980
assert_return(() => call($18, "load8_u", [48973]), 0);

// memory_copy.wast:3981
assert_return(() => call($18, "load8_u", [49172]), 0);

// memory_copy.wast:3982
assert_return(() => call($18, "load8_u", [49371]), 0);

// memory_copy.wast:3983
assert_return(() => call($18, "load8_u", [49570]), 0);

// memory_copy.wast:3984
assert_return(() => call($18, "load8_u", [49769]), 0);

// memory_copy.wast:3985
assert_return(() => call($18, "load8_u", [49968]), 0);

// memory_copy.wast:3986
assert_return(() => call($18, "load8_u", [50167]), 0);

// memory_copy.wast:3987
assert_return(() => call($18, "load8_u", [50366]), 0);

// memory_copy.wast:3988
assert_return(() => call($18, "load8_u", [50565]), 0);

// memory_copy.wast:3989
assert_return(() => call($18, "load8_u", [50764]), 0);

// memory_copy.wast:3990
assert_return(() => call($18, "load8_u", [50963]), 0);

// memory_copy.wast:3991
assert_return(() => call($18, "load8_u", [51162]), 0);

// memory_copy.wast:3992
assert_return(() => call($18, "load8_u", [51361]), 0);

// memory_copy.wast:3993
assert_return(() => call($18, "load8_u", [51560]), 0);

// memory_copy.wast:3994
assert_return(() => call($18, "load8_u", [51759]), 0);

// memory_copy.wast:3995
assert_return(() => call($18, "load8_u", [51958]), 0);

// memory_copy.wast:3996
assert_return(() => call($18, "load8_u", [52157]), 0);

// memory_copy.wast:3997
assert_return(() => call($18, "load8_u", [52356]), 0);

// memory_copy.wast:3998
assert_return(() => call($18, "load8_u", [52555]), 0);

// memory_copy.wast:3999
assert_return(() => call($18, "load8_u", [52754]), 0);

// memory_copy.wast:4000
assert_return(() => call($18, "load8_u", [52953]), 0);

// memory_copy.wast:4001
assert_return(() => call($18, "load8_u", [53152]), 0);

// memory_copy.wast:4002
assert_return(() => call($18, "load8_u", [53351]), 0);

// memory_copy.wast:4003
assert_return(() => call($18, "load8_u", [53550]), 0);

// memory_copy.wast:4004
assert_return(() => call($18, "load8_u", [53749]), 0);

// memory_copy.wast:4005
assert_return(() => call($18, "load8_u", [53948]), 0);

// memory_copy.wast:4006
assert_return(() => call($18, "load8_u", [54147]), 0);

// memory_copy.wast:4007
assert_return(() => call($18, "load8_u", [54346]), 0);

// memory_copy.wast:4008
assert_return(() => call($18, "load8_u", [54545]), 0);

// memory_copy.wast:4009
assert_return(() => call($18, "load8_u", [54744]), 0);

// memory_copy.wast:4010
assert_return(() => call($18, "load8_u", [54943]), 0);

// memory_copy.wast:4011
assert_return(() => call($18, "load8_u", [55142]), 0);

// memory_copy.wast:4012
assert_return(() => call($18, "load8_u", [55341]), 0);

// memory_copy.wast:4013
assert_return(() => call($18, "load8_u", [55540]), 0);

// memory_copy.wast:4014
assert_return(() => call($18, "load8_u", [55739]), 0);

// memory_copy.wast:4015
assert_return(() => call($18, "load8_u", [55938]), 0);

// memory_copy.wast:4016
assert_return(() => call($18, "load8_u", [56137]), 0);

// memory_copy.wast:4017
assert_return(() => call($18, "load8_u", [56336]), 0);

// memory_copy.wast:4018
assert_return(() => call($18, "load8_u", [56535]), 0);

// memory_copy.wast:4019
assert_return(() => call($18, "load8_u", [56734]), 0);

// memory_copy.wast:4020
assert_return(() => call($18, "load8_u", [56933]), 0);

// memory_copy.wast:4021
assert_return(() => call($18, "load8_u", [57132]), 0);

// memory_copy.wast:4022
assert_return(() => call($18, "load8_u", [57331]), 0);

// memory_copy.wast:4023
assert_return(() => call($18, "load8_u", [57530]), 0);

// memory_copy.wast:4024
assert_return(() => call($18, "load8_u", [57729]), 0);

// memory_copy.wast:4025
assert_return(() => call($18, "load8_u", [57928]), 0);

// memory_copy.wast:4026
assert_return(() => call($18, "load8_u", [58127]), 0);

// memory_copy.wast:4027
assert_return(() => call($18, "load8_u", [58326]), 0);

// memory_copy.wast:4028
assert_return(() => call($18, "load8_u", [58525]), 0);

// memory_copy.wast:4029
assert_return(() => call($18, "load8_u", [58724]), 0);

// memory_copy.wast:4030
assert_return(() => call($18, "load8_u", [58923]), 0);

// memory_copy.wast:4031
assert_return(() => call($18, "load8_u", [59122]), 0);

// memory_copy.wast:4032
assert_return(() => call($18, "load8_u", [59321]), 0);

// memory_copy.wast:4033
assert_return(() => call($18, "load8_u", [59520]), 0);

// memory_copy.wast:4034
assert_return(() => call($18, "load8_u", [59719]), 0);

// memory_copy.wast:4035
assert_return(() => call($18, "load8_u", [59918]), 0);

// memory_copy.wast:4036
assert_return(() => call($18, "load8_u", [60117]), 0);

// memory_copy.wast:4037
assert_return(() => call($18, "load8_u", [60316]), 0);

// memory_copy.wast:4038
assert_return(() => call($18, "load8_u", [60515]), 0);

// memory_copy.wast:4039
assert_return(() => call($18, "load8_u", [60714]), 0);

// memory_copy.wast:4040
assert_return(() => call($18, "load8_u", [60913]), 0);

// memory_copy.wast:4041
assert_return(() => call($18, "load8_u", [61112]), 0);

// memory_copy.wast:4042
assert_return(() => call($18, "load8_u", [61311]), 0);

// memory_copy.wast:4043
assert_return(() => call($18, "load8_u", [61510]), 0);

// memory_copy.wast:4044
assert_return(() => call($18, "load8_u", [61709]), 0);

// memory_copy.wast:4045
assert_return(() => call($18, "load8_u", [61908]), 0);

// memory_copy.wast:4046
assert_return(() => call($18, "load8_u", [62107]), 0);

// memory_copy.wast:4047
assert_return(() => call($18, "load8_u", [62306]), 0);

// memory_copy.wast:4048
assert_return(() => call($18, "load8_u", [62505]), 0);

// memory_copy.wast:4049
assert_return(() => call($18, "load8_u", [62704]), 0);

// memory_copy.wast:4050
assert_return(() => call($18, "load8_u", [62903]), 0);

// memory_copy.wast:4051
assert_return(() => call($18, "load8_u", [63102]), 0);

// memory_copy.wast:4052
assert_return(() => call($18, "load8_u", [63301]), 0);

// memory_copy.wast:4053
assert_return(() => call($18, "load8_u", [63500]), 0);

// memory_copy.wast:4054
assert_return(() => call($18, "load8_u", [63699]), 0);

// memory_copy.wast:4055
assert_return(() => call($18, "load8_u", [63898]), 0);

// memory_copy.wast:4056
assert_return(() => call($18, "load8_u", [64097]), 0);

// memory_copy.wast:4057
assert_return(() => call($18, "load8_u", [64296]), 0);

// memory_copy.wast:4058
assert_return(() => call($18, "load8_u", [64495]), 0);

// memory_copy.wast:4059
assert_return(() => call($18, "load8_u", [64694]), 0);

// memory_copy.wast:4060
assert_return(() => call($18, "load8_u", [64893]), 0);

// memory_copy.wast:4061
assert_return(() => call($18, "load8_u", [65092]), 0);

// memory_copy.wast:4062
assert_return(() => call($18, "load8_u", [65291]), 0);

// memory_copy.wast:4063
assert_return(() => call($18, "load8_u", [65490]), 0);

// memory_copy.wast:4064
assert_return(() => call($18, "load8_u", [65516]), 0);

// memory_copy.wast:4065
assert_return(() => call($18, "load8_u", [65517]), 1);

// memory_copy.wast:4066
assert_return(() => call($18, "load8_u", [65518]), 2);

// memory_copy.wast:4067
assert_return(() => call($18, "load8_u", [65519]), 3);

// memory_copy.wast:4068
assert_return(() => call($18, "load8_u", [65520]), 4);

// memory_copy.wast:4069
assert_return(() => call($18, "load8_u", [65521]), 5);

// memory_copy.wast:4070
assert_return(() => call($18, "load8_u", [65522]), 6);

// memory_copy.wast:4071
assert_return(() => call($18, "load8_u", [65523]), 7);

// memory_copy.wast:4072
assert_return(() => call($18, "load8_u", [65524]), 8);

// memory_copy.wast:4073
assert_return(() => call($18, "load8_u", [65525]), 9);

// memory_copy.wast:4074
assert_return(() => call($18, "load8_u", [65526]), 10);

// memory_copy.wast:4075
assert_return(() => call($18, "load8_u", [65527]), 11);

// memory_copy.wast:4076
assert_return(() => call($18, "load8_u", [65528]), 12);

// memory_copy.wast:4077
assert_return(() => call($18, "load8_u", [65529]), 13);

// memory_copy.wast:4078
assert_return(() => call($18, "load8_u", [65530]), 14);

// memory_copy.wast:4079
assert_return(() => call($18, "load8_u", [65531]), 15);

// memory_copy.wast:4080
assert_return(() => call($18, "load8_u", [65532]), 16);

// memory_copy.wast:4081
assert_return(() => call($18, "load8_u", [65533]), 17);

// memory_copy.wast:4082
assert_return(() => call($18, "load8_u", [65534]), 18);

// memory_copy.wast:4083
assert_return(() => call($18, "load8_u", [65535]), 19);

// memory_copy.wast:4085
let $19 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x97\x80\x80\x80\x00\x03\x03\x6d\x65\x6d\x02\x00\x03\x72\x75\x6e\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x9c\x80\x80\x80\x00\x01\x00\x41\x80\xe0\x03\x0b\x14\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13");

// memory_copy.wast:4093
assert_trap(() => call($19, "run", [65516, 61440, -256]));

// memory_copy.wast:4096
assert_return(() => call($19, "load8_u", [198]), 0);

// memory_copy.wast:4097
assert_return(() => call($19, "load8_u", [397]), 0);

// memory_copy.wast:4098
assert_return(() => call($19, "load8_u", [596]), 0);

// memory_copy.wast:4099
assert_return(() => call($19, "load8_u", [795]), 0);

// memory_copy.wast:4100
assert_return(() => call($19, "load8_u", [994]), 0);

// memory_copy.wast:4101
assert_return(() => call($19, "load8_u", [1193]), 0);

// memory_copy.wast:4102
assert_return(() => call($19, "load8_u", [1392]), 0);

// memory_copy.wast:4103
assert_return(() => call($19, "load8_u", [1591]), 0);

// memory_copy.wast:4104
assert_return(() => call($19, "load8_u", [1790]), 0);

// memory_copy.wast:4105
assert_return(() => call($19, "load8_u", [1989]), 0);

// memory_copy.wast:4106
assert_return(() => call($19, "load8_u", [2188]), 0);

// memory_copy.wast:4107
assert_return(() => call($19, "load8_u", [2387]), 0);

// memory_copy.wast:4108
assert_return(() => call($19, "load8_u", [2586]), 0);

// memory_copy.wast:4109
assert_return(() => call($19, "load8_u", [2785]), 0);

// memory_copy.wast:4110
assert_return(() => call($19, "load8_u", [2984]), 0);

// memory_copy.wast:4111
assert_return(() => call($19, "load8_u", [3183]), 0);

// memory_copy.wast:4112
assert_return(() => call($19, "load8_u", [3382]), 0);

// memory_copy.wast:4113
assert_return(() => call($19, "load8_u", [3581]), 0);

// memory_copy.wast:4114
assert_return(() => call($19, "load8_u", [3780]), 0);

// memory_copy.wast:4115
assert_return(() => call($19, "load8_u", [3979]), 0);

// memory_copy.wast:4116
assert_return(() => call($19, "load8_u", [4178]), 0);

// memory_copy.wast:4117
assert_return(() => call($19, "load8_u", [4377]), 0);

// memory_copy.wast:4118
assert_return(() => call($19, "load8_u", [4576]), 0);

// memory_copy.wast:4119
assert_return(() => call($19, "load8_u", [4775]), 0);

// memory_copy.wast:4120
assert_return(() => call($19, "load8_u", [4974]), 0);

// memory_copy.wast:4121
assert_return(() => call($19, "load8_u", [5173]), 0);

// memory_copy.wast:4122
assert_return(() => call($19, "load8_u", [5372]), 0);

// memory_copy.wast:4123
assert_return(() => call($19, "load8_u", [5571]), 0);

// memory_copy.wast:4124
assert_return(() => call($19, "load8_u", [5770]), 0);

// memory_copy.wast:4125
assert_return(() => call($19, "load8_u", [5969]), 0);

// memory_copy.wast:4126
assert_return(() => call($19, "load8_u", [6168]), 0);

// memory_copy.wast:4127
assert_return(() => call($19, "load8_u", [6367]), 0);

// memory_copy.wast:4128
assert_return(() => call($19, "load8_u", [6566]), 0);

// memory_copy.wast:4129
assert_return(() => call($19, "load8_u", [6765]), 0);

// memory_copy.wast:4130
assert_return(() => call($19, "load8_u", [6964]), 0);

// memory_copy.wast:4131
assert_return(() => call($19, "load8_u", [7163]), 0);

// memory_copy.wast:4132
assert_return(() => call($19, "load8_u", [7362]), 0);

// memory_copy.wast:4133
assert_return(() => call($19, "load8_u", [7561]), 0);

// memory_copy.wast:4134
assert_return(() => call($19, "load8_u", [7760]), 0);

// memory_copy.wast:4135
assert_return(() => call($19, "load8_u", [7959]), 0);

// memory_copy.wast:4136
assert_return(() => call($19, "load8_u", [8158]), 0);

// memory_copy.wast:4137
assert_return(() => call($19, "load8_u", [8357]), 0);

// memory_copy.wast:4138
assert_return(() => call($19, "load8_u", [8556]), 0);

// memory_copy.wast:4139
assert_return(() => call($19, "load8_u", [8755]), 0);

// memory_copy.wast:4140
assert_return(() => call($19, "load8_u", [8954]), 0);

// memory_copy.wast:4141
assert_return(() => call($19, "load8_u", [9153]), 0);

// memory_copy.wast:4142
assert_return(() => call($19, "load8_u", [9352]), 0);

// memory_copy.wast:4143
assert_return(() => call($19, "load8_u", [9551]), 0);

// memory_copy.wast:4144
assert_return(() => call($19, "load8_u", [9750]), 0);

// memory_copy.wast:4145
assert_return(() => call($19, "load8_u", [9949]), 0);

// memory_copy.wast:4146
assert_return(() => call($19, "load8_u", [10148]), 0);

// memory_copy.wast:4147
assert_return(() => call($19, "load8_u", [10347]), 0);

// memory_copy.wast:4148
assert_return(() => call($19, "load8_u", [10546]), 0);

// memory_copy.wast:4149
assert_return(() => call($19, "load8_u", [10745]), 0);

// memory_copy.wast:4150
assert_return(() => call($19, "load8_u", [10944]), 0);

// memory_copy.wast:4151
assert_return(() => call($19, "load8_u", [11143]), 0);

// memory_copy.wast:4152
assert_return(() => call($19, "load8_u", [11342]), 0);

// memory_copy.wast:4153
assert_return(() => call($19, "load8_u", [11541]), 0);

// memory_copy.wast:4154
assert_return(() => call($19, "load8_u", [11740]), 0);

// memory_copy.wast:4155
assert_return(() => call($19, "load8_u", [11939]), 0);

// memory_copy.wast:4156
assert_return(() => call($19, "load8_u", [12138]), 0);

// memory_copy.wast:4157
assert_return(() => call($19, "load8_u", [12337]), 0);

// memory_copy.wast:4158
assert_return(() => call($19, "load8_u", [12536]), 0);

// memory_copy.wast:4159
assert_return(() => call($19, "load8_u", [12735]), 0);

// memory_copy.wast:4160
assert_return(() => call($19, "load8_u", [12934]), 0);

// memory_copy.wast:4161
assert_return(() => call($19, "load8_u", [13133]), 0);

// memory_copy.wast:4162
assert_return(() => call($19, "load8_u", [13332]), 0);

// memory_copy.wast:4163
assert_return(() => call($19, "load8_u", [13531]), 0);

// memory_copy.wast:4164
assert_return(() => call($19, "load8_u", [13730]), 0);

// memory_copy.wast:4165
assert_return(() => call($19, "load8_u", [13929]), 0);

// memory_copy.wast:4166
assert_return(() => call($19, "load8_u", [14128]), 0);

// memory_copy.wast:4167
assert_return(() => call($19, "load8_u", [14327]), 0);

// memory_copy.wast:4168
assert_return(() => call($19, "load8_u", [14526]), 0);

// memory_copy.wast:4169
assert_return(() => call($19, "load8_u", [14725]), 0);

// memory_copy.wast:4170
assert_return(() => call($19, "load8_u", [14924]), 0);

// memory_copy.wast:4171
assert_return(() => call($19, "load8_u", [15123]), 0);

// memory_copy.wast:4172
assert_return(() => call($19, "load8_u", [15322]), 0);

// memory_copy.wast:4173
assert_return(() => call($19, "load8_u", [15521]), 0);

// memory_copy.wast:4174
assert_return(() => call($19, "load8_u", [15720]), 0);

// memory_copy.wast:4175
assert_return(() => call($19, "load8_u", [15919]), 0);

// memory_copy.wast:4176
assert_return(() => call($19, "load8_u", [16118]), 0);

// memory_copy.wast:4177
assert_return(() => call($19, "load8_u", [16317]), 0);

// memory_copy.wast:4178
assert_return(() => call($19, "load8_u", [16516]), 0);

// memory_copy.wast:4179
assert_return(() => call($19, "load8_u", [16715]), 0);

// memory_copy.wast:4180
assert_return(() => call($19, "load8_u", [16914]), 0);

// memory_copy.wast:4181
assert_return(() => call($19, "load8_u", [17113]), 0);

// memory_copy.wast:4182
assert_return(() => call($19, "load8_u", [17312]), 0);

// memory_copy.wast:4183
assert_return(() => call($19, "load8_u", [17511]), 0);

// memory_copy.wast:4184
assert_return(() => call($19, "load8_u", [17710]), 0);

// memory_copy.wast:4185
assert_return(() => call($19, "load8_u", [17909]), 0);

// memory_copy.wast:4186
assert_return(() => call($19, "load8_u", [18108]), 0);

// memory_copy.wast:4187
assert_return(() => call($19, "load8_u", [18307]), 0);

// memory_copy.wast:4188
assert_return(() => call($19, "load8_u", [18506]), 0);

// memory_copy.wast:4189
assert_return(() => call($19, "load8_u", [18705]), 0);

// memory_copy.wast:4190
assert_return(() => call($19, "load8_u", [18904]), 0);

// memory_copy.wast:4191
assert_return(() => call($19, "load8_u", [19103]), 0);

// memory_copy.wast:4192
assert_return(() => call($19, "load8_u", [19302]), 0);

// memory_copy.wast:4193
assert_return(() => call($19, "load8_u", [19501]), 0);

// memory_copy.wast:4194
assert_return(() => call($19, "load8_u", [19700]), 0);

// memory_copy.wast:4195
assert_return(() => call($19, "load8_u", [19899]), 0);

// memory_copy.wast:4196
assert_return(() => call($19, "load8_u", [20098]), 0);

// memory_copy.wast:4197
assert_return(() => call($19, "load8_u", [20297]), 0);

// memory_copy.wast:4198
assert_return(() => call($19, "load8_u", [20496]), 0);

// memory_copy.wast:4199
assert_return(() => call($19, "load8_u", [20695]), 0);

// memory_copy.wast:4200
assert_return(() => call($19, "load8_u", [20894]), 0);

// memory_copy.wast:4201
assert_return(() => call($19, "load8_u", [21093]), 0);

// memory_copy.wast:4202
assert_return(() => call($19, "load8_u", [21292]), 0);

// memory_copy.wast:4203
assert_return(() => call($19, "load8_u", [21491]), 0);

// memory_copy.wast:4204
assert_return(() => call($19, "load8_u", [21690]), 0);

// memory_copy.wast:4205
assert_return(() => call($19, "load8_u", [21889]), 0);

// memory_copy.wast:4206
assert_return(() => call($19, "load8_u", [22088]), 0);

// memory_copy.wast:4207
assert_return(() => call($19, "load8_u", [22287]), 0);

// memory_copy.wast:4208
assert_return(() => call($19, "load8_u", [22486]), 0);

// memory_copy.wast:4209
assert_return(() => call($19, "load8_u", [22685]), 0);

// memory_copy.wast:4210
assert_return(() => call($19, "load8_u", [22884]), 0);

// memory_copy.wast:4211
assert_return(() => call($19, "load8_u", [23083]), 0);

// memory_copy.wast:4212
assert_return(() => call($19, "load8_u", [23282]), 0);

// memory_copy.wast:4213
assert_return(() => call($19, "load8_u", [23481]), 0);

// memory_copy.wast:4214
assert_return(() => call($19, "load8_u", [23680]), 0);

// memory_copy.wast:4215
assert_return(() => call($19, "load8_u", [23879]), 0);

// memory_copy.wast:4216
assert_return(() => call($19, "load8_u", [24078]), 0);

// memory_copy.wast:4217
assert_return(() => call($19, "load8_u", [24277]), 0);

// memory_copy.wast:4218
assert_return(() => call($19, "load8_u", [24476]), 0);

// memory_copy.wast:4219
assert_return(() => call($19, "load8_u", [24675]), 0);

// memory_copy.wast:4220
assert_return(() => call($19, "load8_u", [24874]), 0);

// memory_copy.wast:4221
assert_return(() => call($19, "load8_u", [25073]), 0);

// memory_copy.wast:4222
assert_return(() => call($19, "load8_u", [25272]), 0);

// memory_copy.wast:4223
assert_return(() => call($19, "load8_u", [25471]), 0);

// memory_copy.wast:4224
assert_return(() => call($19, "load8_u", [25670]), 0);

// memory_copy.wast:4225
assert_return(() => call($19, "load8_u", [25869]), 0);

// memory_copy.wast:4226
assert_return(() => call($19, "load8_u", [26068]), 0);

// memory_copy.wast:4227
assert_return(() => call($19, "load8_u", [26267]), 0);

// memory_copy.wast:4228
assert_return(() => call($19, "load8_u", [26466]), 0);

// memory_copy.wast:4229
assert_return(() => call($19, "load8_u", [26665]), 0);

// memory_copy.wast:4230
assert_return(() => call($19, "load8_u", [26864]), 0);

// memory_copy.wast:4231
assert_return(() => call($19, "load8_u", [27063]), 0);

// memory_copy.wast:4232
assert_return(() => call($19, "load8_u", [27262]), 0);

// memory_copy.wast:4233
assert_return(() => call($19, "load8_u", [27461]), 0);

// memory_copy.wast:4234
assert_return(() => call($19, "load8_u", [27660]), 0);

// memory_copy.wast:4235
assert_return(() => call($19, "load8_u", [27859]), 0);

// memory_copy.wast:4236
assert_return(() => call($19, "load8_u", [28058]), 0);

// memory_copy.wast:4237
assert_return(() => call($19, "load8_u", [28257]), 0);

// memory_copy.wast:4238
assert_return(() => call($19, "load8_u", [28456]), 0);

// memory_copy.wast:4239
assert_return(() => call($19, "load8_u", [28655]), 0);

// memory_copy.wast:4240
assert_return(() => call($19, "load8_u", [28854]), 0);

// memory_copy.wast:4241
assert_return(() => call($19, "load8_u", [29053]), 0);

// memory_copy.wast:4242
assert_return(() => call($19, "load8_u", [29252]), 0);

// memory_copy.wast:4243
assert_return(() => call($19, "load8_u", [29451]), 0);

// memory_copy.wast:4244
assert_return(() => call($19, "load8_u", [29650]), 0);

// memory_copy.wast:4245
assert_return(() => call($19, "load8_u", [29849]), 0);

// memory_copy.wast:4246
assert_return(() => call($19, "load8_u", [30048]), 0);

// memory_copy.wast:4247
assert_return(() => call($19, "load8_u", [30247]), 0);

// memory_copy.wast:4248
assert_return(() => call($19, "load8_u", [30446]), 0);

// memory_copy.wast:4249
assert_return(() => call($19, "load8_u", [30645]), 0);

// memory_copy.wast:4250
assert_return(() => call($19, "load8_u", [30844]), 0);

// memory_copy.wast:4251
assert_return(() => call($19, "load8_u", [31043]), 0);

// memory_copy.wast:4252
assert_return(() => call($19, "load8_u", [31242]), 0);

// memory_copy.wast:4253
assert_return(() => call($19, "load8_u", [31441]), 0);

// memory_copy.wast:4254
assert_return(() => call($19, "load8_u", [31640]), 0);

// memory_copy.wast:4255
assert_return(() => call($19, "load8_u", [31839]), 0);

// memory_copy.wast:4256
assert_return(() => call($19, "load8_u", [32038]), 0);

// memory_copy.wast:4257
assert_return(() => call($19, "load8_u", [32237]), 0);

// memory_copy.wast:4258
assert_return(() => call($19, "load8_u", [32436]), 0);

// memory_copy.wast:4259
assert_return(() => call($19, "load8_u", [32635]), 0);

// memory_copy.wast:4260
assert_return(() => call($19, "load8_u", [32834]), 0);

// memory_copy.wast:4261
assert_return(() => call($19, "load8_u", [33033]), 0);

// memory_copy.wast:4262
assert_return(() => call($19, "load8_u", [33232]), 0);

// memory_copy.wast:4263
assert_return(() => call($19, "load8_u", [33431]), 0);

// memory_copy.wast:4264
assert_return(() => call($19, "load8_u", [33630]), 0);

// memory_copy.wast:4265
assert_return(() => call($19, "load8_u", [33829]), 0);

// memory_copy.wast:4266
assert_return(() => call($19, "load8_u", [34028]), 0);

// memory_copy.wast:4267
assert_return(() => call($19, "load8_u", [34227]), 0);

// memory_copy.wast:4268
assert_return(() => call($19, "load8_u", [34426]), 0);

// memory_copy.wast:4269
assert_return(() => call($19, "load8_u", [34625]), 0);

// memory_copy.wast:4270
assert_return(() => call($19, "load8_u", [34824]), 0);

// memory_copy.wast:4271
assert_return(() => call($19, "load8_u", [35023]), 0);

// memory_copy.wast:4272
assert_return(() => call($19, "load8_u", [35222]), 0);

// memory_copy.wast:4273
assert_return(() => call($19, "load8_u", [35421]), 0);

// memory_copy.wast:4274
assert_return(() => call($19, "load8_u", [35620]), 0);

// memory_copy.wast:4275
assert_return(() => call($19, "load8_u", [35819]), 0);

// memory_copy.wast:4276
assert_return(() => call($19, "load8_u", [36018]), 0);

// memory_copy.wast:4277
assert_return(() => call($19, "load8_u", [36217]), 0);

// memory_copy.wast:4278
assert_return(() => call($19, "load8_u", [36416]), 0);

// memory_copy.wast:4279
assert_return(() => call($19, "load8_u", [36615]), 0);

// memory_copy.wast:4280
assert_return(() => call($19, "load8_u", [36814]), 0);

// memory_copy.wast:4281
assert_return(() => call($19, "load8_u", [37013]), 0);

// memory_copy.wast:4282
assert_return(() => call($19, "load8_u", [37212]), 0);

// memory_copy.wast:4283
assert_return(() => call($19, "load8_u", [37411]), 0);

// memory_copy.wast:4284
assert_return(() => call($19, "load8_u", [37610]), 0);

// memory_copy.wast:4285
assert_return(() => call($19, "load8_u", [37809]), 0);

// memory_copy.wast:4286
assert_return(() => call($19, "load8_u", [38008]), 0);

// memory_copy.wast:4287
assert_return(() => call($19, "load8_u", [38207]), 0);

// memory_copy.wast:4288
assert_return(() => call($19, "load8_u", [38406]), 0);

// memory_copy.wast:4289
assert_return(() => call($19, "load8_u", [38605]), 0);

// memory_copy.wast:4290
assert_return(() => call($19, "load8_u", [38804]), 0);

// memory_copy.wast:4291
assert_return(() => call($19, "load8_u", [39003]), 0);

// memory_copy.wast:4292
assert_return(() => call($19, "load8_u", [39202]), 0);

// memory_copy.wast:4293
assert_return(() => call($19, "load8_u", [39401]), 0);

// memory_copy.wast:4294
assert_return(() => call($19, "load8_u", [39600]), 0);

// memory_copy.wast:4295
assert_return(() => call($19, "load8_u", [39799]), 0);

// memory_copy.wast:4296
assert_return(() => call($19, "load8_u", [39998]), 0);

// memory_copy.wast:4297
assert_return(() => call($19, "load8_u", [40197]), 0);

// memory_copy.wast:4298
assert_return(() => call($19, "load8_u", [40396]), 0);

// memory_copy.wast:4299
assert_return(() => call($19, "load8_u", [40595]), 0);

// memory_copy.wast:4300
assert_return(() => call($19, "load8_u", [40794]), 0);

// memory_copy.wast:4301
assert_return(() => call($19, "load8_u", [40993]), 0);

// memory_copy.wast:4302
assert_return(() => call($19, "load8_u", [41192]), 0);

// memory_copy.wast:4303
assert_return(() => call($19, "load8_u", [41391]), 0);

// memory_copy.wast:4304
assert_return(() => call($19, "load8_u", [41590]), 0);

// memory_copy.wast:4305
assert_return(() => call($19, "load8_u", [41789]), 0);

// memory_copy.wast:4306
assert_return(() => call($19, "load8_u", [41988]), 0);

// memory_copy.wast:4307
assert_return(() => call($19, "load8_u", [42187]), 0);

// memory_copy.wast:4308
assert_return(() => call($19, "load8_u", [42386]), 0);

// memory_copy.wast:4309
assert_return(() => call($19, "load8_u", [42585]), 0);

// memory_copy.wast:4310
assert_return(() => call($19, "load8_u", [42784]), 0);

// memory_copy.wast:4311
assert_return(() => call($19, "load8_u", [42983]), 0);

// memory_copy.wast:4312
assert_return(() => call($19, "load8_u", [43182]), 0);

// memory_copy.wast:4313
assert_return(() => call($19, "load8_u", [43381]), 0);

// memory_copy.wast:4314
assert_return(() => call($19, "load8_u", [43580]), 0);

// memory_copy.wast:4315
assert_return(() => call($19, "load8_u", [43779]), 0);

// memory_copy.wast:4316
assert_return(() => call($19, "load8_u", [43978]), 0);

// memory_copy.wast:4317
assert_return(() => call($19, "load8_u", [44177]), 0);

// memory_copy.wast:4318
assert_return(() => call($19, "load8_u", [44376]), 0);

// memory_copy.wast:4319
assert_return(() => call($19, "load8_u", [44575]), 0);

// memory_copy.wast:4320
assert_return(() => call($19, "load8_u", [44774]), 0);

// memory_copy.wast:4321
assert_return(() => call($19, "load8_u", [44973]), 0);

// memory_copy.wast:4322
assert_return(() => call($19, "load8_u", [45172]), 0);

// memory_copy.wast:4323
assert_return(() => call($19, "load8_u", [45371]), 0);

// memory_copy.wast:4324
assert_return(() => call($19, "load8_u", [45570]), 0);

// memory_copy.wast:4325
assert_return(() => call($19, "load8_u", [45769]), 0);

// memory_copy.wast:4326
assert_return(() => call($19, "load8_u", [45968]), 0);

// memory_copy.wast:4327
assert_return(() => call($19, "load8_u", [46167]), 0);

// memory_copy.wast:4328
assert_return(() => call($19, "load8_u", [46366]), 0);

// memory_copy.wast:4329
assert_return(() => call($19, "load8_u", [46565]), 0);

// memory_copy.wast:4330
assert_return(() => call($19, "load8_u", [46764]), 0);

// memory_copy.wast:4331
assert_return(() => call($19, "load8_u", [46963]), 0);

// memory_copy.wast:4332
assert_return(() => call($19, "load8_u", [47162]), 0);

// memory_copy.wast:4333
assert_return(() => call($19, "load8_u", [47361]), 0);

// memory_copy.wast:4334
assert_return(() => call($19, "load8_u", [47560]), 0);

// memory_copy.wast:4335
assert_return(() => call($19, "load8_u", [47759]), 0);

// memory_copy.wast:4336
assert_return(() => call($19, "load8_u", [47958]), 0);

// memory_copy.wast:4337
assert_return(() => call($19, "load8_u", [48157]), 0);

// memory_copy.wast:4338
assert_return(() => call($19, "load8_u", [48356]), 0);

// memory_copy.wast:4339
assert_return(() => call($19, "load8_u", [48555]), 0);

// memory_copy.wast:4340
assert_return(() => call($19, "load8_u", [48754]), 0);

// memory_copy.wast:4341
assert_return(() => call($19, "load8_u", [48953]), 0);

// memory_copy.wast:4342
assert_return(() => call($19, "load8_u", [49152]), 0);

// memory_copy.wast:4343
assert_return(() => call($19, "load8_u", [49351]), 0);

// memory_copy.wast:4344
assert_return(() => call($19, "load8_u", [49550]), 0);

// memory_copy.wast:4345
assert_return(() => call($19, "load8_u", [49749]), 0);

// memory_copy.wast:4346
assert_return(() => call($19, "load8_u", [49948]), 0);

// memory_copy.wast:4347
assert_return(() => call($19, "load8_u", [50147]), 0);

// memory_copy.wast:4348
assert_return(() => call($19, "load8_u", [50346]), 0);

// memory_copy.wast:4349
assert_return(() => call($19, "load8_u", [50545]), 0);

// memory_copy.wast:4350
assert_return(() => call($19, "load8_u", [50744]), 0);

// memory_copy.wast:4351
assert_return(() => call($19, "load8_u", [50943]), 0);

// memory_copy.wast:4352
assert_return(() => call($19, "load8_u", [51142]), 0);

// memory_copy.wast:4353
assert_return(() => call($19, "load8_u", [51341]), 0);

// memory_copy.wast:4354
assert_return(() => call($19, "load8_u", [51540]), 0);

// memory_copy.wast:4355
assert_return(() => call($19, "load8_u", [51739]), 0);

// memory_copy.wast:4356
assert_return(() => call($19, "load8_u", [51938]), 0);

// memory_copy.wast:4357
assert_return(() => call($19, "load8_u", [52137]), 0);

// memory_copy.wast:4358
assert_return(() => call($19, "load8_u", [52336]), 0);

// memory_copy.wast:4359
assert_return(() => call($19, "load8_u", [52535]), 0);

// memory_copy.wast:4360
assert_return(() => call($19, "load8_u", [52734]), 0);

// memory_copy.wast:4361
assert_return(() => call($19, "load8_u", [52933]), 0);

// memory_copy.wast:4362
assert_return(() => call($19, "load8_u", [53132]), 0);

// memory_copy.wast:4363
assert_return(() => call($19, "load8_u", [53331]), 0);

// memory_copy.wast:4364
assert_return(() => call($19, "load8_u", [53530]), 0);

// memory_copy.wast:4365
assert_return(() => call($19, "load8_u", [53729]), 0);

// memory_copy.wast:4366
assert_return(() => call($19, "load8_u", [53928]), 0);

// memory_copy.wast:4367
assert_return(() => call($19, "load8_u", [54127]), 0);

// memory_copy.wast:4368
assert_return(() => call($19, "load8_u", [54326]), 0);

// memory_copy.wast:4369
assert_return(() => call($19, "load8_u", [54525]), 0);

// memory_copy.wast:4370
assert_return(() => call($19, "load8_u", [54724]), 0);

// memory_copy.wast:4371
assert_return(() => call($19, "load8_u", [54923]), 0);

// memory_copy.wast:4372
assert_return(() => call($19, "load8_u", [55122]), 0);

// memory_copy.wast:4373
assert_return(() => call($19, "load8_u", [55321]), 0);

// memory_copy.wast:4374
assert_return(() => call($19, "load8_u", [55520]), 0);

// memory_copy.wast:4375
assert_return(() => call($19, "load8_u", [55719]), 0);

// memory_copy.wast:4376
assert_return(() => call($19, "load8_u", [55918]), 0);

// memory_copy.wast:4377
assert_return(() => call($19, "load8_u", [56117]), 0);

// memory_copy.wast:4378
assert_return(() => call($19, "load8_u", [56316]), 0);

// memory_copy.wast:4379
assert_return(() => call($19, "load8_u", [56515]), 0);

// memory_copy.wast:4380
assert_return(() => call($19, "load8_u", [56714]), 0);

// memory_copy.wast:4381
assert_return(() => call($19, "load8_u", [56913]), 0);

// memory_copy.wast:4382
assert_return(() => call($19, "load8_u", [57112]), 0);

// memory_copy.wast:4383
assert_return(() => call($19, "load8_u", [57311]), 0);

// memory_copy.wast:4384
assert_return(() => call($19, "load8_u", [57510]), 0);

// memory_copy.wast:4385
assert_return(() => call($19, "load8_u", [57709]), 0);

// memory_copy.wast:4386
assert_return(() => call($19, "load8_u", [57908]), 0);

// memory_copy.wast:4387
assert_return(() => call($19, "load8_u", [58107]), 0);

// memory_copy.wast:4388
assert_return(() => call($19, "load8_u", [58306]), 0);

// memory_copy.wast:4389
assert_return(() => call($19, "load8_u", [58505]), 0);

// memory_copy.wast:4390
assert_return(() => call($19, "load8_u", [58704]), 0);

// memory_copy.wast:4391
assert_return(() => call($19, "load8_u", [58903]), 0);

// memory_copy.wast:4392
assert_return(() => call($19, "load8_u", [59102]), 0);

// memory_copy.wast:4393
assert_return(() => call($19, "load8_u", [59301]), 0);

// memory_copy.wast:4394
assert_return(() => call($19, "load8_u", [59500]), 0);

// memory_copy.wast:4395
assert_return(() => call($19, "load8_u", [59699]), 0);

// memory_copy.wast:4396
assert_return(() => call($19, "load8_u", [59898]), 0);

// memory_copy.wast:4397
assert_return(() => call($19, "load8_u", [60097]), 0);

// memory_copy.wast:4398
assert_return(() => call($19, "load8_u", [60296]), 0);

// memory_copy.wast:4399
assert_return(() => call($19, "load8_u", [60495]), 0);

// memory_copy.wast:4400
assert_return(() => call($19, "load8_u", [60694]), 0);

// memory_copy.wast:4401
assert_return(() => call($19, "load8_u", [60893]), 0);

// memory_copy.wast:4402
assert_return(() => call($19, "load8_u", [61092]), 0);

// memory_copy.wast:4403
assert_return(() => call($19, "load8_u", [61291]), 0);

// memory_copy.wast:4404
assert_return(() => call($19, "load8_u", [61440]), 0);

// memory_copy.wast:4405
assert_return(() => call($19, "load8_u", [61441]), 1);

// memory_copy.wast:4406
assert_return(() => call($19, "load8_u", [61442]), 2);

// memory_copy.wast:4407
assert_return(() => call($19, "load8_u", [61443]), 3);

// memory_copy.wast:4408
assert_return(() => call($19, "load8_u", [61444]), 4);

// memory_copy.wast:4409
assert_return(() => call($19, "load8_u", [61445]), 5);

// memory_copy.wast:4410
assert_return(() => call($19, "load8_u", [61446]), 6);

// memory_copy.wast:4411
assert_return(() => call($19, "load8_u", [61447]), 7);

// memory_copy.wast:4412
assert_return(() => call($19, "load8_u", [61448]), 8);

// memory_copy.wast:4413
assert_return(() => call($19, "load8_u", [61449]), 9);

// memory_copy.wast:4414
assert_return(() => call($19, "load8_u", [61450]), 10);

// memory_copy.wast:4415
assert_return(() => call($19, "load8_u", [61451]), 11);

// memory_copy.wast:4416
assert_return(() => call($19, "load8_u", [61452]), 12);

// memory_copy.wast:4417
assert_return(() => call($19, "load8_u", [61453]), 13);

// memory_copy.wast:4418
assert_return(() => call($19, "load8_u", [61454]), 14);

// memory_copy.wast:4419
assert_return(() => call($19, "load8_u", [61455]), 15);

// memory_copy.wast:4420
assert_return(() => call($19, "load8_u", [61456]), 16);

// memory_copy.wast:4421
assert_return(() => call($19, "load8_u", [61457]), 17);

// memory_copy.wast:4422
assert_return(() => call($19, "load8_u", [61458]), 18);

// memory_copy.wast:4423
assert_return(() => call($19, "load8_u", [61459]), 19);

// memory_copy.wast:4424
assert_return(() => call($19, "load8_u", [61510]), 0);

// memory_copy.wast:4425
assert_return(() => call($19, "load8_u", [61709]), 0);

// memory_copy.wast:4426
assert_return(() => call($19, "load8_u", [61908]), 0);

// memory_copy.wast:4427
assert_return(() => call($19, "load8_u", [62107]), 0);

// memory_copy.wast:4428
assert_return(() => call($19, "load8_u", [62306]), 0);

// memory_copy.wast:4429
assert_return(() => call($19, "load8_u", [62505]), 0);

// memory_copy.wast:4430
assert_return(() => call($19, "load8_u", [62704]), 0);

// memory_copy.wast:4431
assert_return(() => call($19, "load8_u", [62903]), 0);

// memory_copy.wast:4432
assert_return(() => call($19, "load8_u", [63102]), 0);

// memory_copy.wast:4433
assert_return(() => call($19, "load8_u", [63301]), 0);

// memory_copy.wast:4434
assert_return(() => call($19, "load8_u", [63500]), 0);

// memory_copy.wast:4435
assert_return(() => call($19, "load8_u", [63699]), 0);

// memory_copy.wast:4436
assert_return(() => call($19, "load8_u", [63898]), 0);

// memory_copy.wast:4437
assert_return(() => call($19, "load8_u", [64097]), 0);

// memory_copy.wast:4438
assert_return(() => call($19, "load8_u", [64296]), 0);

// memory_copy.wast:4439
assert_return(() => call($19, "load8_u", [64495]), 0);

// memory_copy.wast:4440
assert_return(() => call($19, "load8_u", [64694]), 0);

// memory_copy.wast:4441
assert_return(() => call($19, "load8_u", [64893]), 0);

// memory_copy.wast:4442
assert_return(() => call($19, "load8_u", [65092]), 0);

// memory_copy.wast:4443
assert_return(() => call($19, "load8_u", [65291]), 0);

// memory_copy.wast:4444
assert_return(() => call($19, "load8_u", [65490]), 0);

// memory_copy.wast:4446
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x0a\x41\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4452
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x41\x0a\x41\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4459
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x0a\x41\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4466
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x41\x0a\x41\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4473
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x41\x0a\x43\x00\x00\xa0\x41\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4480
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x41\x0a\x43\x00\x00\xa0\x41\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4487
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x41\x0a\x43\x00\x00\xa0\x41\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4494
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x41\x0a\x43\x00\x00\xa0\x41\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4501
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x0a\x42\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4508
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x41\x0a\x42\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4515
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x0a\x42\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4522
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x41\x0a\x42\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4529
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x41\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4536
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x41\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4543
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x41\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4550
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x41\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4557
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x41\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4564
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x41\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4571
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x41\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4578
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x41\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4585
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x43\x00\x00\xa0\x41\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4592
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x43\x00\x00\xa0\x41\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4599
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x43\x00\x00\xa0\x41\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4606
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9f\x80\x80\x80\x00\x01\x99\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x43\x00\x00\xa0\x41\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4613
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x42\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4620
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x42\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4627
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x42\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4634
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x42\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4641
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x44\x00\x00\x00\x00\x00\x00\x34\x40\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4648
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9f\x80\x80\x80\x00\x01\x99\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x44\x00\x00\x00\x00\x00\x00\x34\x40\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4655
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x44\x00\x00\x00\x00\x00\x00\x34\x40\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4662
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa3\x80\x80\x80\x00\x01\x9d\x80\x80\x80\x00\x00\x43\x00\x00\x20\x41\x44\x00\x00\x00\x00\x00\x00\x34\x40\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4669
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x42\x0a\x41\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4676
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x42\x0a\x41\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4683
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x42\x0a\x41\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4690
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x42\x0a\x41\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4697
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x42\x0a\x43\x00\x00\xa0\x41\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4704
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x98\x80\x80\x80\x00\x01\x92\x80\x80\x80\x00\x00\x42\x0a\x43\x00\x00\xa0\x41\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4711
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x42\x0a\x43\x00\x00\xa0\x41\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4718
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x42\x0a\x43\x00\x00\xa0\x41\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4725
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x42\x0a\x42\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4732
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x00\x42\x0a\x42\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4739
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x42\x0a\x42\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4746
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x42\x0a\x42\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4753
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x42\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4760
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x42\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4767
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x42\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4774
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x42\x0a\x44\x00\x00\x00\x00\x00\x00\x34\x40\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4781
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x41\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4788
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x41\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4795
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x41\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4802
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x41\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4809
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x43\x00\x00\xa0\x41\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4816
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9f\x80\x80\x80\x00\x01\x99\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x43\x00\x00\xa0\x41\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4823
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x43\x00\x00\xa0\x41\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4830
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa3\x80\x80\x80\x00\x01\x9d\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x43\x00\x00\xa0\x41\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4837
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x42\x14\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4844
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x9c\x80\x80\x80\x00\x01\x96\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x42\x14\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4851
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x42\x14\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4858
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x42\x14\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4865
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x44\x00\x00\x00\x00\x00\x00\x34\x40\x41\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4872
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa3\x80\x80\x80\x00\x01\x9d\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x44\x00\x00\x00\x00\x00\x00\x34\x40\x43\x00\x00\xf0\x41\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4879
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa0\x80\x80\x80\x00\x01\x9a\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x44\x00\x00\x00\x00\x00\x00\x34\x40\x42\x1e\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4886
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x8a\x80\x80\x80\x00\x01\x06\x74\x65\x73\x74\x66\x6e\x00\x00\x0a\xa7\x80\x80\x80\x00\x01\xa1\x80\x80\x80\x00\x00\x44\x00\x00\x00\x00\x00\x00\x24\x40\x44\x00\x00\x00\x00\x00\x00\x34\x40\x44\x00\x00\x00\x00\x00\x00\x3e\x40\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4894
let $20 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x02\x60\x00\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x95\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x00\x0a\x63\x68\x65\x63\x6b\x52\x61\x6e\x67\x65\x00\x01\x0a\xc8\x80\x80\x80\x00\x02\x96\x80\x80\x80\x00\x00\x41\x0a\x41\xd5\x00\x41\x0a\xfc\x0b\x00\x41\x09\x41\x0a\x41\x05\xfc\x0a\x00\x00\x0b\xa7\x80\x80\x80\x00\x00\x03\x40\x20\x00\x20\x01\x46\x04\x40\x41\x7f\x0f\x0b\x20\x00\x2d\x00\x00\x20\x02\x46\x04\x40\x20\x00\x41\x01\x6a\x21\x00\x0c\x01\x0b\x0b\x20\x00\x0f\x0b");

// memory_copy.wast:4911
run(() => call($20, "test", []));

// memory_copy.wast:4913
assert_return(() => call($20, "checkRange", [0, 9, 0]), -1);

// memory_copy.wast:4915
assert_return(() => call($20, "checkRange", [9, 20, 85]), -1);

// memory_copy.wast:4917
assert_return(() => call($20, "checkRange", [20, 65536, 0]), -1);

// memory_copy.wast:4920
let $21 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x02\x60\x00\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x95\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x00\x0a\x63\x68\x65\x63\x6b\x52\x61\x6e\x67\x65\x00\x01\x0a\xc8\x80\x80\x80\x00\x02\x96\x80\x80\x80\x00\x00\x41\x0a\x41\xd5\x00\x41\x0a\xfc\x0b\x00\x41\x10\x41\x0f\x41\x05\xfc\x0a\x00\x00\x0b\xa7\x80\x80\x80\x00\x00\x03\x40\x20\x00\x20\x01\x46\x04\x40\x41\x7f\x0f\x0b\x20\x00\x2d\x00\x00\x20\x02\x46\x04\x40\x20\x00\x41\x01\x6a\x21\x00\x0c\x01\x0b\x0b\x20\x00\x0f\x0b");

// memory_copy.wast:4937
run(() => call($21, "test", []));

// memory_copy.wast:4939
assert_return(() => call($21, "checkRange", [0, 10, 0]), -1);

// memory_copy.wast:4941
assert_return(() => call($21, "checkRange", [10, 21, 85]), -1);

// memory_copy.wast:4943
assert_return(() => call($21, "checkRange", [21, 65536, 0]), -1);

// memory_copy.wast:4946
let $22 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x97\x80\x80\x80\x00\x01\x91\x80\x80\x80\x00\x00\x41\x80\xfe\x03\x41\x80\x80\x02\x41\x81\x02\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4950
assert_trap(() => call($22, "test", []));

// memory_copy.wast:4952
let $23 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x96\x80\x80\x80\x00\x01\x90\x80\x80\x80\x00\x00\x41\x80\x7e\x41\x80\x80\x01\x41\x81\x02\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4956
assert_trap(() => call($23, "test", []));

// memory_copy.wast:4958
let $24 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x97\x80\x80\x80\x00\x01\x91\x80\x80\x80\x00\x00\x41\x80\x80\x02\x41\x80\xfe\x03\x41\x81\x02\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4962
assert_trap(() => call($24, "test", []));

// memory_copy.wast:4964
let $25 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x96\x80\x80\x80\x00\x01\x90\x80\x80\x80\x00\x00\x41\x80\x80\x01\x41\x80\x7e\x41\x81\x02\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4968
assert_trap(() => call($25, "test", []));

// memory_copy.wast:4970
let $26 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x02\x60\x00\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x95\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x00\x0a\x63\x68\x65\x63\x6b\x52\x61\x6e\x67\x65\x00\x01\x0a\xdc\x80\x80\x80\x00\x02\xaa\x80\x80\x80\x00\x00\x41\x00\x41\xd5\x00\x41\x80\x80\x02\xfc\x0b\x00\x41\x80\x80\x02\x41\xaa\x01\x41\x80\x80\x02\xfc\x0b\x00\x41\x80\xa0\x02\x41\x80\xe0\x01\x41\x00\xfc\x0a\x00\x00\x0b\xa7\x80\x80\x80\x00\x00\x03\x40\x20\x00\x20\x01\x46\x04\x40\x41\x7f\x0f\x0b\x20\x00\x2d\x00\x00\x20\x02\x46\x04\x40\x20\x00\x41\x01\x6a\x21\x00\x0c\x01\x0b\x0b\x20\x00\x0f\x0b");

// memory_copy.wast:4988
run(() => call($26, "test", []));

// memory_copy.wast:4990
assert_return(() => call($26, "checkRange", [0, 32768, 85]), -1);

// memory_copy.wast:4992
assert_return(() => call($26, "checkRange", [32768, 65536, 170]), -1);

// memory_copy.wast:4994
let $27 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x96\x80\x80\x80\x00\x01\x90\x80\x80\x80\x00\x00\x41\x80\x80\x04\x41\x80\xe0\x01\x41\x00\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:4998
run(() => call($27, "test", []));

// memory_copy.wast:5000
let $28 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x00\x0a\x96\x80\x80\x80\x00\x01\x90\x80\x80\x80\x00\x00\x41\x80\xa0\x02\x41\x80\x80\x04\x41\x00\xfc\x0a\x00\x00\x0b");

// memory_copy.wast:5004
run(() => call($28, "test", []));

// memory_copy.wast:5006
let $29 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x02\x60\x00\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x95\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x00\x0a\x63\x68\x65\x63\x6b\x52\x61\x6e\x67\x65\x00\x01\x0a\xbe\x95\x80\x80\x00\x02\x8c\x95\x80\x80\x00\x00\x41\xe7\x8a\x01\x41\x01\x41\xc0\x0a\xfc\x0b\x00\x41\xe9\xb0\x02\x41\x02\x41\x9f\x08\xfc\x0b\x00\x41\xd1\xb8\x03\x41\x03\x41\xdc\x07\xfc\x0b\x00\x41\xca\xa8\x02\x41\x04\x41\xc2\x02\xfc\x0b\x00\x41\xa9\x3e\x41\x05\x41\xca\x0f\xfc\x0b\x00\x41\xba\xb1\x01\x41\x06\x41\xdc\x17\xfc\x0b\x00\x41\xf2\x83\x01\x41\x07\x41\xc4\x12\xfc\x0b\x00\x41\xe3\xd3\x02\x41\x08\x41\xc3\x06\xfc\x0b\x00\x41\xfc\x00\x41\x09\x41\xf1\x0a\xfc\x0b\x00\x41\xd4\x10\x41\x0a\x41\xc6\x15\xfc\x0b\x00\x41\x9b\xc6\x00\x41\x0b\x41\x9a\x18\xfc\x0b\x00\x41\xe7\x9b\x03\x41\x0c\x41\xe5\x05\xfc\x0b\x00\x41\xf6\x1e\x41\x0d\x41\x87\x16\xfc\x0b\x00\x41\xb3\x84\x03\x41\x0e\x41\x80\x0a\xfc\x0b\x00\x41\xc9\x89\x03\x41\x0f\x41\xba\x0b\xfc\x0b\x00\x41\x8d\xa0\x01\x41\x10\x41\xd6\x18\xfc\x0b\x00\x41\xb1\xf4\x02\x41\x11\x41\xa0\x04\xfc\x0b\x00\x41\xa3\xe1\x00\x41\x12\x41\xed\x14\xfc\x0b\x00\x41\xa5\xc2\x01\x41\x13\x41\xdb\x14\xfc\x0b\x00\x41\x85\xe2\x02\x41\x14\x41\xa2\x0c\xfc\x0b\x00\x41\xd8\xd0\x02\x41\x15\x41\x9b\x0d\xfc\x0b\x00\x41\xde\x88\x02\x41\x16\x41\x86\x05\xfc\x0b\x00\x41\xab\xfb\x02\x41\x17\x41\xc2\x0e\xfc\x0b\x00\x41\xcd\xa1\x03\x41\x18\x41\xe1\x14\xfc\x0b\x00\x41\x9b\xed\x01\x41\x19\x41\xd5\x07\xfc\x0b\x00\x41\xd4\xc8\x00\x41\x1a\x41\x8f\x0e\xfc\x0b\x00\x41\x8e\x88\x03\x41\x1b\x41\xe7\x03\xfc\x0b\x00\x41\xa1\xea\x03\x41\x1c\x41\x92\x04\xfc\x0b\x00\x41\xdc\x9b\x02\x41\x1d\x41\xaf\x07\xfc\x0b\x00\x41\xf0\x34\x41\x1e\x41\xfd\x02\xfc\x0b\x00\x41\xbe\x90\x03\x41\x1f\x41\x91\x18\xfc\x0b\x00\x41\xc1\x84\x03\x41\x20\x41\x92\x05\xfc\x0b\x00\x41\xfc\xdb\x02\x41\x21\x41\xa6\x0d\xfc\x0b\x00\x41\xbe\x84\x02\x41\x22\x41\xc4\x08\xfc\x0b\x00\x41\xfe\x8c\x03\x41\x23\x41\x82\x0b\xfc\x0b\x00\x41\xea\xf3\x02\x41\x24\x41\x9c\x11\xfc\x0b\x00\x41\xeb\xa6\x03\x41\x25\x41\xda\x12\xfc\x0b\x00\x41\x8f\xaf\x03\x41\x26\x41\xfa\x01\xfc\x0b\x00\x41\xdc\xb0\x01\x41\x27\x41\xb1\x10\xfc\x0b\x00\x41\xec\x85\x01\x41\x28\x41\xc0\x19\xfc\x0b\x00\x41\xbb\xa8\x03\x41\x29\x41\xe3\x19\xfc\x0b\x00\x41\xb2\xb4\x02\x41\x2a\x41\xec\x15\xfc\x0b\x00\x41\xbc\x9a\x02\x41\x2b\x41\x96\x10\xfc\x0b\x00\x41\xec\x93\x02\x41\x2c\x41\xcb\x15\xfc\x0b\x00\x41\xdb\xff\x01\x41\x2d\x41\xb8\x02\xfc\x0b\x00\x41\x82\xf2\x03\x41\x2e\x41\xc0\x01\xfc\x0b\x00\x41\xfe\xf1\x01\x41\x2f\x41\xd4\x04\xfc\x0b\x00\x41\xfb\x81\x01\x41\x30\x41\xf5\x03\xfc\x0b\x00\x41\xaa\xbd\x03\x41\x31\x41\xae\x05\xfc\x0b\x00\x41\xfb\x8b\x02\x41\x32\x41\x81\x03\xfc\x0b\x00\x41\xd1\xdb\x03\x41\x33\x41\x87\x07\xfc\x0b\x00\x41\x85\xe0\x03\x41\x34\x41\xd6\x12\xfc\x0b\x00\x41\xfc\xee\x02\x41\x35\x41\xa1\x0b\xfc\x0b\x00\x41\xf5\xca\x01\x41\x36\x41\xda\x18\xfc\x0b\x00\x41\xbe\x2b\x41\x37\x41\xd7\x10\xfc\x0b\x00\x41\x89\x99\x02\x41\x38\x41\x87\x04\xfc\x0b\x00\x41\xdc\xde\x02\x41\x39\x41\xd0\x19\xfc\x0b\x00\x41\xa8\xed\x02\x41\x3a\x41\x8e\x0d\xfc\x0b\x00\x41\x8f\xec\x02\x41\x3b\x41\xe0\x18\xfc\x0b\x00\x41\xb1\xaf\x01\x41\x3c\x41\xa1\x0b\xfc\x0b\x00\x41\xf1\xc9\x03\x41\x3d\x41\x97\x05\xfc\x0b\x00\x41\x85\xfc\x01\x41\x3e\x41\x87\x0d\xfc\x0b\x00\x41\xf7\x17\x41\x3f\x41\xd1\x05\xfc\x0b\x00\x41\xe9\x89\x02\x41\xc0\x00\x41\xd4\x00\xfc\x0b\x00\x41\xba\x84\x02\x41\xc1\x00\x41\xed\x0f\xfc\x0b\x00\x41\xca\x9f\x02\x41\xc2\x00\x41\x1d\xfc\x0b\x00\x41\xcb\x95\x01\x41\xc3\x00\x41\xda\x17\xfc\x0b\x00\x41\xc8\xe2\x00\x41\xc4\x00\x41\x93\x08\xfc\x0b\x00\x41\xe4\x8e\x01\x41\xc5\x00\x41\xfc\x19\xfc\x0b\x00\x41\x9f\x24\x41\xc6\x00\x41\xc3\x08\xfc\x0b\x00\x41\x9e\xfe\x00\x41\xc7\x00\x41\xcd\x0f\xfc\x0b\x00\x41\x9c\x8e\x01\x41\xc8\x00\x41\xd3\x11\xfc\x0b\x00\x41\xe4\x8a\x03\x41\xc9\x00\x41\xf5\x18\xfc\x0b\x00\x41\x94\xd6\x00\x41\xca\x00\x41\xb0\x0f\xfc\x0b\x00\x41\xda\xfc\x00\x41\xcb\x00\x41\xaf\x0b\xfc\x0b\x00\x41\xde\xe2\x02\x41\xcc\x00\x41\x99\x09\xfc\x0b\x00\x41\xf9\xa6\x03\x41\xcd\x00\x41\xa0\x0c\xfc\x0b\x00\x41\xbb\x82\x02\x41\xce\x00\x41\xea\x0c\xfc\x0b\x00\x41\xe4\xdc\x03\x41\xcf\x00\x41\xd4\x19\xfc\x0b\x00\x41\x91\x94\x03\x41\xd0\x00\x41\xdf\x01\xfc\x0b\x00\x41\x89\x22\x41\xd1\x00\x41\xfb\x10\xfc\x0b\x00\x41\xaa\xc1\x03\x41\xd2\x00\x41\xaa\x0a\xfc\x0b\x00\x41\xac\xb3\x03\x41\xd3\x00\x41\xd8\x14\xfc\x0b\x00\x41\x9b\xbc\x01\x41\xd4\x00\x41\x95\x08\xfc\x0b\x00\x41\xaf\xd1\x02\x41\xd5\x00\x41\x99\x18\xfc\x0b\x00\x41\xb3\xfc\x01\x41\xd6\x00\x41\xec\x15\xfc\x0b\x00\x41\xe3\x1d\x41\xd7\x00\x41\xda\x0f\xfc\x0b\x00\x41\xc8\xac\x03\x41\xd8\x00\x41\x00\xfc\x0b\x00\x41\x95\x86\x03\x41\xd9\x00\x41\x95\x10\xfc\x0b\x00\x41\xbb\x9f\x01\x41\xda\x00\x41\xd0\x16\xfc\x0b\x00\x41\xa2\x88\x02\x41\xdb\x00\x41\xc0\x01\xfc\x0b\x00\x41\xba\xc9\x00\x41\xdc\x00\x41\x93\x11\xfc\x0b\x00\x41\xfd\xe0\x00\x41\xdd\x00\x41\x18\xfc\x0b\x00\x41\x8b\xee\x00\x41\xde\x00\x41\xc1\x04\xfc\x0b\x00\x41\x9a\xd8\x02\x41\xdf\x00\x41\xa9\x10\xfc\x0b\x00\x41\xff\x9e\x02\x41\xe0\x00\x41\xec\x1a\xfc\x0b\x00\x41\xf8\xb5\x01\x41\xe1\x00\x41\xcd\x15\xfc\x0b\x00\x41\xf8\x31\x41\xe2\x00\x41\xbe\x06\xfc\x0b\x00\x41\x9b\x84\x02\x41\xe3\x00\x41\x92\x0f\xfc\x0b\x00\x41\xb5\xab\x01\x41\xe4\x00\x41\xbe\x15\xfc\x0b\x00\x41\xce\xce\x03\x41\xe8\xa7\x03\x41\xb2\x10\xfc\x0a\x00\x00\x41\xb2\xec\x03\x41\xb8\xb2\x02\x41\xe6\x01\xfc\x0a\x00\x00\x41\xf9\x94\x03\x41\xcd\xb8\x01\x41\xfc\x11\xfc\x0a\x00\x00\x41\xb4\x34\x41\xbc\xbb\x01\x41\xff\x04\xfc\x0a\x00\x00\x41\xce\x36\x41\xf7\x84\x02\x41\xc9\x08\xfc\x0a\x00\x00\x41\xcb\x97\x01\x41\xec\xd0\x00\x41\xfd\x18\xfc\x0a\x00\x00\x41\xac\xd5\x01\x41\x86\xa9\x03\x41\xe4\x00\xfc\x0a\x00\x00\x41\xd5\xd4\x01\x41\xa2\xd5\x02\x41\xb5\x0d\xfc\x0a\x00\x00\x41\xf0\xd8\x03\x41\xb5\xc3\x00\x41\xf7\x00\xfc\x0a\x00\x00\x41\xbb\x2e\x41\x84\x12\x41\x92\x05\xfc\x0a\x00\x00\x41\xb3\x25\x41\xaf\x93\x03\x41\xdd\x11\xfc\x0a\x00\x00\x41\xc9\xe2\x00\x41\xfd\x95\x01\x41\xc1\x06\xfc\x0a\x00\x00\x41\xce\xdc\x00\x41\xa9\xeb\x02\x41\xe4\x19\xfc\x0a\x00\x00\x41\xf0\xd8\x00\x41\xd4\xdf\x02\x41\xe9\x11\xfc\x0a\x00\x00\x41\x8a\x8b\x02\x41\xa9\x34\x41\x8c\x14\xfc\x0a\x00\x00\x41\xc8\x26\x41\x9a\x0d\x41\xb0\x0a\xfc\x0a\x00\x00\x41\xbc\xed\x03\x41\xd5\x3b\x41\x86\x0d\xfc\x0a\x00\x00\x41\x98\xdc\x02\x41\xa8\x8f\x01\x41\x21\xfc\x0a\x00\x00\x41\x8e\xd7\x02\x41\xcc\xae\x01\x41\x93\x0b\xfc\x0a\x00\x00\x41\xad\xec\x02\x41\x9b\x85\x03\x41\x9a\x0b\xfc\x0a\x00\x00\x41\xc4\xf1\x03\x41\xb3\xc4\x00\x41\xc2\x06\xfc\x0a\x00\x00\x41\xcd\x85\x02\x41\xa3\x9d\x01\x41\xf5\x19\xfc\x0a\x00\x00\x41\xff\xbc\x02\x41\xad\xa8\x03\x41\x81\x19\xfc\x0a\x00\x00\x41\xd4\xc9\x01\x41\xf6\xce\x03\x41\x94\x13\xfc\x0a\x00\x00\x41\xde\x99\x01\x41\xb2\xbc\x03\x41\xda\x02\xfc\x0a\x00\x00\x41\xec\xfb\x00\x41\xca\x98\x02\x41\xfe\x12\xfc\x0a\x00\x00\x41\xb0\xdc\x00\x41\xf6\x95\x02\x41\xac\x02\xfc\x0a\x00\x00\x41\xa3\xd0\x03\x41\x85\xed\x00\x41\xd1\x18\xfc\x0a\x00\x00\x41\xfb\x8b\x02\x41\xb2\xd9\x03\x41\x81\x0a\xfc\x0a\x00\x00\x41\x84\xc6\x00\x41\xf4\xdf\x00\x41\xaf\x07\xfc\x0a\x00\x00\x41\x8b\x16\x41\xb9\xd1\x00\x41\xdf\x0e\xfc\x0a\x00\x00\x41\xba\xd1\x02\x41\x86\xd7\x02\x41\xe2\x05\xfc\x0a\x00\x00\x41\xbe\xec\x03\x41\x85\x94\x01\x41\xfa\x00\xfc\x0a\x00\x00\x41\xec\xbb\x01\x41\xd9\xdd\x02\x41\xdb\x0d\xfc\x0a\x00\x00\x41\xd0\xb0\x01\x41\xa3\xf3\x00\x41\xbe\x05\xfc\x0a\x00\x00\x41\x94\xd8\x00\x41\xd3\xcf\x01\x41\xa6\x0e\xfc\x0a\x00\x00\x41\xb4\xb4\x01\x41\xf7\x9f\x01\x41\xa8\x08\xfc\x0a\x00\x00\x41\xa0\xbf\x03\x41\xf2\xab\x03\x41\xc7\x14\xfc\x0a\x00\x00\x41\x94\xc7\x01\x41\x81\x08\x41\xa9\x18\xfc\x0a\x00\x00\x41\xb4\x83\x03\x41\xbc\xd9\x02\x41\xcf\x07\xfc\x0a\x00\x00\x41\xf8\xdc\x01\x41\xfa\xc5\x02\x41\xa0\x12\xfc\x0a\x00\x00\x41\xe9\xde\x03\x41\xe6\x01\x41\xb8\x16\xfc\x0a\x00\x00\x41\xd0\xaf\x01\x41\x9a\x9a\x03\x41\x95\x11\xfc\x0a\x00\x00\x41\xe9\xbc\x02\x41\xea\xca\x00\x41\xa6\x0f\xfc\x0a\x00\x00\x41\xcc\xe2\x01\x41\xfe\xa2\x01\x41\x8a\x11\xfc\x0a\x00\x00\x41\xa5\x9e\x03\x41\xb3\xd7\x02\x41\x8d\x08\xfc\x0a\x00\x00\x41\x84\xc7\x01\x41\xd3\x96\x02\x41\xf2\x0c\xfc\x0a\x00\x00\x41\x94\xc9\x03\x41\xfb\xe5\x02\x41\xc2\x0f\xfc\x0a\x00\x00\x41\x99\xab\x02\x41\x90\x2d\x41\xa3\x0f\xfc\x0a\x00\x00\x41\xd7\xde\x01\x41\xc4\xb0\x03\x41\xc0\x12\xfc\x0a\x00\x00\x41\x9b\xe9\x03\x41\xbc\x8d\x01\x41\xcc\x0a\xfc\x0a\x00\x00\x41\xe5\x87\x03\x41\xa5\xec\x00\x41\xfe\x02\xfc\x0a\x00\x00\x41\x88\x84\x01\x41\xf5\x9b\x02\x41\xec\x0e\xfc\x0a\x00\x00\x41\xe2\xf7\x02\x41\xde\xd8\x00\x41\xf7\x15\xfc\x0a\x00\x00\x41\xe0\xde\x01\x41\xaa\xbb\x02\x41\xc3\x02\xfc\x0a\x00\x00\x41\xb2\x95\x02\x41\xd0\xd9\x01\x41\x86\x0d\xfc\x0a\x00\x00\x41\xfa\xeb\x03\x41\xd4\xa0\x03\x41\xbd\x0a\xfc\x0a\x00\x00\x41\xb5\xee\x00\x41\xe8\xe9\x02\x41\x84\x05\xfc\x0a\x00\x00\x41\xe6\xe2\x01\x41\x82\x95\x01\x41\xf0\x03\xfc\x0a\x00\x00\x41\x98\xdf\x02\x41\xd9\xf3\x02\x41\xe0\x15\xfc\x0a\x00\x00\x41\x87\xb5\x02\x41\xf5\xdc\x02\x41\xc6\x0a\xfc\x0a\x00\x00\x41\xf0\xd0\x00\x41\xda\xe4\x01\x41\xc3\x0b\xfc\x0a\x00\x00\x41\xbf\xee\x02\x41\xe2\xe8\x02\x41\xbb\x0b\xfc\x0a\x00\x00\x41\xa9\x26\x41\xc4\xe0\x01\x41\xe7\x0e\xfc\x0a\x00\x00\x41\xfc\xa8\x02\x41\xa5\xbf\x03\x41\xd7\x0d\xfc\x0a\x00\x00\x41\xce\xce\x01\x41\xd7\xd4\x01\x41\xe7\x08\xfc\x0a\x00\x00\x41\xd3\xcb\x03\x41\xd1\xc0\x01\x41\xa7\x08\xfc\x0a\x00\x00\x41\xac\xdf\x03\x41\x86\xaf\x02\x41\xfe\x05\xfc\x0a\x00\x00\x41\x80\xd9\x02\x41\xec\x11\x41\xf0\x0b\xfc\x0a\x00\x00\x41\xe4\xff\x01\x41\x85\xf1\x02\x41\xc6\x17\xfc\x0a\x00\x00\x41\x8c\xd7\x00\x41\x8c\xa6\x01\x41\xf3\x07\xfc\x0a\x00\x00\x41\xf1\x3b\x41\xfc\xf6\x01\x41\xda\x17\xfc\x0a\x00\x00\x41\xfc\x8c\x01\x41\xbb\xe5\x00\x41\xf8\x19\xfc\x0a\x00\x00\x41\xda\xbf\x03\x41\xe1\xb4\x03\x41\xb4\x02\xfc\x0a\x00\x00\x41\xe3\xc0\x01\x41\xaf\x83\x01\x41\x83\x09\xfc\x0a\x00\x00\x41\xbc\x9b\x01\x41\x83\xcf\x00\x41\xd2\x05\xfc\x0a\x00\x00\x41\xe9\x16\x41\xaf\x2e\x41\xc2\x12\xfc\x0a\x00\x00\x41\xff\xfb\x01\x41\xaf\x87\x03\x41\xee\x16\xfc\x0a\x00\x00\x41\x96\xf6\x00\x41\x93\x87\x01\x41\xaf\x14\xfc\x0a\x00\x00\x41\x87\xe4\x02\x41\x9f\xde\x01\x41\xfd\x0f\xfc\x0a\x00\x00\x41\xed\xae\x03\x41\x91\x9a\x02\x41\xa4\x14\xfc\x0a\x00\x00\x41\xad\xde\x01\x41\x8d\xa7\x03\x41\x90\x09\xfc\x0a\x00\x00\x41\xcf\xf6\x02\x41\x89\xa1\x03\x41\xc1\x18\xfc\x0a\x00\x00\x41\xb6\xef\x01\x41\xe3\xe0\x02\x41\xd9\x14\xfc\x0a\x00\x00\x41\xc1\x27\x41\xc7\x21\x41\x34\xfc\x0a\x00\x00\x41\xa4\x34\x41\x83\xbd\x01\x41\xb9\x03\xfc\x0a\x00\x00\x41\xd8\x81\x02\x41\xed\xd3\x01\x41\xf5\x1a\xfc\x0a\x00\x00\x41\x92\xfe\x01\x41\xec\xcf\x03\x41\xe1\x15\xfc\x0a\x00\x00\x41\xb9\x8c\x02\x41\x82\xc6\x00\x41\xe6\x12\xfc\x0a\x00\x00\x41\xe5\x8b\x01\x41\x8a\xaa\x03\x41\xb5\x1a\xfc\x0a\x00\x00\x41\x9d\xb1\x01\x41\xf7\xd8\x02\x41\x88\x01\xfc\x0a\x00\x00\x41\xd1\xcd\x03\x41\xa5\x37\x41\x95\x08\xfc\x0a\x00\x00\x41\xc1\xcf\x02\x41\xf4\xad\x03\x41\xd5\x12\xfc\x0a\x00\x00\x41\x95\xdd\x02\x41\xaa\x9d\x01\x41\xed\x06\xfc\x0a\x00\x00\x41\xca\x9f\x02\x41\xec\xc4\x01\x41\xf7\x1a\xfc\x0a\x00\x00\x41\xae\xe5\x02\x41\x90\xf9\x01\x41\xd6\x06\xfc\x0a\x00\x00\x41\xac\xbd\x01\x41\xfa\xf8\x01\x41\xe1\x0a\xfc\x0a\x00\x00\x41\xf2\x87\x02\x41\xb4\x05\x41\xba\x0c\xfc\x0a\x00\x00\x41\xca\xd9\x03\x41\x99\x91\x01\x41\xab\x17\xfc\x0a\x00\x00\x41\xc2\x89\x03\x41\xb7\xc2\x02\x41\xfe\x0a\xfc\x0a\x00\x00\x0b\xa7\x80\x80\x80\x00\x00\x03\x40\x20\x00\x20\x01\x46\x04\x40\x41\x7f\x0f\x0b\x20\x00\x2d\x00\x00\x20\x02\x46\x04\x40\x20\x00\x41\x01\x6a\x21\x00\x0c\x01\x0b\x0b\x20\x00\x0f\x0b");

// memory_copy.wast:5222
run(() => call($29, "test", []));

// memory_copy.wast:5224
assert_return(() => call($29, "checkRange", [0, 124, 0]), -1);

// memory_copy.wast:5226
assert_return(() => call($29, "checkRange", [124, 1517, 9]), -1);

// memory_copy.wast:5228
assert_return(() => call($29, "checkRange", [1517, 2132, 0]), -1);

// memory_copy.wast:5230
assert_return(() => call($29, "checkRange", [2132, 2827, 10]), -1);

// memory_copy.wast:5232
assert_return(() => call($29, "checkRange", [2827, 2921, 92]), -1);

// memory_copy.wast:5234
assert_return(() => call($29, "checkRange", [2921, 3538, 83]), -1);

// memory_copy.wast:5236
assert_return(() => call($29, "checkRange", [3538, 3786, 77]), -1);

// memory_copy.wast:5238
assert_return(() => call($29, "checkRange", [3786, 4042, 97]), -1);

// memory_copy.wast:5240
assert_return(() => call($29, "checkRange", [4042, 4651, 99]), -1);

// memory_copy.wast:5242
assert_return(() => call($29, "checkRange", [4651, 5057, 0]), -1);

// memory_copy.wast:5244
assert_return(() => call($29, "checkRange", [5057, 5109, 99]), -1);

// memory_copy.wast:5246
assert_return(() => call($29, "checkRange", [5109, 5291, 0]), -1);

// memory_copy.wast:5248
assert_return(() => call($29, "checkRange", [5291, 5524, 72]), -1);

// memory_copy.wast:5250
assert_return(() => call($29, "checkRange", [5524, 5691, 92]), -1);

// memory_copy.wast:5252
assert_return(() => call($29, "checkRange", [5691, 6552, 83]), -1);

// memory_copy.wast:5254
assert_return(() => call($29, "checkRange", [6552, 7133, 77]), -1);

// memory_copy.wast:5256
assert_return(() => call($29, "checkRange", [7133, 7665, 99]), -1);

// memory_copy.wast:5258
assert_return(() => call($29, "checkRange", [7665, 8314, 0]), -1);

// memory_copy.wast:5260
assert_return(() => call($29, "checkRange", [8314, 8360, 62]), -1);

// memory_copy.wast:5262
assert_return(() => call($29, "checkRange", [8360, 8793, 86]), -1);

// memory_copy.wast:5264
assert_return(() => call($29, "checkRange", [8793, 8979, 83]), -1);

// memory_copy.wast:5266
assert_return(() => call($29, "checkRange", [8979, 9373, 79]), -1);

// memory_copy.wast:5268
assert_return(() => call($29, "checkRange", [9373, 9518, 95]), -1);

// memory_copy.wast:5270
assert_return(() => call($29, "checkRange", [9518, 9934, 59]), -1);

// memory_copy.wast:5272
assert_return(() => call($29, "checkRange", [9934, 10087, 77]), -1);

// memory_copy.wast:5274
assert_return(() => call($29, "checkRange", [10087, 10206, 5]), -1);

// memory_copy.wast:5276
assert_return(() => call($29, "checkRange", [10206, 10230, 77]), -1);

// memory_copy.wast:5278
assert_return(() => call($29, "checkRange", [10230, 10249, 41]), -1);

// memory_copy.wast:5280
assert_return(() => call($29, "checkRange", [10249, 11148, 83]), -1);

// memory_copy.wast:5282
assert_return(() => call($29, "checkRange", [11148, 11356, 74]), -1);

// memory_copy.wast:5284
assert_return(() => call($29, "checkRange", [11356, 11380, 93]), -1);

// memory_copy.wast:5286
assert_return(() => call($29, "checkRange", [11380, 11939, 74]), -1);

// memory_copy.wast:5288
assert_return(() => call($29, "checkRange", [11939, 12159, 68]), -1);

// memory_copy.wast:5290
assert_return(() => call($29, "checkRange", [12159, 12575, 83]), -1);

// memory_copy.wast:5292
assert_return(() => call($29, "checkRange", [12575, 12969, 79]), -1);

// memory_copy.wast:5294
assert_return(() => call($29, "checkRange", [12969, 13114, 95]), -1);

// memory_copy.wast:5296
assert_return(() => call($29, "checkRange", [13114, 14133, 59]), -1);

// memory_copy.wast:5298
assert_return(() => call($29, "checkRange", [14133, 14404, 76]), -1);

// memory_copy.wast:5300
assert_return(() => call($29, "checkRange", [14404, 14428, 57]), -1);

// memory_copy.wast:5302
assert_return(() => call($29, "checkRange", [14428, 14458, 59]), -1);

// memory_copy.wast:5304
assert_return(() => call($29, "checkRange", [14458, 14580, 32]), -1);

// memory_copy.wast:5306
assert_return(() => call($29, "checkRange", [14580, 14777, 89]), -1);

// memory_copy.wast:5308
assert_return(() => call($29, "checkRange", [14777, 15124, 59]), -1);

// memory_copy.wast:5310
assert_return(() => call($29, "checkRange", [15124, 15126, 36]), -1);

// memory_copy.wast:5312
assert_return(() => call($29, "checkRange", [15126, 15192, 100]), -1);

// memory_copy.wast:5314
assert_return(() => call($29, "checkRange", [15192, 15871, 96]), -1);

// memory_copy.wast:5316
assert_return(() => call($29, "checkRange", [15871, 15998, 95]), -1);

// memory_copy.wast:5318
assert_return(() => call($29, "checkRange", [15998, 17017, 59]), -1);

// memory_copy.wast:5320
assert_return(() => call($29, "checkRange", [17017, 17288, 76]), -1);

// memory_copy.wast:5322
assert_return(() => call($29, "checkRange", [17288, 17312, 57]), -1);

// memory_copy.wast:5324
assert_return(() => call($29, "checkRange", [17312, 17342, 59]), -1);

// memory_copy.wast:5326
assert_return(() => call($29, "checkRange", [17342, 17464, 32]), -1);

// memory_copy.wast:5328
assert_return(() => call($29, "checkRange", [17464, 17661, 89]), -1);

// memory_copy.wast:5330
assert_return(() => call($29, "checkRange", [17661, 17727, 59]), -1);

// memory_copy.wast:5332
assert_return(() => call($29, "checkRange", [17727, 17733, 5]), -1);

// memory_copy.wast:5334
assert_return(() => call($29, "checkRange", [17733, 17893, 96]), -1);

// memory_copy.wast:5336
assert_return(() => call($29, "checkRange", [17893, 18553, 77]), -1);

// memory_copy.wast:5338
assert_return(() => call($29, "checkRange", [18553, 18744, 42]), -1);

// memory_copy.wast:5340
assert_return(() => call($29, "checkRange", [18744, 18801, 76]), -1);

// memory_copy.wast:5342
assert_return(() => call($29, "checkRange", [18801, 18825, 57]), -1);

// memory_copy.wast:5344
assert_return(() => call($29, "checkRange", [18825, 18876, 59]), -1);

// memory_copy.wast:5346
assert_return(() => call($29, "checkRange", [18876, 18885, 77]), -1);

// memory_copy.wast:5348
assert_return(() => call($29, "checkRange", [18885, 18904, 41]), -1);

// memory_copy.wast:5350
assert_return(() => call($29, "checkRange", [18904, 19567, 83]), -1);

// memory_copy.wast:5352
assert_return(() => call($29, "checkRange", [19567, 20403, 96]), -1);

// memory_copy.wast:5354
assert_return(() => call($29, "checkRange", [20403, 21274, 77]), -1);

// memory_copy.wast:5356
assert_return(() => call($29, "checkRange", [21274, 21364, 100]), -1);

// memory_copy.wast:5358
assert_return(() => call($29, "checkRange", [21364, 21468, 74]), -1);

// memory_copy.wast:5360
assert_return(() => call($29, "checkRange", [21468, 21492, 93]), -1);

// memory_copy.wast:5362
assert_return(() => call($29, "checkRange", [21492, 22051, 74]), -1);

// memory_copy.wast:5364
assert_return(() => call($29, "checkRange", [22051, 22480, 68]), -1);

// memory_copy.wast:5366
assert_return(() => call($29, "checkRange", [22480, 22685, 100]), -1);

// memory_copy.wast:5368
assert_return(() => call($29, "checkRange", [22685, 22694, 68]), -1);

// memory_copy.wast:5370
assert_return(() => call($29, "checkRange", [22694, 22821, 10]), -1);

// memory_copy.wast:5372
assert_return(() => call($29, "checkRange", [22821, 22869, 100]), -1);

// memory_copy.wast:5374
assert_return(() => call($29, "checkRange", [22869, 24107, 97]), -1);

// memory_copy.wast:5376
assert_return(() => call($29, "checkRange", [24107, 24111, 37]), -1);

// memory_copy.wast:5378
assert_return(() => call($29, "checkRange", [24111, 24236, 77]), -1);

// memory_copy.wast:5380
assert_return(() => call($29, "checkRange", [24236, 24348, 72]), -1);

// memory_copy.wast:5382
assert_return(() => call($29, "checkRange", [24348, 24515, 92]), -1);

// memory_copy.wast:5384
assert_return(() => call($29, "checkRange", [24515, 24900, 83]), -1);

// memory_copy.wast:5386
assert_return(() => call($29, "checkRange", [24900, 25136, 95]), -1);

// memory_copy.wast:5388
assert_return(() => call($29, "checkRange", [25136, 25182, 85]), -1);

// memory_copy.wast:5390
assert_return(() => call($29, "checkRange", [25182, 25426, 68]), -1);

// memory_copy.wast:5392
assert_return(() => call($29, "checkRange", [25426, 25613, 89]), -1);

// memory_copy.wast:5394
assert_return(() => call($29, "checkRange", [25613, 25830, 96]), -1);

// memory_copy.wast:5396
assert_return(() => call($29, "checkRange", [25830, 26446, 100]), -1);

// memory_copy.wast:5398
assert_return(() => call($29, "checkRange", [26446, 26517, 10]), -1);

// memory_copy.wast:5400
assert_return(() => call($29, "checkRange", [26517, 27468, 92]), -1);

// memory_copy.wast:5402
assert_return(() => call($29, "checkRange", [27468, 27503, 95]), -1);

// memory_copy.wast:5404
assert_return(() => call($29, "checkRange", [27503, 27573, 77]), -1);

// memory_copy.wast:5406
assert_return(() => call($29, "checkRange", [27573, 28245, 92]), -1);

// memory_copy.wast:5408
assert_return(() => call($29, "checkRange", [28245, 28280, 95]), -1);

// memory_copy.wast:5410
assert_return(() => call($29, "checkRange", [28280, 29502, 77]), -1);

// memory_copy.wast:5412
assert_return(() => call($29, "checkRange", [29502, 29629, 42]), -1);

// memory_copy.wast:5414
assert_return(() => call($29, "checkRange", [29629, 30387, 83]), -1);

// memory_copy.wast:5416
assert_return(() => call($29, "checkRange", [30387, 30646, 77]), -1);

// memory_copy.wast:5418
assert_return(() => call($29, "checkRange", [30646, 31066, 92]), -1);

// memory_copy.wast:5420
assert_return(() => call($29, "checkRange", [31066, 31131, 77]), -1);

// memory_copy.wast:5422
assert_return(() => call($29, "checkRange", [31131, 31322, 42]), -1);

// memory_copy.wast:5424
assert_return(() => call($29, "checkRange", [31322, 31379, 76]), -1);

// memory_copy.wast:5426
assert_return(() => call($29, "checkRange", [31379, 31403, 57]), -1);

// memory_copy.wast:5428
assert_return(() => call($29, "checkRange", [31403, 31454, 59]), -1);

// memory_copy.wast:5430
assert_return(() => call($29, "checkRange", [31454, 31463, 77]), -1);

// memory_copy.wast:5432
assert_return(() => call($29, "checkRange", [31463, 31482, 41]), -1);

// memory_copy.wast:5434
assert_return(() => call($29, "checkRange", [31482, 31649, 83]), -1);

// memory_copy.wast:5436
assert_return(() => call($29, "checkRange", [31649, 31978, 72]), -1);

// memory_copy.wast:5438
assert_return(() => call($29, "checkRange", [31978, 32145, 92]), -1);

// memory_copy.wast:5440
assert_return(() => call($29, "checkRange", [32145, 32530, 83]), -1);

// memory_copy.wast:5442
assert_return(() => call($29, "checkRange", [32530, 32766, 95]), -1);

// memory_copy.wast:5444
assert_return(() => call($29, "checkRange", [32766, 32812, 85]), -1);

// memory_copy.wast:5446
assert_return(() => call($29, "checkRange", [32812, 33056, 68]), -1);

// memory_copy.wast:5448
assert_return(() => call($29, "checkRange", [33056, 33660, 89]), -1);

// memory_copy.wast:5450
assert_return(() => call($29, "checkRange", [33660, 33752, 59]), -1);

// memory_copy.wast:5452
assert_return(() => call($29, "checkRange", [33752, 33775, 36]), -1);

// memory_copy.wast:5454
assert_return(() => call($29, "checkRange", [33775, 33778, 32]), -1);

// memory_copy.wast:5456
assert_return(() => call($29, "checkRange", [33778, 34603, 9]), -1);

// memory_copy.wast:5458
assert_return(() => call($29, "checkRange", [34603, 35218, 0]), -1);

// memory_copy.wast:5460
assert_return(() => call($29, "checkRange", [35218, 35372, 10]), -1);

// memory_copy.wast:5462
assert_return(() => call($29, "checkRange", [35372, 35486, 77]), -1);

// memory_copy.wast:5464
assert_return(() => call($29, "checkRange", [35486, 35605, 5]), -1);

// memory_copy.wast:5466
assert_return(() => call($29, "checkRange", [35605, 35629, 77]), -1);

// memory_copy.wast:5468
assert_return(() => call($29, "checkRange", [35629, 35648, 41]), -1);

// memory_copy.wast:5470
assert_return(() => call($29, "checkRange", [35648, 36547, 83]), -1);

// memory_copy.wast:5472
assert_return(() => call($29, "checkRange", [36547, 36755, 74]), -1);

// memory_copy.wast:5474
assert_return(() => call($29, "checkRange", [36755, 36767, 93]), -1);

// memory_copy.wast:5476
assert_return(() => call($29, "checkRange", [36767, 36810, 83]), -1);

// memory_copy.wast:5478
assert_return(() => call($29, "checkRange", [36810, 36839, 100]), -1);

// memory_copy.wast:5480
assert_return(() => call($29, "checkRange", [36839, 37444, 96]), -1);

// memory_copy.wast:5482
assert_return(() => call($29, "checkRange", [37444, 38060, 100]), -1);

// memory_copy.wast:5484
assert_return(() => call($29, "checkRange", [38060, 38131, 10]), -1);

// memory_copy.wast:5486
assert_return(() => call($29, "checkRange", [38131, 39082, 92]), -1);

// memory_copy.wast:5488
assert_return(() => call($29, "checkRange", [39082, 39117, 95]), -1);

// memory_copy.wast:5490
assert_return(() => call($29, "checkRange", [39117, 39187, 77]), -1);

// memory_copy.wast:5492
assert_return(() => call($29, "checkRange", [39187, 39859, 92]), -1);

// memory_copy.wast:5494
assert_return(() => call($29, "checkRange", [39859, 39894, 95]), -1);

// memory_copy.wast:5496
assert_return(() => call($29, "checkRange", [39894, 40257, 77]), -1);

// memory_copy.wast:5498
assert_return(() => call($29, "checkRange", [40257, 40344, 89]), -1);

// memory_copy.wast:5500
assert_return(() => call($29, "checkRange", [40344, 40371, 59]), -1);

// memory_copy.wast:5502
assert_return(() => call($29, "checkRange", [40371, 40804, 77]), -1);

// memory_copy.wast:5504
assert_return(() => call($29, "checkRange", [40804, 40909, 5]), -1);

// memory_copy.wast:5506
assert_return(() => call($29, "checkRange", [40909, 42259, 92]), -1);

// memory_copy.wast:5508
assert_return(() => call($29, "checkRange", [42259, 42511, 77]), -1);

// memory_copy.wast:5510
assert_return(() => call($29, "checkRange", [42511, 42945, 83]), -1);

// memory_copy.wast:5512
assert_return(() => call($29, "checkRange", [42945, 43115, 77]), -1);

// memory_copy.wast:5514
assert_return(() => call($29, "checkRange", [43115, 43306, 42]), -1);

// memory_copy.wast:5516
assert_return(() => call($29, "checkRange", [43306, 43363, 76]), -1);

// memory_copy.wast:5518
assert_return(() => call($29, "checkRange", [43363, 43387, 57]), -1);

// memory_copy.wast:5520
assert_return(() => call($29, "checkRange", [43387, 43438, 59]), -1);

// memory_copy.wast:5522
assert_return(() => call($29, "checkRange", [43438, 43447, 77]), -1);

// memory_copy.wast:5524
assert_return(() => call($29, "checkRange", [43447, 43466, 41]), -1);

// memory_copy.wast:5526
assert_return(() => call($29, "checkRange", [43466, 44129, 83]), -1);

// memory_copy.wast:5528
assert_return(() => call($29, "checkRange", [44129, 44958, 96]), -1);

// memory_copy.wast:5530
assert_return(() => call($29, "checkRange", [44958, 45570, 77]), -1);

// memory_copy.wast:5532
assert_return(() => call($29, "checkRange", [45570, 45575, 92]), -1);

// memory_copy.wast:5534
assert_return(() => call($29, "checkRange", [45575, 45640, 77]), -1);

// memory_copy.wast:5536
assert_return(() => call($29, "checkRange", [45640, 45742, 42]), -1);

// memory_copy.wast:5538
assert_return(() => call($29, "checkRange", [45742, 45832, 72]), -1);

// memory_copy.wast:5540
assert_return(() => call($29, "checkRange", [45832, 45999, 92]), -1);

// memory_copy.wast:5542
assert_return(() => call($29, "checkRange", [45999, 46384, 83]), -1);

// memory_copy.wast:5544
assert_return(() => call($29, "checkRange", [46384, 46596, 95]), -1);

// memory_copy.wast:5546
assert_return(() => call($29, "checkRange", [46596, 46654, 92]), -1);

// memory_copy.wast:5548
assert_return(() => call($29, "checkRange", [46654, 47515, 83]), -1);

// memory_copy.wast:5550
assert_return(() => call($29, "checkRange", [47515, 47620, 77]), -1);

// memory_copy.wast:5552
assert_return(() => call($29, "checkRange", [47620, 47817, 79]), -1);

// memory_copy.wast:5554
assert_return(() => call($29, "checkRange", [47817, 47951, 95]), -1);

// memory_copy.wast:5556
assert_return(() => call($29, "checkRange", [47951, 48632, 100]), -1);

// memory_copy.wast:5558
assert_return(() => call($29, "checkRange", [48632, 48699, 97]), -1);

// memory_copy.wast:5560
assert_return(() => call($29, "checkRange", [48699, 48703, 37]), -1);

// memory_copy.wast:5562
assert_return(() => call($29, "checkRange", [48703, 49764, 77]), -1);

// memory_copy.wast:5564
assert_return(() => call($29, "checkRange", [49764, 49955, 42]), -1);

// memory_copy.wast:5566
assert_return(() => call($29, "checkRange", [49955, 50012, 76]), -1);

// memory_copy.wast:5568
assert_return(() => call($29, "checkRange", [50012, 50036, 57]), -1);

// memory_copy.wast:5570
assert_return(() => call($29, "checkRange", [50036, 50087, 59]), -1);

// memory_copy.wast:5572
assert_return(() => call($29, "checkRange", [50087, 50096, 77]), -1);

// memory_copy.wast:5574
assert_return(() => call($29, "checkRange", [50096, 50115, 41]), -1);

// memory_copy.wast:5576
assert_return(() => call($29, "checkRange", [50115, 50370, 83]), -1);

// memory_copy.wast:5578
assert_return(() => call($29, "checkRange", [50370, 51358, 92]), -1);

// memory_copy.wast:5580
assert_return(() => call($29, "checkRange", [51358, 51610, 77]), -1);

// memory_copy.wast:5582
assert_return(() => call($29, "checkRange", [51610, 51776, 83]), -1);

// memory_copy.wast:5584
assert_return(() => call($29, "checkRange", [51776, 51833, 89]), -1);

// memory_copy.wast:5586
assert_return(() => call($29, "checkRange", [51833, 52895, 100]), -1);

// memory_copy.wast:5588
assert_return(() => call($29, "checkRange", [52895, 53029, 97]), -1);

// memory_copy.wast:5590
assert_return(() => call($29, "checkRange", [53029, 53244, 68]), -1);

// memory_copy.wast:5592
assert_return(() => call($29, "checkRange", [53244, 54066, 100]), -1);

// memory_copy.wast:5594
assert_return(() => call($29, "checkRange", [54066, 54133, 97]), -1);

// memory_copy.wast:5596
assert_return(() => call($29, "checkRange", [54133, 54137, 37]), -1);

// memory_copy.wast:5598
assert_return(() => call($29, "checkRange", [54137, 55198, 77]), -1);

// memory_copy.wast:5600
assert_return(() => call($29, "checkRange", [55198, 55389, 42]), -1);

// memory_copy.wast:5602
assert_return(() => call($29, "checkRange", [55389, 55446, 76]), -1);

// memory_copy.wast:5604
assert_return(() => call($29, "checkRange", [55446, 55470, 57]), -1);

// memory_copy.wast:5606
assert_return(() => call($29, "checkRange", [55470, 55521, 59]), -1);

// memory_copy.wast:5608
assert_return(() => call($29, "checkRange", [55521, 55530, 77]), -1);

// memory_copy.wast:5610
assert_return(() => call($29, "checkRange", [55530, 55549, 41]), -1);

// memory_copy.wast:5612
assert_return(() => call($29, "checkRange", [55549, 56212, 83]), -1);

// memory_copy.wast:5614
assert_return(() => call($29, "checkRange", [56212, 57048, 96]), -1);

// memory_copy.wast:5616
assert_return(() => call($29, "checkRange", [57048, 58183, 77]), -1);

// memory_copy.wast:5618
assert_return(() => call($29, "checkRange", [58183, 58202, 41]), -1);

// memory_copy.wast:5620
assert_return(() => call($29, "checkRange", [58202, 58516, 83]), -1);

// memory_copy.wast:5622
assert_return(() => call($29, "checkRange", [58516, 58835, 95]), -1);

// memory_copy.wast:5624
assert_return(() => call($29, "checkRange", [58835, 58855, 77]), -1);

// memory_copy.wast:5626
assert_return(() => call($29, "checkRange", [58855, 59089, 95]), -1);

// memory_copy.wast:5628
assert_return(() => call($29, "checkRange", [59089, 59145, 77]), -1);

// memory_copy.wast:5630
assert_return(() => call($29, "checkRange", [59145, 59677, 99]), -1);

// memory_copy.wast:5632
assert_return(() => call($29, "checkRange", [59677, 60134, 0]), -1);

// memory_copy.wast:5634
assert_return(() => call($29, "checkRange", [60134, 60502, 89]), -1);

// memory_copy.wast:5636
assert_return(() => call($29, "checkRange", [60502, 60594, 59]), -1);

// memory_copy.wast:5638
assert_return(() => call($29, "checkRange", [60594, 60617, 36]), -1);

// memory_copy.wast:5640
assert_return(() => call($29, "checkRange", [60617, 60618, 32]), -1);

// memory_copy.wast:5642
assert_return(() => call($29, "checkRange", [60618, 60777, 42]), -1);

// memory_copy.wast:5644
assert_return(() => call($29, "checkRange", [60777, 60834, 76]), -1);

// memory_copy.wast:5646
assert_return(() => call($29, "checkRange", [60834, 60858, 57]), -1);

// memory_copy.wast:5648
assert_return(() => call($29, "checkRange", [60858, 60909, 59]), -1);

// memory_copy.wast:5650
assert_return(() => call($29, "checkRange", [60909, 60918, 77]), -1);

// memory_copy.wast:5652
assert_return(() => call($29, "checkRange", [60918, 60937, 41]), -1);

// memory_copy.wast:5654
assert_return(() => call($29, "checkRange", [60937, 61600, 83]), -1);

// memory_copy.wast:5656
assert_return(() => call($29, "checkRange", [61600, 62436, 96]), -1);

// memory_copy.wast:5658
assert_return(() => call($29, "checkRange", [62436, 63307, 77]), -1);

// memory_copy.wast:5660
assert_return(() => call($29, "checkRange", [63307, 63397, 100]), -1);

// memory_copy.wast:5662
assert_return(() => call($29, "checkRange", [63397, 63501, 74]), -1);

// memory_copy.wast:5664
assert_return(() => call($29, "checkRange", [63501, 63525, 93]), -1);

// memory_copy.wast:5666
assert_return(() => call($29, "checkRange", [63525, 63605, 74]), -1);

// memory_copy.wast:5668
assert_return(() => call($29, "checkRange", [63605, 63704, 100]), -1);

// memory_copy.wast:5670
assert_return(() => call($29, "checkRange", [63704, 63771, 97]), -1);

// memory_copy.wast:5672
assert_return(() => call($29, "checkRange", [63771, 63775, 37]), -1);

// memory_copy.wast:5674
assert_return(() => call($29, "checkRange", [63775, 64311, 77]), -1);

// memory_copy.wast:5676
assert_return(() => call($29, "checkRange", [64311, 64331, 26]), -1);

// memory_copy.wast:5678
assert_return(() => call($29, "checkRange", [64331, 64518, 92]), -1);

// memory_copy.wast:5680
assert_return(() => call($29, "checkRange", [64518, 64827, 11]), -1);

// memory_copy.wast:5682
assert_return(() => call($29, "checkRange", [64827, 64834, 26]), -1);

// memory_copy.wast:5684
assert_return(() => call($29, "checkRange", [64834, 65536, 0]), -1);
