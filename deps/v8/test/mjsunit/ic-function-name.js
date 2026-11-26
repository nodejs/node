// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function getName(fn) {
  return fn.name;
}

function getKeyedName(fn, k) {
  return fn[k];
}

function hasName(fn) {
  return 'name' in fn;
}

// Construct a function with at least kScopeInfoMaxInlinedLocalNamesSize (75)
// context-allocated locals so that its ScopeInfo uses a hashtable for local
// names.
function makeManyContextLocalsFn(n) {
  let decls = [];
  let refs = [];
  for (let i = 0; i < n; i++) {
    decls.push(`let v${i} = ${i};`);
    refs.push(`v${i}`);
  }
  const src = `
    return function manyLocalsFn() {
      ${decls.join('\n')}
      return function inner() { return ${refs.join(' + ')}; };
    };
  `;
  return new Function(src)();
}

function makeNamedExprManyLocals(n) {
  let decls = [];
  let refs = [];
  for (let i = 0; i < n; i++) {
    decls.push(`let v${i} = ${i};`);
    refs.push(`v${i}`);
  }
  const src = `
    return (function selfManyLocals() {
      ${decls.join('\n')}
      return function inner() {
        return selfManyLocals.length + ${refs.join(' + ')};
      };
    });
  `;
  return new Function(src)();
}

function makeClassManyLocals(n) {
  let decls = [];
  let refs = [];
  for (let i = 0; i < n; i++) {
    decls.push(`let v${i} = ${i};`);
    refs.push(`v${i}`);
  }
  const src = `
    return class ManyLocalsClass {
      constructor() {
        ${decls.join('\n')}
        this.f = () => ${refs.join(' + ')};
      }
    };
  `;
  return new Function(src)();
}

// Derived class hierarchy to force megamorphic JSFunction maps (each derived
// class has a distinct [[Prototype]] = superclass constructor).
class Base {}
const derivedClasses = [Base];
for (let i = 0; i < 12; i++) {
  const Parent = derivedClasses[derivedClasses.length - 1];
  const Child = new Function('Parent', `
    return class Derived${i} extends Parent {
      constructor() { super(); }
    };
  `)(Parent);
  derivedClasses.push(Child);
}

function regularNamed() {}
const anonExpr = (0, function() {});
const inferredExpr = function() {};
const inferredArrow = () => {};
const dynamicFn = new Function('return 1;');
const selfRefExpr = function selfRef() {
  return selfRef;
};
function withSmallContextLocals() {
  let a = 1, b = 2;
  return () => a + b;
}
// The largest ScopeInfo with inlined local names and the smallest one with a
// hashtable.
const withMaxInlinedLocals = makeManyContextLocalsFn(74);
const withMinHashtableLocals = makeManyContextLocalsFn(75);
const withManyLocals = makeManyContextLocalsFn(80);
const namedExprManyLocals = makeNamedExprManyLocals(80);
const classManyLocals = makeClassManyLocals(80);

function reconfiguredFn() {}
Object.defineProperty(reconfiguredFn, 'name', {value: 'customName'});

function deletedNameFn() {}
delete deletedNameFn.name;

class StaticNameClass {
  static name = 'staticOverride';
}

const boundFn = regularNamed.bind(null);
const protoInherited = Object.create(regularNamed);

const getterFn =
    Object.getOwnPropertyDescriptor({get myProp() {}}, 'myProp').get;
const setterFn =
    Object.getOwnPropertyDescriptor({set myProp(v) {}}, 'myProp').set;
async function asyncFn() {}
function* genFn() {}
async function* asyncGenFn() {}

