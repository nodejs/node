'use strict';

const stringValue = 'hello';
const booleanValue = true;
const undefinedValue = undefined;
const nullValue = null;
const nanValue = NaN;
const bigintValue = 1n;
const symbolValue = Symbol('tag');
const functionValue = () => 1;
const objectValue = { alpha: 1, beta: 'two' };
const arrayValue = [1, 'two', 3];
const errorValue = new Error('boom');
errorValue.stack = 'Error: boom';

function consume() {}
consume();

// TODO(joyeecheung): add a test for evaluation errors. This can be
// somewhat flaky on slow CI machines.
// const holder = new class Foo {
//   get throwingGetter() {
//     throw new Error('foo');
//   }
// }
// probe holder.throwingGetter
