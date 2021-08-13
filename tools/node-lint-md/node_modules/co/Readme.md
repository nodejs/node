# Co

  [![Build Status](https://travis-ci.org/visionmedia/co.svg?branch=master)](https://travis-ci.org/visionmedia/co)

  Generator based flow-control goodness for nodejs and the browser, using
  thunks _or_ promises, letting you write non-blocking code in a nice-ish
  way.

  Co is careful to relay any errors that occur back to the generator, including those
  within the thunk, or from the thunk's callback. "Uncaught" exceptions in the generator
  are passed to `co()`'s thunk.

  Make sure to view the [examples](https://github.com/visionmedia/co/tree/master/examples).

## Platform Compatibility

  When using node 0.11.x or greater, you must use the `--harmony-generators`
  flag or just `--harmony` to get access to generators.

  When using node 0.10.x and lower or browsers without generator support,
  you must use [gnode](https://github.com/TooTallNate/gnode) and/or [regenerator](http://facebook.github.io/regenerator/).

  When using node 0.8.x and lower or browsers without `setImmediate`,
  you must include a `setImmediate` polyfill.
  For a really simple polyfill, you may use [component/setimmediate.js](https://github.com/component/setimmediate.js).
  For a more robust polyfill, you may use [YuzuJS/setImmediate](https://github.com/YuzuJS/setImmediate).

## Installation

```
$ npm install co
```

## Associated libraries

  View the [wiki](https://github.com/visionmedia/co/wiki) for libraries that
  work well with Co.

## Example

```js
var co = require('co');
var thunkify = require('thunkify');
var request = require('request');
var get = thunkify(request.get);

co(function *(){
  var a = yield get('http://google.com');
  var b = yield get('http://yahoo.com');
  var c = yield get('http://cloudup.com');
  console.log(a[0].statusCode);
  console.log(b[0].statusCode);
  console.log(c[0].statusCode);
})()

co(function *(){
  var a = get('http://google.com');
  var b = get('http://yahoo.com');
  var c = get('http://cloudup.com');
  var res = yield [a, b, c];
  console.log(res);
})()

// Error handling

co(function *(){
  try {
    var res = yield get('http://badhost.invalid');
    console.log(res);
  } catch(e) {
    console.log(e.code) // ENOTFOUND
 }
})()
```

## Yieldables

  The "yieldable" objects currently supported are:

  - promises
  - thunks (functions)
  - array (parallel execution)
  - objects (parallel execution)
  - generators (delegation)
  - generator functions (delegation)

To convert a regular node function that accepts a callback into one which returns a thunk you may want to use [thunkify](https://github.com/visionmedia/node-thunkify) or similar.

## Thunks vs promises

  While co supports promises, you may return "thunks" from your functions,
  which otherwise behaves just like the traditional node-style callback
  with a signature of: `(err, result)`.


  For example take `fs.readFile`, we all know the signature is:

```js
fs.readFile(path, encoding, function(err, result){

});
```

  To work with Co we need a function to return another function of
  the same signature:

```js
fs.readFile(path, encoding)(function(err, result){

});
```

  Which basically looks like this:

```js
function read(path, encoding) {
  return function(cb){
    fs.readFile(path, encoding, cb);
  }
}
```

  or to execute immediately like this (see [`thunkify`](https://github.com/visionmedia/node-thunkify)):

```js
function read(path, encoding) {
  // call fs.readFile immediately, store result later
  return function(cb){
    // cb(err, result) or when result ready
  }
}
```

## Receiver propagation

  When `co` is invoked with a receiver it will propagate to most yieldables,
  allowing you to alter `this`.

```js
var ctx = {};

function foo() {
  assert(this == ctx);
}

co(function *(){
  assert(this == ctx);
  yield foo;
}).call(ctx)
```

  You also pass arguments through the generator:

```js
co(function *(a){
  assert(this == ctx);
  assert('yay' == a);
  yield foo;
}).call(ctx, 'yay');
```

## API

### co(fn)

  Pass a generator `fn` and return a thunk. The thunk's signature is
  `(err, result)`, where `result` is the value passed to the `return`
  statement.

```js
var co = require('co');
var fs = require('fs');

function read(file) {
  return function(fn){
    fs.readFile(file, 'utf8', fn);
  }
}

co(function *(){
  var a = yield read('.gitignore');
  var b = yield read('Makefile');
  var c = yield read('package.json');
  return [a, b, c];
})()
```

  You may also yield `Generator` objects to support nesting:


```js
var co = require('co');
var fs = require('fs');

function size(file) {
  return function(fn){
    fs.stat(file, function(err, stat){
      if (err) return fn(err);
      fn(null, stat.size);
    });
  }
}

function *foo(){
  var a = yield size('.gitignore');
  var b = yield size('Makefile');
  var c = yield size('package.json');
  return [a, b, c];
}

function *bar(){
  var a = yield size('examples/parallel.js');
  var b = yield size('examples/nested.js');
  var c = yield size('examples/simple.js');
  return [a, b, c];
}

co(function *(){
  var results = yield [foo(), bar()];
  console.log(results);
})()
```

  Or if the generator functions do not require arguments, simply `yield` the function:

```js
var co = require('co');
var thunkify = require('thunkify');
var request = require('request');

var get = thunkify(request.get);

function *results() {
  var a = get('http://google.com')
  var b = get('http://yahoo.com')
  var c = get('http://ign.com')
  return yield [a, b, c]
}

co(function *(){
  // 3 concurrent requests at a time
  var a = yield results;
  var b = yield results;
  console.log(a, b);

  // 6 concurrent requests
  console.log(yield [results, results]);
})()
```

  If a thunk is written to execute immediately you may achieve parallelism
  by simply `yield`-ing _after_ the call. The following are equivalent if
  each call kicks off execution immediately:

```js
co(function *(){
  var a = size('package.json');
  var b = size('Readme.md');
  var c = size('Makefile');

  return [yield a, yield b, yield c];
})()
```

  Or:

```js
co(function *(){
  var a = size('package.json');
  var b = size('Readme.md');
  var c = size('Makefile');

  return yield [a, b, c];
})()
```

  You can also pass arguments into the generator. The last argument, `done`, is
  the callback function. Here's an example:

```js
var exec = require('co-exec');
co(function *(cmd) {
  var res = yield exec(cmd);
  return res;
})('pwd', done);
```

### yield array

  By yielding an array of thunks you may "join" them all into a single thunk which executes them all concurrently,
  instead of in sequence. Note that the resulting array ordering _is_ retained.

```js

var co = require('co');
var fs = require('fs');

function size(file) {
  return function(fn){
    fs.stat(file, function(err, stat){
      if (err) return fn(err);
      fn(null, stat.size);
    });
  }
}

co(function *(){
  var a = size('.gitignore');
  var b = size('index.js');
  var c = size('Makefile');
  var res = yield [a, b, c];
  console.log(res);
  // => [ 13, 1687, 129 ]
})()
```

Nested arrays may also be expressed as simple nested arrays:

```js
var a = [
  get('http://google.com'),
  get('http://yahoo.com'),
  get('http://ign.com')
];

var b = [
  get('http://google.com'),
  get('http://yahoo.com'),
  get('http://ign.com')
];

console.log(yield [a, b]);
```

### yield object

  Yielding an object behaves much like yielding an array, however recursion is supported:

```js
co(function *(){
  var user = yield {
    name: {
      first: get('name.first'),
      last: get('name.last')
    }
  };
})()
```

  Here is the sequential equivalent without yielding an object:

```js
co(function *(){
  var user = {
    name: {
      first: yield get('name.first'),
      last: yield get('name.last')
    }
  };
})()
```

### Performance

  On my machine 30,000 sequential stat()s takes an avg of 570ms,
  while the same number of sequential stat()s with `co()` takes
  610ms, aka the overhead introduced by generators is _extremely_ negligible.

## License

  MIT
