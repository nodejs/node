# errs [![Build Status](https://secure.travis-ci.org/flatiron/errs.png)](http://travis-ci.org/flatiron/errs)

Simple error creation and passing utilities focused on:

* [Creating Errors](#creating-errors)
* [Reusing Error Types](#reusing-types)
* [Merging with Existing Errors](#merging-errors)
* [Optional Callback Invocation](#optional-invocation)
* [Piping Error Events](#piping-errors)

<a name="creating-errors" />
## Creating Errors

You should know by now that [a String is not an Error][0]. Unfortunately the `Error` constructor in Javascript isn't all that convenient either. How often do you find yourself in this situation?

``` js
  var err = new Error('This is an error. There are many like it.');
  err.someProperty = 'more syntax';
  err.someOtherProperty = 'it wont stop.';
  err.notEven = 'for the mayor';

  throw err;
```

Rest your fingers, `errs` is here to help. The following is equivalent to the above:

``` js
  var errs = require('errs');

  throw errs.create({
    message: 'This is an error. There are many like it.',
    someProperty: 'more syntax',
    someOtherProperty: 'it wont stop.',
    notEven: 'for the mayor'
  });
```

<a name="reusing-types" />
## Reusing Custom Error Types

`errs` also exposes an [inversion of control][1] interface for easily reusing custom error types across your application. Custom Error Types registered with `errs` will transparently invoke `Error` constructor and `Error.captureStackTrace` to attach transparent stack traces:

``` js
  /*
   * file-a.js: Create and register your error type.
   *
   */

  var util = require('util'),
      errs = require('errs');

  function MyError() {
    this.message = 'This is my error; I made it myself. It has a transparent stack trace.';
  }

  //
  // Alternatively `MyError.prototype.__proto__ = Error;`
  //
  util.inherits(MyError, Error);

  //
  // Register the error type
  //
  errs.register('myerror', MyError);



  /*
   * file-b.js: Use your error type.
   *
   */

  var errs = require('errs');

  console.log(
    errs.create('myerror')
      .stack
      .split('\n')
  );
```

The output from the two files above is shown below. Notice how it contains no references to `errs.js`:

```
[ 'MyError: This is my error; I made it myself. It has a transparent stack trace.',
  '    at Object.<anonymous> (/file-b.js:19:8)',
  '    at Module._compile (module.js:441:26)',
  '    at Object..js (module.js:459:10)',
  '    at Module.load (module.js:348:31)',
  '    at Function._load (module.js:308:12)',
  '    at Array.0 (module.js:479:10)',
  '    at EventEmitter._tickCallback (node.js:192:40)' ]
```

<a name="merging-errors" />
## Merging with Existing Errors

When working with errors you catch or are returned in a callback you can extend those errors with properties by using the `errs.merge` method. This will also create a human readable error message and stack-trace:

``` js
process.on('uncaughtException', function(err) {
  console.log(errs.merge(err, {namespace: 'uncaughtException'}));
});

var file = fs.createReadStream('FileDoesNotExist.here');
```

``` js
{ [Error: Unspecified error]
  name: 'Error',
  namespace: 'uncaughtException',
  errno: 34,
  code: 'ENOENT',
  path: 'FileDoesNotExist.here',
  description: 'ENOENT, no such file or directory \'FileDoesNotExist.here\'',
  stacktrace: [ 'Error: ENOENT, no such file or directory \'FileDoesNotExist.here\'' ] }
```

<a name="optional-invocation" />
## Optional Callback Invocation

Node.js handles asynchronous IO through the elegant `EventEmitter` API. In many scenarios the `callback` may be optional because you are returning an `EventEmitter` for piping or other event multiplexing. This complicates code with a lot of boilerplate:

``` js
  function importantFeature(callback) {
    return someAsyncFn(function (err) {
      if (err) {
        if (callback) {
          return callback(err);
        }

        throw err;
      }
    });
  }
```

`errs` it presents a common API for both emitting `error` events and invoking continuations (i.e. callbacks) with errors. If a `callback` is supplied to `errs.handle()` it will be invoked with the error. It no `callback` is provided then an `EventEmitter` is returned which emits an `error` event on the next tick:

``` js
  function importantFeature(callback) {
    return someAsyncFn(function (err) {
      if (err) {
        return errs.handle(err, callback);
      }
    });
  }
```

<a name="piping-errors" />
## Piping Errors

Often when working with streams (especially when buffering for whatever reason), you may have already returned an `EventEmitter` or `Stream` instance by the time an error is handled.

``` js
  function pipeSomething(callback) {
    //
    // You have a stream (e.g. http.ResponseStream) and you
    // have an optional `callback`.
    //
    var stream = new require('stream').Stream;

    //
    // You need to do something async which may respond with an
    // error
    //
    getAnotherStream(function (err, source) {
      if (err) {
        if (callback)
          callback(err);
        }

        stream.emit('error', err);
        return;
      }

      source.pipe(stream);
    })

    return stream;
  }
```

You may pass either a `function` or `EventEmitter` instance to `errs.handle`.

``` js
  function pipeSomething(callback) {
    //
    // You have a stream (e.g. http.ResponseStream) and you
    // have an optional `callback`.
    //
    var stream = new require('stream').Stream;

    //
    // You need to do something async which may respond with an
    // error
    //
    getAnotherStream(function (err, source) {
      if (err) {
        //
        // Invoke the callback if it exists otherwise the stream.
        //
        return errs.handle(err, callback || stream);
      }

      source.pipe(stream);
    })

    return stream;
  }
```

If you wish to invoke both a `callback` function and an `error` event simply pass both:

``` js
  errs.handle(err, callback, stream);
```

## Methods
The `errs` modules exposes some simple utility methods:

* `.create(type, opts)`: Creates a new error instance for with the specified `type` and `opts`. If the `type` is not registered then a new `Error` instance will be created.
* `.register(type, proto)`: Registers the specified `proto` to `type` for future calls to `errors.create(type, opts)`.
* `.unregister(type)`: Unregisters the specified `type` for future calls to `errors.create(type, opts)`.
* `.handle(err, callback)`: Attempts to instantiate the given `error`. If the `error` is already a properly formed `error` object (with a `stack` property) it will not be modified.
* `.merge(err, type, opts)`: Merges an existing error with a new error instance for with the specified `type` and `opts`.

## Installation

### Installing npm (node package manager)

``` bash
  $ curl http://npmjs.org/install.sh | sh
```

### Installing errs

``` bash
  $ [sudo] npm install errs
```

## Tests
All tests are written with [vows][2] and should be run with [npm][3]:

``` bash
  $ npm test
```

#### Author: [Charlie Robbins](http://github.com/indexzero)
#### Contributors: [Nuno Job](http://github.com/dscape)
#### License: MIT

[0]: http://www.devthought.com/2011/12/22/a-string-is-not-an-error/
[1]: http://martinfowler.com/articles/injection.html
[2]: https://vowsjs.org
[3]: https://npmjs.org
