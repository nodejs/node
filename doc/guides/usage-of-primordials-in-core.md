# Usage of primordials in core

The file `lib/internal/per_context/primordials.js` subclasses and stores the JS
builtins that come from the VM so that Node.js builtin modules do not need to
later look these up from the global proxy, which can be mutated by users.

Usage of primordials should be preferred for any new code, but replacing current
code with primordials should be done with care.

## How to access primordials

The primordials is meant for internal use only, and is only accessible for
internal core modules. User-code cannot use or rely on primordials, it usually
fine to rely on ECMAScript built-ins and assume that it will behave as specd.

If you'd like to access the `primordials` object to help you on the Node.js
development or for tinkering, you can make it globally available:

```bash
node --expose-internals -r internal/test/binding
```

## What is in primordials

### Properties of the global object

Object and functions on global objects can be deleted or replaced, using them
from primordials makes the code more reliable:

```js
globalThis.Array === primordials.Array; // true

globalThis.Array = function() {
  return [1, 2, 3];
};
globalThis.Array === primordials.Array; // false

primordials.Array(0); // []
globalThis.Array(0); // [1,2,3]
```

### Prototype methods

ECMAScript provides a bunch of methods available on builtin objects that are
used to interact with JavaScript objects.

```js
const array = [1, 2, 3];
array.push(4); // Here `push` refers to %Array.prototype.push%.
console.log(array); // [1,2,3,4]

// %Array.prototype%.push is modified in userland.
Array.prototype.push = function push(val) {
  return this.unshift(val);
};

array.push(5); // Now `push` refers to the modified method.
console.log(array); // [5,1,2,3,4]
```

Primordials save a reference to the original method that takes `this` as its
first argument:

```js
const {
  ArrayPrototypePush,
} = primordials;

const array = [1, 2, 3];
ArrayPrototypePush(array, 4);
console.log(array); // [1,2,3,4]

Array.prototype.push = function push(vush) {
  return this.unshift(val);
};

ArrayPrototypePush(array, 5);
console.log(array); // [1,2,3,4,5]
```

### Safe classes

Safe classes are classes that provide the same API as their equivalent class,
but whose implementation aims to avoid any reliance on user-mutable code.

### Variadic functions

There are some built-in functions that accepts a variable number of arguments
(E.G.: `Math.max`, `%Array.prototype.push%`), it is sometimes useful to provide
the list of arguments as an array. You can use function with the suffix `Apply`
(E.G.: `MathMaxApply`, `FunctionPrototypePushApply`) to do that.

## Primordials with known performance issues

One of the reason the current Node.js API is not temper-proof today is
performance: sometimes use of primordials removes some V8 optimisations which,
when on a hot path, could significantly decrease the performance of Node.js.

* Methods that mutates the internal state of arrays:
  * `ArrayPrototypePush`
  * `ArrayPrototypePop`
  * `ArrayPrototypeShift`
  * `ArrayPrototypeUnshift`
* Methods of the function prototype:
  * `FunctionPrototypeBind`: use with caution
  * `FunctionPrototypeCall`: creates perf pitfall when used to invoke super
    constructor.
  * `FunctionPrototype`: use `() => {}` instead when referencing a no-op
    function.
* `SafeArrayIterator`

In general, when sending or reviewing a PR that touches a hot path, use extra
caution and run extensive benchmarks.

## How to avoid unsafe array iteration

There are many usual practices in JavaScript that rely on iteration.

<details>

<summary>Avoid `for(of)` loops on arrays</summary>

```js
for (const item of array) {
  console.log(item);
}
```

This is code can be de-sugared to:

```js
{
  // 1. Lookup @@iterator property on `array` (user-mutable if user provided).
  // 2. Lookup @@iterator property on %Array.prototype% (user-mutable).
  // 3. Call that function.
  const iterator = array[Symbol.iterator]();
  // 1. Lookup `next` property on `iterator` (doesn't exist).
  // 2. Lookup `next` property on %ArrayIteratorPrototype% (user-mutable).
  // 3. Call that function.
  let { done, value: item } = iterator.next();
  while (!done) {
    console.log(item);
    // Repeat.
    ({ done, value: item } = iterator.next());
  }
}
```

Instead, you can use the old-style but still very performant `for(;;)` loop:

```js
for (let i = 0; i < array.length; i++) {
  console.log(item);
}
```

This only applies if you are working with a genuine array (or array-like
object), if you are instead expecting any iterator, `for(of)` loop may be the
correct syntax to use.

<details>

<summary>Avoid array destructuring assignment on arrays</summary>

```js
const [first, second] = array;
```

This is roughly equivalent to:

```js
// 1. Lookup @@iterator property on `array` (user-mutable if user provided).
// 2. Lookup @@iterator property on %Array.prototype% (user-mutable).
// 3. Call that function.
const iterator = array[Symbol.iterator]();
// 1. Lookup `next` property on `iterator` (doesn't exist).
// 2. Lookup `next` property on %ArrayIteratorPrototype% (user-mutable).
// 3. Call that function.
const first = iterator.next().value;
// Repeat.
const second = iterator.next().value;
```

Instead you can use object destructuring:

```js
const { 0: first, 1: second } = array;
```

or

```js
const first = array[0];
const second = array[1];
```

This only applies if you are working with a genuine array (or array-like
object), if you are instead expecting any iterator, array destructuring is the
correct syntax to use.

</details>

<details>

<summary>Avoid spread operator on arrays</summary>

```js
// 1. Lookup @@iterator property on `array` (user-mutable if user provided).
// 2. Lookup @@iterator property on %Array.prototype% (user-mutable).
// 3. Lookup `next` property on %ArrayIteratorPrototype% (user-mutable).
const arrayCopy = [...array];
func(...array);
```

Instead you can use other ECMAScript features to achieve the same result:

```js
const arrayCopy = ArrayPrototypeSlice(array); // fastest: https://jsben.ch/roLVC
ReflectApply(func, null, array);
```

</details>

<details>

<summary>`%Promise.all%` iterate over an array</summary>

```js
// 1. Lookup @@iterator property on `array` (user-mutable if user provided).
// 2. Lookup @@iterator property on %Array.prototype% (user-mutable).
// 3. Lookup `next` property on %ArrayIteratorPrototype% (user-mutable).
PromiseAll(array); // unsafe

PromiseAll(new SafeArrayIterator(array)); // safe
```

</details>

<details>

<summary>`%Map%` and `%Set%` constructors iterate over an array</summary>

```js
// 1. Lookup @@iterator property on %Array.prototype% (user-mutable).
// 2. Lookup `next` property on %ArrayIteratorPrototype% (user-mutable).
const set = new SafeSet([1, 2, 3]);
```

```js
const set = new SafeSet();
set.add(1).add(2).add(3);
```

</details>
