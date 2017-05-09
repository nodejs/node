# trough [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

> **trough** /trôf/ — a channel used to convey a liquid.

**trough** is like [`ware`][ware] with less sugar, and middleware
functions can change the input of the next.

## Installation

[npm][]:

```bash
npm install trough
```

## Usage

```js
var fs = require('fs');
var path = require('path');
var trough = require('trough');

var pipeline = trough()
  .use(function (fileName) {
    console.log('Checking... ' + fileName);
  })
  .use(function (fileName) {
    return path.join(process.cwd(), fileName);
  })
  .use(function (filePath, next) {
    fs.stat(filePath, function (err, stats) {
      next(err, {filePath: filePath, stats: stats});
    });
  })
  .use(function (ctx, next) {
    if (ctx.stats.isFile()) {
      fs.readFile(ctx.filePath, next);
    } else {
      next(new Error('Expected file'));
    }
  });

pipeline.run('readme.md', console.log);
pipeline.run('node_modules', console.log);
```

Yields:

```txt
Checking... readme.md
Checking... node_modules
Error: Expected file
    at ~/example.js:18:12
    at wrapped (~/node_modules/trough/index.js:120:19)
    at next (~/node_modules/trough/index.js:77:24)
    at done (~/node_modules/trough/index.js:157:12)
    at ~/example.js:11:7
    at FSReqWrap.oncomplete (fs.js:123:15)
null <Buffer 23 20 74 72 6f 75 67 68 20 5b 21 5b 42 75 69 6c 64 20 53 74 61 74 75 73 5d 5b 74 72 61 76 69 73 2d 62 61 64 67 65 5d 5d 5b 74 72 61 76 69 73 5d 20 5b ... >
```

## API

### `trough()`

Create a new [`Trough`][trough].

### `Trough`

A pipeline.

### `Trough#run([input..., ]done)`

Run the pipeline (all [`use()`][use]d middleware).  Invokes [`done`][done]
on completion with either an error or the output of the last middleware

> Note! as the length of input defines whether [async][] function
> get a `next` function, it’s recommended to keep `input` at one
> value normally.

#### `function done(err?, [output...])`

The final handler passed to [`run()`][run], invoked with an error
if a [middleware function][fn] rejected, passed, or threw one, or
the output of the last middleware function.

### `Trough#use(fn)`

Add `fn`, a [middleware function][fn], to the pipeline.

#### `function fn([input..., ][next])`

A middleware function invoked with the output of its predecessor.

##### Synchronous

If `fn` returns or throws an error, the pipeline fails and `done` is
invoked with that error.

If `fn` returns a value (neither `null` nor `undefined`), the first
`input` of the next function is set to that value (all other `input`
is passed through).

###### Example

The following example shows how returning an error stops the pipeline:

```js
var trough = require('trough');

trough().use(function (val) {
  return new Error('Got: ' + val);
}).run('some value', console.log);
```

Yields:

```txt
Error: Got: some value
    at ~example.js:6:10
    ...
```

The following example shows how throwing an error stops the pipeline:

```js
var trough = require('trough');

trough().use(function (val) {
  throw new Error('Got: ' + val);
}).run('more value', console.log);
```

Yields:

```txt
Error: Got: more value
    at ~example.js:6:10
    ...
```

The following example shows how the first output can be modified:

```js
var trough = require('trough');

trough().use(function (val) {
  return 'even ' + val;
}).run('more value', 'untouched', console.log);
```

Yields:

```txt
null 'even more value' 'untouched'
```

##### Promise

If `fn` returns a promise, and that promise rejects, the pipeline fails
and `done` is invoked with the rejected value.

If `fn` returns a promise, and that promise resolves with a value
(neither `null` nor `undefined`), the first `input` of the next function
is set to that value (all other `input` is passed through).

###### Example

The following example shows how rejecting a promise stops the pipeline:

```js
trough().use(function (val) {
  return new Promise(function (resolve, reject) {
    reject('Got: ' + val);
  });
}).run('val', console.log);
```

Yields:

```txt
Got: val
```

The following example shows how the input isn’t touched by resolving
to `null`.

```js
trough().use(function (val) {
  return new Promise(function (resolve, reject) {
    setTimeout(function () { resolve(null); }, 100);
  });
}).run('Input', console.log);
```

Yields:

```txt
null 'Input'
```

##### Asynchronous

If `fn` accepts one more argument than the given `input`, a `next`
function is given (after the input).  `next` must be called, but doesn’t
have to be called async.

If `next` is given a a value (neither `null` nor `undefined`) as its first
argument, the pipeline fails and `done` is invoked with that value.

If `next` is given no value (either `null` or `undefined`) as the first
argument, all following non-nully values change the input of the following
function, and all nully values default to the `input`.

###### Example

The following example shows how passing a first argument stops the
pipeline:

```js
trough().use(function (val, next) {
  next(new Error('Got: ' + val));
}).run('val', console.log);
```

Yields:

```txt
Error: Got: val
    at ~/example.js:6:8
```

The following example shows how more values than the input are passed.

```js
trough().use(function (val, next) {
  setTimeout(function () {
    next(null, null, 'values');
  }, 100);
}).run('some', console.log);
```

Yields:

```txt
null 'some' 'values'
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/trough.svg

[travis]: https://travis-ci.org/wooorm/trough

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/trough.svg

[codecov]: https://codecov.io/github/wooorm/trough

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[ware]: https://github.com/segmentio/ware

[trough]: #trough-1

[use]: #troughusefn

[run]: #troughruninput-done

[fn]: #function-fninput-next

[done]: #function-doneerr-output

[async]: #asynchronous