const cases = [
  [regularNamed, 'regularNamed'],
  [anonExpr, ''],
  [inferredExpr, 'inferredExpr'],
  [inferredArrow, 'inferredArrow'],
  [dynamicFn, 'anonymous'],
  [Function.prototype, ''],
  [selfRefExpr, 'selfRef'],
  [withSmallContextLocals, 'withSmallContextLocals'],
  [withMaxInlinedLocals, 'manyLocalsFn'],
  [withMinHashtableLocals, 'manyLocalsFn'],
  [withManyLocals, 'manyLocalsFn'],
  [namedExprManyLocals, 'selfManyLocals'],
  [classManyLocals, 'ManyLocalsClass'],
  [reconfiguredFn, 'customName'],
  [deletedNameFn, ''],
  [StaticNameClass, 'staticOverride'],
  [boundFn, 'bound regularNamed'],
  [protoInherited, 'regularNamed'],
  [Math.sin, 'sin'],
  [Array.prototype.push, 'push'],
  [getterFn, 'get myProp'],
  [setterFn, 'set myProp'],
  [asyncFn, 'asyncFn'],
  [genFn, 'genFn'],
  [asyncGenFn, 'asyncGenFn'],
  [Base, 'Base'],
];
for (let i = 0; i < 12; i++) {
  cases.push([derivedClasses[i + 1], `Derived${i}`]);
}

function checkAll() {
  for (const [fn, expected] of cases) {
    assertEquals(expected, getName(fn));
    assertEquals(expected, getKeyedName(fn, 'name'));
    assertEquals(true, hasName(fn));
  }
}

// Warm up ICs into megamorphic state across Ignition, Sparkplug, Maglev, TF.
%PrepareFunctionForOptimization(getName);
%PrepareFunctionForOptimization(getKeyedName);
%PrepareFunctionForOptimization(hasName);
for (let i = 0; i < 5; i++) checkAll();
%OptimizeFunctionOnNextCall(getName);
%OptimizeFunctionOnNextCall(getKeyedName);
%OptimizeFunctionOnNextCall(hasName);
checkAll();

// Compiling a function moves its name from the SharedFunctionInfo into the
// ScopeInfo. Most of the functions above are still uncompiled; compile them
// and check again.
const uncompiledFns = [
  regularNamed, anonExpr, inferredExpr, inferredArrow, dynamicFn, selfRefExpr,
  withSmallContextLocals, withMaxInlinedLocals, withMinHashtableLocals,
  withManyLocals, namedExprManyLocals, reconfiguredFn, deletedNameFn, getterFn,
  setterFn, asyncFn, genFn, asyncGenFn
];
for (const fn of uncompiledFns) fn();
for (const c of [classManyLocals, StaticNameClass, ...derivedClasses]) new c();
checkAll();

// Monomorphic load site (should also use LoadIC_FunctionName).
function monoName(fn) {
  return fn.name;
}
%PrepareFunctionForOptimization(monoName);
assertEquals('regularNamed', monoName(regularNamed));
assertEquals('withSmallContextLocals', monoName(withSmallContextLocals));
assertEquals('manyLocalsFn', monoName(withManyLocals));
assertEquals('anonymous', monoName(dynamicFn));
assertEquals('', monoName(anonExpr));
%OptimizeFunctionOnNextCall(monoName);
assertEquals('regularNamed', monoName(regularNamed));
assertEquals('manyLocalsFn', monoName(withManyLocals));
assertEquals('anonymous', monoName(dynamicFn));
assertEquals('customName', monoName(reconfiguredFn));

// super.name in a static method (LoadSuperIC).
class SuperBase {}
class SuperSub extends SuperBase {
  static getSuperName() {
    return super.name;
  }
}
%PrepareFunctionForOptimization(SuperSub.getSuperName);
for (let i = 0; i < 5; i++) assertEquals('SuperBase', SuperSub.getSuperName());
%OptimizeFunctionOnNextCall(SuperSub.getSuperName);
assertEquals('SuperBase', SuperSub.getSuperName());

// Reconfiguring .name after caching must not return the old name.
function willMutate() {}
assertEquals('willMutate', getName(willMutate));
Object.defineProperty(willMutate, 'name', {value: 'mutated'});
assertEquals('mutated', getName(willMutate));
assertEquals('mutated', getKeyedName(willMutate, 'name'));

// Deleting .name after caching must fall back to Function.prototype.name ("").
function willDelete() {}
assertEquals('willDelete', getName(willDelete));
delete willDelete.name;
assertEquals('', getName(willDelete));
assertEquals(true, hasName(willDelete));
