// Show the difference between calling a V8 binding C++ function
// relative to a comparable N-API C++ function,
// in various types/numbers of arguments.
// Reports n of calls per second.
'use strict';

const common = require('../../common.js');

let v8;
let napi;

try {
  v8 = require(`./build/${common.buildType}/binding`);
} catch {
  console.error(`${__filename}: V8 Binding failed to load`);
  process.exit(0);
}

try {
  napi = require(`./build/${common.buildType}/napi_binding`);
} catch {
  console.error(`${__filename}: NAPI-Binding failed to load`);
  process.exit(0);
}

const argsTypes = ['String', 'Number', 'Object', 'Array', 'Typedarray',
                   '10Numbers', '100Numbers', '1000Numbers'];

const generateArgs = (argType) => {
  let args = [];

  if (argType === 'String') {
    args.push('The quick brown fox jumps over the lazy dog');
  } else if (argType === 'LongString') {
    args.push(Buffer.alloc(32768, '42').toString());
  } else if (argType === 'Number') {
    args.push(Math.floor(314158964 * Math.random()));
  } else if (argType === 'Object') {
    args.push({
      map: 'add',
      operand: 10,
      data: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
      reduce: 'add',
    });
  } else if (argType === 'Array') {
    const arr = [];
    for (let i = 0; i < 50; ++i) {
      arr.push(Math.random() * 10e9);
    }
    args.push(arr);
  } else if (argType === 'Typedarray') {
    const arr = new Uint32Array(1000);
    for (let i = 0; i < 1000; ++i) {
      arr[i] = Math.random() * 4294967296;
    }
    args.push(arr);
  } else if (argType === '10Numbers') {
    args.push(10);
    for (let i = 0; i < 9; ++i) {
      args = [...args, ...generateArgs('Number')];
    }
  } else if (argType === '100Numbers') {
    args.push(100);
    for (let i = 0; i < 99; ++i) {
      args = [...args, ...generateArgs('Number')];
    }
  } else if (argType === '1000Numbers') {
    args.push(1000);
    for (let i = 0; i < 999; ++i) {
      args = [...args, ...generateArgs('Number')];
    }
  }

  return args;
};

const bench = common.createBenchmark(main, {
  type: argsTypes,
  engine: ['v8', 'napi'],
  n: [1, 1e1, 1e2, 1e3, 1e4, 1e5],
});

function main({ n, engine, type }) {
  const bindings = engine === 'v8' ? v8 : napi;
  const methodName = 'callWith' + type;
  const fn = bindings[methodName];

  if (fn) {
    const args = generateArgs(type);

    bench.start();
    for (var i = 0; i < n; i++) {
      fn.apply(null, args);
    }
    bench.end(n);
  }
}
