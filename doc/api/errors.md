# Errors

<!--type=misc-->

Applications running in Node.js will generally experience four categories of
errors:

- Standard JavaScript errors such as:
  - {EvalError} : thrown when a call to `eval()` fails.
  - {SyntaxError} : thrown in response to improper JavaScript language
    syntax.
  - {RangeError} : thrown when a value is not within an expected range
  - {ReferenceError} : thrown when using undefined variables
  - {TypeError} : thrown when passing arguments of the wrong type
  - {URIError} : thrown when a global URI handling function is misused.
- System errors triggered by underlying operating system constraints such
  as attempting to open a file that does not exist, attempting to send data
  over a closed socket, etc;
- And User-specified errors triggered by application code.
- Assertion Errors are a special class of error that can be triggered whenever
  Node.js detects an exceptional logic violation that should never occur. These
  are raised typically by the `assert` module.

All JavaScript and System errors raised by Node.js inherit from, or are
instances of, the standard JavaScript {Error} class and are guaranteed
to provide *at least* the properties available on that class.

## Error Propagation and Interception

<!--type=misc-->

Node.js supports several mechanisms for propagating and handling errors that
occur while an application is running. How these errors are reported and
handled depends entirely on the type of Error and the style of the API that is
called.

All JavaScript errors are handled as exceptions that *immediately* generate
and throw an error using the standard JavaScript `throw` mechanism. These
are handled using the [`try / catch` construct][try-catch] provided by the JavaScript
language.

```js
// Throws with a ReferenceError because z is undefined
try {
  const m = 1;
  const n = m + z;
} catch (err) {
  // Handle the error here.
}
```

Any use of the JavaScript `throw` mechanism will raise an exception that
*must* be handled using `try / catch` or the Node.js process will exit
immediately.

With few exceptions, _Synchronous_ APIs (any blocking method that does not
accept a `callback` function, such as [`fs.readFileSync`][]), will use `throw`
to report errors.

Errors that occur within _Asynchronous APIs_ may be reported in multiple ways:

- Most asynchronous methods that accept a `callback` function will accept an
  `Error` object passed as the first argument to that function. If that first
  argument is not `null` and is an instance of `Error`, then an error occurred
  that should be handled.

  ```js
  const fs = require('fs');
  fs.readFile('a file that does not exist', (err, data) => {
    if (err) {
      console.error('There was an error reading the file!', err);
      return;
    }
    // Otherwise handle the data
  });
  ```
- When an asynchronous method is called on an object that is an `EventEmitter`,
  errors can be routed to that object's `'error'` event.

  ```js
  const net = require('net');
  const connection = net.connect('localhost');

  // Adding an 'error' event handler to a stream:
  connection.on('error', (err) => {
    // If the connection is reset by the server, or if it can't
    // connect at all, or on any sort of error encountered by
    // the connection, the error will be sent here.
    console.error(err);
  });

  connection.pipe(process.stdout);
  ```

- A handful of typically asynchronous methods in the Node.js API may still
  use the `throw` mechanism to raise exceptions that must be handled using
  `try / catch`. There is no comprehensive list of such methods; please
  refer to the documentation of each method to determine the appropriate
  error handling mechanism required.

The use of the `'error'` event mechanism is most common for [stream-based][]
and [event emitter-based][] APIs, which themselves represent a series of
asynchronous operations over time (as opposed to a single operation that may
pass or fail).

For *all* `EventEmitter` objects, if an `'error'` event handler is not
provided, the error will be thrown, causing the Node.js process to report an
unhandled exception and  crash unless either: The [`domain`][domains] module is used
appropriately or a handler has been registered for the
[`process.on('uncaughtException')`][] event.

```js
const EventEmitter = require('events');
const ee = new EventEmitter();

setImmediate(() => {
  // This will crash the process because no 'error' event
  // handler has been added.
  ee.emit('error', new Error('This will crash'));
});
```

Errors generated in this way *cannot* be intercepted using `try / catch` as
they are thrown *after* the calling code has already exited.

Developers must refer to the documentation for each method to determine
exactly how errors raised by those methods are propagated.

### Node.js style callbacks

<!--type=misc-->

Most asynchronous methods exposed by the Node.js core API follow an idiomatic
pattern  referred to as a "Node.js style callback". With this pattern, a
callback function is passed to the method as an argument. When the operation
either completes or an error is raised, the callback function is called with
the Error object (if any) passed as the first argument. If no error was raised,
the first argument will be passed as `null`.

