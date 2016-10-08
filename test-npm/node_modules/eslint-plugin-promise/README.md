# eslint-plugin-promise

Enforce best practices for JavaScript promises.

 [![js-standard-style](https://cdn.rawgit.com/feross/standard/master/badge.svg)](https://github.com/feross/standard)
 [![travis-ci](https://travis-ci.org/xjamundx/eslint-plugin-promise.svg)](https://travis-ci.org/xjamundx/eslint-plugin-promise)
[![npm version](https://badge.fury.io/js/eslint-plugin-promise.svg)](https://www.npmjs.com/package/eslint-plugin-promise)

## Rule


### `catch-or-return`

Ensure that each time a `then()` is applied to a promise, a
`catch()` is applied as well. Exceptions are made if you are
returning that promise.

Formerly called `always-catch`.

#### Valid

```js
myPromise.then(doSomething).catch(errors);
myPromise.then(doSomething).then(doSomethingElse).catch(errors);
function doSomethingElse() { return myPromise.then(doSomething) }
```

#### Invalid

```js
myPromise.then(doSomething);
myPromise.then(doSomething, catchErrors); // catch() may be a little better
function doSomethingElse() { myPromise.then(doSomething) }
```

#### Options

#### `allowThen`

You can pass an `{ allowThen: true }` as an option to this rule
 to allow for `.then(null, fn)` to be used instead of `catch()` at
 the end of the promise chain.

#### `terminationMethod`

You can pass a `{ terminationMethod: 'done' }` as an option to this rule
 to require `done()` instead of `catch()` at the end of the promise chain.
 This is useful for many non-standard Promise implementations.

### `always-return`

Ensure that inside a `then()` you make sure to `return` a new promise or value.
See http://pouchdb.com/2015/05/18/we-have-a-problem-with-promises.html (rule #5)
for more info on why that's a good idea.

We also allow someone to `throw` inside a `then()` which is essentially the same as `return Promise.reject()`.

#### Valid

```js
myPromise.then((val) => val * 2));
myPromise.then(function(val) { return val * 2; });
myPromise.then(doSomething); // could be either
```

#### Invalid

```js
myPromise.then(function(val) {});
myPromise.then(() => { doSomething(); });
```

### `param-names`

Enforce standard parameter names for Promise constructors

#### Valid
```js
new Promise(function (resolve) { ... })
new Promise(function (resolve, reject) { ... })
```

#### Invalid
```js
new Promise(function (reject, resolve) { ... }) // incorrect order
new Promise(function (ok, fail) { ... }) // non-standard parameter names
```

Ensures that `new Promise()` is instantiated with the parameter names `resolve, reject` to avoid confusion with order such as `reject, resolve`. The Promise constructor uses the [RevealingConstructor pattern](https://blog.domenic.me/the-revealing-constructor-pattern/). Using the same parameter names as the language specification makes code more uniform and easier to understand.

### `no-native`

Ensure that `Promise` is included fresh in each file instead of relying
 on the existence of a native promise implementation. Helpful if you want
 to use `bluebird` or if you don't intend to use an ES6 Promise shim.


#### Valid
```js
var Promise = require("blubird");
var x = Promise.resolve("good");
```

#### Invalid
```js
var x = Promise.resolve("bad");
```

## Installation

You'll first need to install [ESLint](http://eslint.org):

```
$ npm i eslint --save-dev
```

Next, install `eslint-plugin-promise`:

```
$ npm install eslint-plugin-promise --save-dev
```

**Note:** If you installed ESLint globally (using the `-g` flag) then you must also install `eslint-plugin-promise` globally.

## Usage

Add `promise` to the plugins section of your `.eslintrc` configuration file. You can omit the `eslint-plugin-` prefix:

```json
{
    "plugins": [
        "promise"
    ]
}
```


Then configure the rules you want to use under the rules section.

```json
{
    "rules": {
        "promise/param-names": 2,
        "promise/always-return": 2,
        "promise/always-catch": 2, // deprecated
        "promise/catch-or-return": 2,
        "promise/no-native": 0,
    }
}
```

## Etc
- (c) MMXV jden <jason@denizac.org> - ISC license.
- (c) 2016 Jamund Ferguson <jamund@gmail.com> - ISC license.
