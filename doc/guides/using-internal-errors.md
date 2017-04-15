# Using the internal/errors.js Module

## What is internal/errors.js

The `require('internal/errors')` module is an internal-only module that can be
used to produce `Error`, `TypeError` and `RangeError` instances that use a
static, permanent error code and an optionally parameterized message.

The intent of the module is to allow errors provided by Node.js to be assigned a
permanent identifier. Without a permanent identifier, userland code may need to
inspect error messages to distinguish one error from another. An unfortunate
result of that practice is that changes to error messages result in broken code
in the ecosystem. For that reason, Node.js has considered error message changes
to be breaking changes. By providing a permanent identifier for a specific
error, we reduce the need for userland code to inspect error messages.

*Note*: Switching an existing error to use the `internal/errors` module must be
considered a `semver-major` change. However, once using `internal/errors`,
changes to `internal/errors` error messages will be handled as `semver-minor`
or `semver-patch`.

## Using internal/errors.js

The `internal/errors` module exposes three custom `Error` classes that
are intended to replace existing `Error` objects within the Node.js source.

For instance, an existing `Error` such as:

```js
  var err = new TypeError('Expected string received ' + type);
```

Can be replaced by first adding a new error key into the `internal/errors.js`
file:

```js
E('FOO', 'Expected string received %s');
```

Then replacing the existing `new TypeError` in the code:

```js
  const errors = require('internal/errors');
  // ...
  var err = new errors.TypeError('FOO', type);
```

## Adding new errors

New static error codes are added by modifying the `internal/errors.js` file
and appending the new error codes to the end using the utility `E()` method.

```js
E('EXAMPLE_KEY1', 'This is the error value');
E('EXAMPLE_KEY2', (a, b) => return `${a} ${b}`);
```

The first argument passed to `E()` is the static identifier. The second
argument is either a String with optional `util.format()` style replacement
tags (e.g. `%s`, `%d`), or a function returning a String. The optional
additional arguments passed to the `errors.message()` function (which is
used by the `errors.Error`, `errors.TypeError` and `errors.RangeError` classes),
will be used to format the error message.

## Documenting new errors

Whenever a new static error code is added and used, corresponding documentation
for the error code should be added to the `doc/api/errors.md` file. This will
give users a place to go to easily look up the meaning of individual error
codes.


## API

### Class: errors.Error(key[, args...])

* `key` {String} The static error identifier
* `args...` {Any} Zero or more optional arguments

```js
const errors = require('internal/errors');

var arg1 = 'foo';
var arg2 = 'bar';
const myError = new errors.Error('KEY', arg1, arg2);
throw myError;
```

The specific error message for the `myError` instance will depend on the
associated value of `KEY` (see "Adding new errors").

The `myError` object will have a `code` property equal to the `key` and a
`name` property equal to `Error[${key}]`.

### Class: errors.TypeError(key[, args...])

* `key` {String} The static error identifier
* `args...` {Any} Zero or more optional arguments

```js
const errors = require('internal/errors');

var arg1 = 'foo';
var arg2 = 'bar';
const myError = new errors.TypeError('KEY', arg1, arg2);
throw myError;
```

The specific error message for the `myError` instance will depend on the
associated value of `KEY` (see "Adding new errors").

The `myError` object will have a `code` property equal to the `key` and a
`name` property equal to `TypeError[${key}]`.

### Class: errors.RangeError(key[, args...])

* `key` {String} The static error identifier
* `args...` {Any} Zero or more optional arguments

```js
const errors = require('internal/errors');

var arg1 = 'foo';
var arg2 = 'bar';
const myError = new errors.RangeError('KEY', arg1, arg2);
throw myError;
```

The specific error message for the `myError` instance will depend on the
associated value of `KEY` (see "Adding new errors").

The `myError` object will have a `code` property equal to the `key` and a
`name` property equal to `RangeError[${key}]`.

### Method: errors.message(key, args)

* `key` {String} The static error identifier
* `args` {Array} Zero or more optional arguments passed as an Array
* Returns: {String}

Returns the formatted error message string for the given `key`.
