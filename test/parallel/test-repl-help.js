'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');

// Tests for `.help [expression]`: without an argument it prints the command
// listing; with an argument it evaluates the expression in the REPL context
// and prints introspection info (type, signature, properties, methods).

function startRepl(options = {}) {
  const input = new stream.PassThrough();
  const output = new stream.PassThrough();
  let data = '';
  output.on('data', (chunk) => { data += chunk; });

  const server = repl.start({
    input,
    output,
    prompt: '> ',
    terminal: false,
    useColors: false,
    useGlobal: false,
    ...options,
  });

  return {
    server,
    input,
    getOutput() { return data; },
    resetOutput() { data = ''; },
  };
}

// Wait until the REPL output stops changing for a few event loop turns.
async function settle(getOutput) {
  let prev;
  let stable = 0;
  do {
    prev = getOutput();
    await new Promise((resolve) => setImmediate(resolve));
    stable = getOutput() === prev ? stable + 1 : 0;
  } while (stable < 5);
}

async function run(ctx, line) {
  ctx.resetOutput();
  ctx.input.write(`${line}\n`);
  await settle(ctx.getOutput);
  return ctx.getOutput();
}

(async () => {
  // No `help` function is injected into the REPL context.
  {
    const ctx = startRepl();
    assert.strictEqual('help' in ctx.server.context, false);
    const out = await run(ctx, 'typeof help');
    assert.match(out, /'undefined'/);
    ctx.server.close();
  }

  // Bare `.help` prints the command listing (existing behavior).
  {
    const ctx = startRepl();
    const out = await run(ctx, '.help');
    assert.match(out, /\.break/);
    assert.match(out, /\.exit/);
    assert.match(out, /\.load/);
    assert.match(out, /Press Ctrl\+C to abort current expression/);
    ctx.server.close();
  }

  // `.help` with trailing whitespace behaves like bare `.help`.
  {
    const ctx = startRepl();
    const out = await run(ctx, '.help   ');
    assert.match(out, /\.break/);
    assert.match(out, /\.exit/);
    ctx.server.close();
  }

  // `.help Array` shows class info with prototype methods.
  {
    const ctx = startRepl();
    const out = await run(ctx, '.help Array');
    assert.match(out, /Function: Array/);
    assert.match(out, /Prototype methods:/);
    assert.match(out, /map/);
    // Introspection output, not the command listing.
    assert.doesNotMatch(out, /\.break/);
    ctx.server.close();
  }

  // `.help <fn>` shows function info with parameters.
  {
    const ctx = startRepl();
    ctx.server.context.greet =
      function greet(name, greeting) { return `${greeting} ${name}`; };
    const out = await run(ctx, '.help greet');
    assert.match(out, /Function/);
    assert.match(out, /greet/);
    assert.match(out, /name, greeting/);
    ctx.server.close();
  }

  // `.help <obj>` categorizes methods and properties.
  {
    const ctx = startRepl();
    ctx.server.context.obj = {
      x: 1,
      y: 'hello',
      doSomething() { return true; },
    };
    const out = await run(ctx, '.help obj');
    assert.match(out, /Methods:/);
    assert.match(out, /doSomething\(\)/);
    assert.match(out, /Properties:/);
    assert.match(out, /x:/);
    ctx.server.close();
  }

  // `.help <class>` shows class info with prototype methods.
  {
    const ctx = startRepl();
    ctx.server.context.Animal = class Animal {
      constructor(name) { this.name = name; }
      speak() { return `${this.name} speaks`; }
      eat() { return 'eating'; }
    };
    const out = await run(ctx, '.help Animal');
    assert.match(out, /Class/);
    assert.match(out, /Animal/);
    assert.match(out, /Prototype methods:/);
    assert.match(out, /speak/);
    assert.match(out, /eat/);
    ctx.server.close();
  }

  // `.help null` and `.help undefined` don't crash.
  {
    const ctx = startRepl();
    let out = await run(ctx, '.help null');
    assert.match(out, /null/);
    out = await run(ctx, '.help undefined');
    assert.match(out, /undefined/);
    ctx.server.close();
  }

  // `.help 42` shows primitive info.
  {
    const ctx = startRepl();
    const out = await run(ctx, '.help 42');
    assert.match(out, /number/);
    assert.match(out, /42/);
    ctx.server.close();
  }

  // `.help {x: 1}` evaluates an object literal expression.
  {
    const ctx = startRepl();
    const out = await run(ctx, '.help {x: 1}');
    assert.match(out, /Properties:/);
    assert.match(out, /x:/);
    ctx.server.close();
  }

  // `.help <asyncFn>` shows AsyncFunction kind.
  {
    const ctx = startRepl();
    ctx.server.context.fetchData = async function fetchData(url) { return url; };
    const out = await run(ctx, '.help fetchData');
    assert.match(out, /AsyncFunction/);
    assert.match(out, /fetchData/);
    ctx.server.close();
  }

  // `.help <generatorFn>` shows GeneratorFunction kind.
  {
    const ctx = startRepl();
    ctx.server.context.gen = function* gen() { yield 1; };
    const out = await run(ctx, '.help gen');
    assert.match(out, /GeneratorFunction/);
    assert.match(out, /gen/);
    ctx.server.close();
  }

  // `.help <asyncGeneratorFn>` shows AsyncGeneratorFunction kind.
  {
    const ctx = startRepl();
    ctx.server.context.asyncGen = async function* asyncGen() { yield 1; };
    const out = await run(ctx, '.help asyncGen');
    assert.match(out, /AsyncGeneratorFunction/);
    assert.match(out, /asyncGen/);
    ctx.server.close();
  }

  // `.help <anonymousFn>` shows '(anonymous)'.
  {
    const ctx = startRepl();
    ctx.server.context.anon = (() => { return function() {}; })();
    const out = await run(ctx, '.help anon');
    assert.match(out, /Function/);
    assert.match(out, /\(anonymous\)/);
    ctx.server.close();
  }

  // `.help <arrowFn>` — arrow function without prototype or parameters.
  {
    const ctx = startRepl();
    const noop = () => {};
    ctx.server.context.noop = noop;
    const out = await run(ctx, '.help noop');
    assert.match(out, /Function/);
    assert.match(out, /noop/);
    assert.doesNotMatch(out, /Parameters:/);
    assert.doesNotMatch(out, /Prototype methods:/);
    ctx.server.close();
  }

  // `.help <fn>` with no parameters omits the Parameters line.
  {
    const ctx = startRepl();
    ctx.server.context.noParams = function noParams() { return 1; };
    const out = await run(ctx, '.help noParams');
    assert.match(out, /Function/);
    assert.match(out, /noParams/);
    assert.doesNotMatch(out, /Parameters:/);
    ctx.server.close();
  }

  // `.help <fn>` whose prototype only has `constructor` omits methods.
  {
    const ctx = startRepl();
    ctx.server.context.Bare = function Bare() {};
    const out = await run(ctx, '.help Bare');
    assert.match(out, /Function/);
    assert.match(out, /Bare/);
    assert.doesNotMatch(out, /Prototype methods:/);
    ctx.server.close();
  }

  // `.help <obj>` with getters and setters shows Accessors.
  {
    const ctx = startRepl();
    const obj = {};
    Object.defineProperty(obj, 'value', {
      get() { return 42; },
      set(_v) { /* noop */ },
      enumerable: true,
      configurable: true,
    });
    Object.defineProperty(obj, 'readOnly', {
      get() { return 'hi'; },
      enumerable: true,
      configurable: true,
    });
    ctx.server.context.accessors = obj;
    const out = await run(ctx, '.help accessors');
    assert.match(out, /Accessors:/);
    assert.match(out, /value/);
    assert.match(out, /get\/set/);
    assert.match(out, /readOnly/);
    ctx.server.close();
  }

  // `.help <obj>` shows the prototype chain for subclassed objects.
  {
    const ctx = startRepl();
    class Base { base() {} }
    class Child extends Base { child() {} }
    ctx.server.context.instance = new Child();
    const out = await run(ctx, '.help instance');
    assert.match(out, /Prototype chain:/);
    assert.match(out, /Child/);
    assert.match(out, /Base/);
    ctx.server.close();
  }

  // `.help <obj>` handles objects that throw on property access.
  {
    const ctx = startRepl();
    ctx.server.context.throwing = new Proxy({}, {
      getOwnPropertyDescriptor() { throw new Error('no access'); },
      ownKeys() { throw new Error('no keys'); },
    });
    const out = await run(ctx, '.help throwing');
    assert.match(out, /\(properties not enumerable\)/);
    ctx.server.close();
  }

  // `.help <emptyObj>` prints an empty body without category headers.
  {
    const ctx = startRepl();
    ctx.server.context.empty = {};
    const out = await run(ctx, '.help empty');
    assert.match(out, /Object \{/);
    assert.doesNotMatch(out, /Methods:/);
    assert.doesNotMatch(out, /Accessors:/);
    assert.doesNotMatch(out, /Properties:/);
    ctx.server.close();
  }

  // `.help <undefinedVariable>` reports the error and keeps the REPL usable.
  {
    const ctx = startRepl();
    let out = await run(ctx, '.help doesNotExist');
    assert.match(out, /doesNotExist is not defined/);
    // The REPL still works afterwards.
    out = await run(ctx, '1 + 1');
    assert.match(out, /2/);
    ctx.server.close();
  }

  // `.help 42` with useColors=true emits ANSI escape codes.
  {
    const ctx = startRepl({ useColors: true });
    const out = await run(ctx, '.help 42');
    assert.match(out, /number/);
    assert.match(out, /42/);
    assert.ok(out.includes('\x1b['));
    ctx.server.close();
  }
})().then(common.mustCall());
