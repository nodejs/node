# trough

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

> **trough** /trôf/ — a channel used to convey a liquid.

`trough` is like [`ware`][ware] with less sugar, and middleware functions can
change the input of the next.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install trough
```

## Use

```js
import fs from 'fs'
import path from 'path'
import {trough} from 'trough'

const pipeline = trough()
  .use(function(fileName) {
    console.log('Checking… ' + fileName)
  })
  .use(function(fileName) {
    return path.join(process.cwd(), fileName)
  })
  .use(function(filePath, next) {
    fs.stat(filePath, function(err, stats) {
      next(err, {filePath, stats})
    })
  })
  .use(function(ctx, next) {
    if (ctx.stats.isFile()) {
      fs.readFile(ctx.filePath, next)
    } else {
      next(new Error('Expected file'))
    }
  })

pipeline.run('readme.md', console.log)
pipeline.run('node_modules', console.log)
```

Yields:

```txt
Checking… readme.md
Checking… node_modules
Error: Expected file
    at ~/example.js:21:12
    at wrapped (~/node_modules/trough/index.js:93:19)
    at next (~/node_modules/trough/index.js:56:24)
    at done (~/node_modules/trough/index.js:124:12)
    at ~/node_modules/example.js:14:7
    at FSReqWrap.oncomplete (fs.js:153:5)
null <Buffer 23 20 74 72 6f 75 67 68 20 5b 21 5b 42 75 69 6c 64 20 53 74 61 74 75 73 5d 5b 74 72 61 76 69 73 2d 62 61 64 67 65 5d 5d 5b 74 72 61 76 69 73 5d 20 5b ... >
```

## API

This package exports the following identifiers: `trough` and `wrap`.
There is no default export.

### `trough()`

Create a new [`Trough`][trough].

### `wrap(middleware, callback)(…input)`

Call `middleware` with all input.
If `middleware` accepts more arguments than given in input, an extra `done`
function is passed in after the input when calling it.
In that case, `done` must be called.

The first value in `input` is the main input value.
All other input values are the rest input values.
The values given to `callback` are the input values, merged with every
non-nullish output value.

*   If `middleware` throws an error, returns a promise that is rejected, or
    calls the given `done` function with an error, `callback` is called with
    that error
*   If `middleware` returns a value or returns a promise that is resolved, that
    value is the main output value
*   If `middleware` calls `done`, all non-nullish values except for the first
    one (the error) overwrite the output values

### `Trough`

A pipeline.

#### `Trough#run([input…, ]done)`

Run the pipeline (all [`use()`][use]d middleware).
Calls [`done`][done] on completion with either an error or the output of the
last middleware.

> Note!
> as the length of input defines whether [async][] functions get a `next`
> function, it’s recommended to keep `input` at one value normally.

##### `function done(err?, [output…])`

The final handler passed to [`run()`][run], called with an error if a
[middleware function][fn] rejected, passed, or threw one, or the output of the
last middleware function.

#### `Trough#use(fn)`

Add `fn`, a [middleware function][fn], to the pipeline.

##### `function fn([input…, ][next])`

A middleware function called with the output of its predecessor.

###### Synchronous

If `fn` returns or throws an error, the pipeline fails and `done` is called
with that error.

If `fn` returns a value (neither `null` nor `undefined`), the first `input` of
the next function is set to that value (all other `input` is passed through).

The following example shows how returning an error stops the pipeline:

```js
import {trough} from 'trough'

trough()
  .use(function(val) {
    return new Error('Got: ' + val)
  })
  .run('some value', console.log)
```

Yields:

```txt
Error: Got: some value
    at ~/example.js:5:12
    …
```

The following example shows how throwing an error stops the pipeline:

```js
import {trough} from 'trough'

trough()
  .use(function(val) {
    throw new Error('Got: ' + val)
  })
  .run('more value', console.log)
```

Yields:

```txt
Error: Got: more value
    at ~/example.js:5:11
    …
```

The following example shows how the first output can be modified:

```js
import {trough} from 'trough'

trough()
  .use(function(val) {
    return 'even ' + val
  })
  .run('more value', 'untouched', console.log)
```

Yields:

```txt
null 'even more value' 'untouched'
```

###### Promise

If `fn` returns a promise, and that promise rejects, the pipeline fails and
`done` is called with the rejected value.

If `fn` returns a promise, and that promise resolves with a value (neither
`null` nor `undefined`), the first `input` of the next function is set to that
value (all other `input` is passed through).

The following example shows how rejecting a promise stops the pipeline:

```js
import {trough} from 'trough'

trough()
  .use(function(val) {
    return new Promise(function(resolve, reject) {
      reject('Got: ' + val)
    })
  })
  .run('val', console.log)
```

Yields:

```txt
Got: val
```

The following example shows how the input isn’t touched by resolving to `null`.

```js
import {trough} from 'trough'

trough()
  .use(function() {
    return new Promise(function(resolve) {
      setTimeout(function() {
        resolve(null)
      }, 100)
    })
  })
  .run('Input', console.log)
```

Yields:

```txt
null 'Input'
```

###### Asynchronous

If `fn` accepts one more argument than the given `input`, a `next` function is
given (after the input).  `next` must be called, but doesn’t have to be called
async.

If `next` is given a value (neither `null` nor `undefined`) as its first
argument, the pipeline fails and `done` is called with that value.

If `next` is given no value (either `null` or `undefined`) as the first
argument, all following non-nullish values change the input of the following
function, and all nullish values default to the `input`.

The following example shows how passing a first argument stops the pipeline:

```js
import {trough} from 'trough'

trough()
  .use(function(val, next) {
    next(new Error('Got: ' + val))
  })
  .run('val', console.log)
```

Yields:

```txt
Error: Got: val
    at ~/example.js:5:10
```

The following example shows how more values than the input are passed.

```js
import {trough} from 'trough'

trough()
  .use(function(val, next) {
    setTimeout(function() {
      next(null, null, 'values')
    }, 100)
  })
  .run('some', console.log)
```

Yields:

```txt
null 'some' 'values'
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/trough/workflows/main/badge.svg

[build]: https://github.com/wooorm/trough/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/trough.svg

[coverage]: https://codecov.io/github/wooorm/trough

[downloads-badge]: https://img.shields.io/npm/dm/trough.svg

[downloads]: https://www.npmjs.com/package/trough

[size-badge]: https://img.shields.io/bundlephobia/minzip/trough.svg

[size]: https://bundlephobia.com/result?p=trough

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[ware]: https://github.com/segmentio/ware

[trough]: #trough-1

[use]: #troughusefn

[run]: #troughruninput-done

[fn]: #function-fninput-next

[done]: #function-doneerr-output

[async]: #asynchronous