```js
const fs = require('fs');

function nodeStyleCallback(err, data) {
 if (err) {
   console.error('There was an error', err);
   return;
 }
 console.log(data);
}

fs.readFile('/some/file/that/does-not-exist', nodeStyleCallback);
fs.readFile('/some/file/that/does-exist', nodeStyleCallback)
```

The JavaScript `try / catch` mechanism **cannot** be used to intercept errors
generated by asynchronous APIs.  A common mistake for beginners is to try to
use `throw` inside a Node.js style callback:

```js
// THIS WILL NOT WORK:
const fs = require('fs');

try {
  fs.readFile('/some/file/that/does-not-exist', (err, data) => {
    // mistaken assumption: throwing here...
    if (err) {
      throw err;
    }
  });
} catch(err) {
  // This will not catch the throw!
  console.log(err);
}
```

This will not work because the callback function passed to `fs.readFile()` is
called asynchronously. By the time the callback has been called, the
surrounding code (including the `try { } catch(err) { }` block will have
already exited. Throwing an error inside the callback **can crash the Node.js
process** in most cases. If [domains][] are enabled, or a handler has been
registered with `process.on('uncaughtException')`, such errors can be
intercepted.

## Class: Error

<!--type=class-->

A generic JavaScript `Error` object that does not denote any specific
circumstance of why the error occurred. `Error` objects capture a "stack trace"
detailing the point in the code at which the `Error` was instantiated, and may
provide a text description of the error.

All errors generated by Node.js, including all System and JavaScript errors,
will either be instances of, or inherit from, the `Error` class.

### new Error(message)

Creates a new `Error` object and sets the `error.message` property to the
provided text message. If an object is passed as `message`, the text message
is generated by calling `message.toString()`. The `error.stack` property will
represent the point in the code at which `new Error()` was called. Stack traces
are dependent on [V8's stack trace API][]. Stack traces extend only to either
(a) the beginning of  *synchronous code execution*, or (b) the number of frames
given by the property `Error.stackTraceLimit`, whichever is smaller.

### Error.captureStackTrace(targetObject[, constructorOpt])

Creates a `.stack` property on `targetObject`, which when accessed returns
a string representing the location in the code at which
`Error.captureStackTrace()` was called.

```js
const myObject = {};
Error.captureStackTrace(myObject);
myObject.stack  // similar to `new Error().stack`
```

The first line of the trace, instead of being prefixed with `ErrorType:
message`, will be the result of calling `targetObject.toString()`.

The optional `constructorOpt` argument accepts a function. If given, all frames
above `constructorOpt`, including `constructorOpt`, will be omitted from the
generated stack trace.

The `constructorOpt` argument is useful for hiding implementation
details of error generation from an end user. For instance:

```js
function MyError() {
  Error.captureStackTrace(this, MyError);
}

// Without passing MyError to captureStackTrace, the MyError
// frame would show up in the .stack property. By passing
// the constructor, we omit that frame and all frames above it.
new MyError().stack
```

### Error.stackTraceLimit

The `Error.stackTraceLimit` property specifies the number of stack frames
collected by a stack trace (whether generated by `new Error().stack` or
`Error.captureStackTrace(obj)`).

The default value is `10` but may be set to any valid JavaScript number. Changes
will affect any stack trace captured *after* the value has been changed.

If set to a non-number value, or set to a negative number, stack traces will
not capture any frames.

#### error.message

Returns the string description of error as set by calling `new Error(message)`.
The `message` passed to the constructor will also appear in the first line of
the stack trace of the `Error`, however changing this property after the
`Error` object is created *may not* change the first line of the stack trace.

```js
const err = new Error('The message');
console.log(err.message);
  // Prints: The message
```

#### error.stack

Returns a string describing the point in the code at which the `Error` was
instantiated.

For example:

```txt
Error: Things keep happening!
   at /home/gbusey/file.js:525:2
   at Frobnicator.refrobulate (/home/gbusey/business-logic.js:424:21)
   at Actor.<anonymous> (/home/gbusey/actors.js:400:8)
   at increaseSynergy (/home/gbusey/actors.js:701:6)
```

The first line is formatted as `<error class name>: <error message>`, and
is followed by a series of stack frames (each line beginning with "at ").
Each frame describes a call site within the code that lead to the error being
generated. V8 attempts to display a name for each function (by variable name,
function name, or object method name), but occasionally it will not be able to
find a suitable name. If V8 cannot determine a name for the function, only
location information will be displayed for that frame. Otherwise, the
determined function name will be displayed with location information appended
in parentheses.

It is important to note that frames are **only** generated for JavaScript
functions. If, for example, execution synchronously passes through a C++ addon
function called `cheetahify`, which itself calls a JavaScript function, the
frame representing the `cheetahify` call will **not** be present in the stack
traces:

```js
const cheetahify = require('./native-binding.node');

function makeFaster() {
  // cheetahify *synchronously* calls speedy.
  cheetahify(function speedy() {
    throw new Error('oh no!');
  });
}

makeFaster(); // will throw:
  // /home/gbusey/file.js:6
  //     throw new Error('oh no!');
  //           ^
  // Error: oh no!
  //     at speedy (/home/gbusey/file.js:6:11)
  //     at makeFaster (/home/gbusey/file.js:5:3)
  //     at Object.<anonymous> (/home/gbusey/file.js:10:1)
  //     at Module._compile (module.js:456:26)
  //     at Object.Module._extensions..js (module.js:474:10)
  //     at Module.load (module.js:356:32)
  //     at Function.Module._load (module.js:312:12)
  //     at Function.Module.runMain (module.js:497:10)
  //     at startup (node.js:119:16)
  //     at node.js:906:3
```

The location information will be one of:

* `native`, if the frame represents a call internal to V8 (as in `[].forEach`).
* `plain-filename.js:line:column`, if the frame represents a call internal
   to Node.js.
* `/absolute/path/to/file.js:line:column`, if the frame represents a call in
  a user program, or its dependencies.

The string representing the stack trace is lazily generated when the
`error.stack` property is **accessed**.

The number of frames captured by the stack trace is bounded by the smaller of
`Error.stackTraceLimit` or the number of available frames on the current event
loop tick.

System-level errors are generated as augmented `Error` instances, which are
detailed [here](#errors_system_errors).

## Class: RangeError

A subclass of `Error` that indicates that a provided argument was not within the
set or range of acceptable values for a function; whether that is a numeric
range, or outside the set of options for a given function parameter.

For example:

```js
require('net').connect(-1);
  // throws RangeError, port should be > 0 && < 65536
```

Node.js will generate and throw `RangeError` instances *immediately* as a form
of argument validation.

## Class: ReferenceError

A subclass of `Error` that indicates that an attempt is being made to access a
variable that is not defined. Such errors commonly indicate typos in code, or
an otherwise broken program.

While client code may generate and propagate these errors, in practice, only V8
will do so.

```js
doesNotExist;
  // throws ReferenceError, doesNotExist is not a variable in this program.
```

`ReferenceError` instances will have an `error.arguments` property whose value
is an array containing a single element: a string representing the variable
that was not defined.

```js
const assert = require('assert');
try {
  doesNotExist;
} catch(err) {
  assert(err.arguments[0], 'doesNotExist');
}
```

Unless an application is dynamically generating and running code,
`ReferenceError` instances should always be considered a bug in the code
or its dependencies.

## Class: SyntaxError

A subclass of `Error` that indicates that a program is not valid JavaScript.
These errors may only be generated and propagated as a result of code
evaluation. Code evaluation may happen as a result of `eval`, `Function`,
`require`, or [vm][]. These errors are almost always indicative of a broken
program.

```js
try {
  require('vm').runInThisContext('binary ! isNotOk');
} catch(err) {
  // err will be a SyntaxError
}
```

`SyntaxError` instances are unrecoverable in the context that created them –
they may only be caught by other contexts.

## Class: TypeError

A subclass of `Error` that indicates that a provided argument is not an
allowable type. For example, passing a function to a parameter which expects a
string would be considered a TypeError.

```js
require('url').parse(() => { });
  // throws TypeError, since it expected a string
```

Node.js will generate and throw `TypeError` instances *immediately* as a form
of argument validation.

## Exceptions vs. Errors

<!--type=misc-->

A JavaScript exception is a value that is thrown as a result of an invalid
operation or as the target of a `throw` statement. While it is not required
that these values are instances of `Error` or classes which inherit from
`Error`, all exceptions thrown by Node.js or the JavaScript runtime *will* be
instances of Error.

Some exceptions are *unrecoverable* at the JavaScript layer. Such exceptions
will *always* cause the Node.js process to crash. Examples include `assert()`
checks or `abort()` calls in the C++ layer.

## System Errors

System errors are generated when exceptions occur within the program's
runtime environment. Typically, these are operational errors that occur
when an application violates an operating system constraint such as attempting
to read a file that does not exist or when the user does not have sufficient
permissions.

System errors are typically generated at the syscall level: an exhaustive list
of error codes and their meanings is available by running `man 2 intro` or
`man 3 errno` on most Unices; or [online][].

In Node.js, system errors are represented as augmented `Error` objects with
added properties.

### Class: System Error

#### error.code
#### error.errno

Returns a string representing the error code, which is always `E` followed by
a sequence of capital letters, and may be referenced in `man 2 intro`.

The properties `error.code` and `error.errno` are aliases of one another and
return the same value.

#### error.syscall

Returns a string describing the [syscall][] that failed.

### Common System Errors

This list is **not exhaustive**, but enumerates many of the common system
errors encountered when writing a Node.js program. An exhaustive list may be
found [here][online].

- `EACCES` (Permission denied): An attempt was made to access a file in a way
  forbidden by its file access permissions.

- `EADDRINUSE` (Address already in use):  An attempt to bind a server
  ([`net`][], [`http`][], or [`https`][]) to a local address failed due to
  another server on the local system already occupying that address.

- `ECONNREFUSED` (Connection refused): No connection could be made because the
  target machine actively refused it. This usually results from trying to
  connect to a service that is inactive on the foreign host.

- `ECONNRESET` (Connection reset by peer): A connection was forcibly closed by
  a peer. This normally results from a loss of the connection on the remote
  socket due to a timeout or reboot. Commonly encountered via the [`http`][]
  and [`net`][] modules.

- `EEXIST` (File exists): An existing file was the target of an operation that
  required that the target not exist.

- `EISDIR` (Is a directory): An operation expected a file, but the given
  pathname was a directory.

- `EMFILE` (Too many open files in system): Maximum number of
  [file descriptors][] allowable on the system has been reached, and
  requests for another descriptor cannot be fulfilled until at least one
  has been closed. This is encountered when opening many files at once in
  parallel, especially on systems (in particular, OS X) where there is a low
  file descriptor limit for processes. To remedy a low limit, run
  `ulimit -n 2048` in the same shell that will run the Node.js process.

- `ENOENT` (No such file or directory): Commonly raised by [`fs`][] operations
  to indicate that a component of the specified pathname does not exist -- no
  entity (file or directory) could be found by the given path.

- `ENOTDIR` (Not a directory): A component of the given pathname existed, but
  was not a directory as expected. Commonly raised by [`fs.readdir`][].

- `ENOTEMPTY` (Directory not empty): A directory with entries was the target
  of an operation that requires an empty directory -- usually [`fs.unlink`][].

- `EPERM` (Operation not permitted): An attempt was made to perform an
  operation that requires elevated privileges.

- `EPIPE` (Broken pipe): A write on a pipe, socket, or FIFO for which there is
  no process to read the data. Commonly encountered at the [`net`][] and
  [`http`][] layers, indicative that the remote side of the stream being
  written to has been closed.

- `ETIMEDOUT` (Operation timed out): A connect or send request failed because
  the connected party did not properly respond after a period of time. Usually
  encountered by [`http`][] or [`net`][] -- often a sign that a `socket.end()`
  was not properly called.

[`fs.readdir`]: fs.html#fs_fs_readdir_path_options_callback
[`fs.readFileSync`]: fs.html#fs_fs_readfilesync_file_options
[`fs.unlink`]: fs.html#fs_fs_unlink_path_callback
[`fs`]: fs.html
[`http`]: http.html
[`https`]: https.html
[`net`]: net.html
[`process.on('uncaughtException')`]: process.html#process_event_uncaughtexception
[domains]: domain.html
[event emitter-based]: events.html#events_class_eventemitter
[file descriptors]: https://en.wikipedia.org/wiki/File_descriptor
[online]: http://man7.org/linux/man-pages/man3/errno.3.html
[stream-based]: stream.html
[syscall]: http://man7.org/linux/man-pages/man2/syscall.2.html
[try-catch]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/try...catch
[V8's stack trace API]: https://github.com/v8/v8/wiki/Stack-Trace-API
[vm]: vm.html
