'use strict';

const common = require('../common.js');
const util = require('util');

const A = Symbol('a');
const B = Symbol('b');

const bench = common.createBenchmark(main, {
  type: ['class',
         'function',
         'function-new',
         'function-new-instanceof',
         'function-new-target'],
  millions: [10]
});

class FooBase {
  constructor(a) {
    this[A] = a;
  }

  test(a, b, c) {}

  get a() {
    return this[A];
  }
}

class Foo extends FooBase {
  constructor(a, b, c) {
    super(a);
    this[B] = b;
    this.c = c;
  }

  test2(a, b, c) {
  }

  test3() {}

  get b() {
    return this[B];
  }
}

function BarBase(a, newcheck) {
  if (newcheck && !(this instanceof BarBase))
    return new BarBase(a);
  this[A] = a;
}
BarBase.prototype.test = function(a, b, c) {};

Object.defineProperty(BarBase.prototype, 'a', {
  get: function() { return this[A]; }
});

function Bar(a, b, c, newcheck) {
  if (newcheck && !(this instanceof Bar))
    return new Bar(a, b, c);
  BarBase.call(this, a);
  this[B] = b;
  this.c = c;
}
util.inherits(Bar, BarBase);
Bar.prototype.test2 = function(a, b, c) {};
Bar.prototype.test3 = function() {};

Object.defineProperty(Bar.prototype, 'b', {
  get: function() { return this[B]; }
});

function BazBase(a, newcheck) {
  if (new.target === undefined)
    return new BazBase(a);
  this[A] = a;
}
BazBase.prototype.test = function(a, b, c) {};

Object.defineProperty(BazBase.prototype, 'a', {
  get: function() { return this[A]; }
});

function Baz(a, b, c) {
  if (new.target === undefined)
    return new Baz(a, b, c);
  BazBase.call(this, a);
  this[B] = b;
  this.c = c;
}
util.inherits(Baz, BazBase);
Baz.prototype.test2 = function(a, b, c) {};
Baz.prototype.test3 = function() {};

Object.defineProperty(Baz.prototype, 'b', {
  get: function() { return this[B]; }
});

function runClass(n, obj) {
  var i;
  bench.start();
  for (i = 0; i < n; i++)
    exercise(new Foo(1, 2, 3));
  bench.end(n / 1e6);
}

function runFunctionWithNew(n, newcheck) {
  var i;
  bench.start();
  for (i = 0; i < n; i++)
    exercise(new Bar(1, 2, 3, newcheck));
  bench.end(n / 1e6);
}

function runFunctionWithoutNew(n, newcheck) {
  var i;
  bench.start();
  for (i = 0; i < n; i++)
    exercise(Bar(1, 2, 3, true))
  bench.end(n / 1e6);
}

function runFunctionWithNewTarget(n) {
  var i;
  bench.start();
  for (i = 0; i < n; i++)
    exercise(new Baz(1, 2, 3));
  bench.end(n / 1e6);
}

function exercise(obj) {
  obj.test();
  obj.test2();
  obj.test3();
  obj.a;
  obj.b;
  obj.c;
}

function main(conf) {
  const n = +conf.millions * 1e6;

  switch (conf.type) {
    case 'class':
      runClass(n);
      break;
    case 'function':
      runFunctionWithoutNew(n);
      break;
    case 'function-new':
      runFunctionWithNew(n);
      break;
    case 'function-new-instanceof':
      runFunctionWithNew(n, true);
      break;
    case 'function-new-target':
      runFunctionWithNewTarget(n);
      break;
    default:
      throw new Error('Unexpected type');
  }
}
