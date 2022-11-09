'use strict';
require('../common');

const assert = require('assert');

const original = {
  name: 'Test'
};

const clone = structuredClone(original);
assert.deepStrictEqual(clone, original);


// #region Test process.env

const { env } = process;
const envClone = structuredClone(process.env);

assert.notStrictEqual(env, envClone); // Clones has diferent identity

for (const key in env) {
  assert.strictEqual(env[key], envClone[key]); // Values should be strict equal
}

// #endregion Test process.env


const source = { child: 'NodeJs' };
source.itself = source;

const cloneOfSource = structuredClone(source);

assert.notStrictEqual(source, cloneOfSource); // Objects doesn't have same identity

assert.strictEqual(cloneOfSource.child, 'NodeJs');
assert.deepStrictEqual(cloneOfSource, cloneOfSource.itself); // No changes in circular refernce

// String
const str = 'NodeJs';
const cloneOfStr = structuredClone(str);
assert.strictEqual(str, cloneOfStr);

// Number
const num = 1;
const cloneOfNum = structuredClone(num);
assert.strictEqual(num, cloneOfNum);

// Empty array
const array = [];
const cloneOfArray = structuredClone(array);
assert.strictEqual(array.length, cloneOfArray.length);

// Boolean
const bool = true;
const cloneOfBool = structuredClone(bool);
assert.strictEqual(bool, cloneOfBool);

// Non empty array
const nonEmptyArray = [1, 2, 3, 4, 5, 'A', 'B'];
const cloneOfNonEmptyArray = structuredClone(nonEmptyArray);

nonEmptyArray.forEach((val, i) => {
  assert.strictEqual(val, cloneOfNonEmptyArray[i]);
});

// Function
const fn = () => { };

try {
  structuredClone(fn);
} catch (err) {
  assert.match(err.message, /could not be clone/);
}

// Class
class ClonableClass {}

try {
  structuredClone(ClonableClass);
} catch (err) {
  assert.match(err.message, /could not be clone/);
}

// Class Object

const objOfClonableClass = new ClonableClass();

try {
  structuredClone(objOfClonableClass);
  assert.ok(true);
} catch {
  assert.fail('Should not throw any errors');
}
