# util

> Stability: 2 - Stable

The `util` module is primarily designed to support the needs of Node.js' own
internal APIs. However, many of the utilities are useful for application and
module developers as well. It can be accessed using:

```js
const util = require('util');
```

## util.debuglog(section)

* `section` {String} A string identifying the portion of the application for
  which the `debuglog` function is being created.
* Returns: {Function} The logging function

The `util.debuglog()` method is used to create a function that conditionally
writes debug messages to `stderr` based on the existence of the `NODE_DEBUG`
environment variable.  If the `section` name appears within the value of that
environment variable, then the returned function operates similar to
[`console.error()`][].  If not, then the returned function is a no-op.

For example:

```js
const util = require('util');
const debuglog = util.debuglog('foo');

debuglog('hello from foo [%d]', 123);
```

If this program is run with `NODE_DEBUG=foo` in the environment, then
it will output something like:

```txt
FOO 3245: hello from foo [123]
```

where `3245` is the process id.  If it is not run with that
environment variable set, then it will not print anything.

Multiple comma-separated `section` names may be specified in the `NODE_DEBUG`
environment variable. For example: `NODE_DEBUG=fs,net,tls`.

## util.deprecate(function, string)

The `util.deprecate()` method wraps the given `function` or class in such a way that
it is marked as deprecated.

```js
const util = require('util');

exports.puts = util.deprecate(function() {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdout.write(arguments[i] + '\n');
  }
}, 'util.puts: Use console.log instead');
```

When called, `util.deprecate()` will return a function that will emit a
`DeprecationWarning` using the `process.on('warning')` event. By default,
this warning will be emitted and printed to `stderr` exactly once, the first
time it is called. After the warning is emitted, the wrapped `function`
is called.

If either the `--no-deprecation` or `--no-warnings` command line flags are
used, or if the `process.noDeprecation` property is set to `true` *prior* to
the first deprecation warning, the `util.deprecate()` method does nothing.

If the `--trace-deprecation` or `--trace-warnings` command line flags are set,
or the `process.traceDeprecation` property is set to `true`, a warning and a
stack trace are printed to `stderr` the first time the deprecated function is
called.

If the `--throw-deprecation` command line flag is set, or the
`process.throwDeprecation` property is set to `true`, then an exception will be
thrown when the deprecated function is called.

The `--throw-deprecation` command line flag and `process.throwDeprecation`
property take precedence over `--trace-deprecation` and
`process.traceDeprecation`.

## util.format(format[, ...])

* `format` {string} A `printf`-like format string.

The `util.format()` method returns a formatted string using the first argument
as a `printf`-like format.

The first argument is a string containing zero or more *placeholder* tokens.
Each placeholder token is replaced with the converted value from the
corresponding argument. Supported placeholders are:

* `%s` - String.
* `%d` - Number (both integer and float).
* `%j` - JSON.  Replaced with the string `'[Circular]'` if the argument
contains circular references.
* `%%` - single percent sign (`'%'`). This does not consume an argument.

If the placeholder does not have a corresponding argument, the placeholder is
not replaced.

```js
util.format('%s:%s', 'foo');
  // Returns 'foo:%s'
```

If there are more arguments passed to the `util.format()` method than the
number of placeholders, the extra arguments are coerced into strings (for
objects and symbols, `util.inspect()` is used) then concatenated to the
returned string, each delimited by a space.

```js
util.format('%s:%s', 'foo', 'bar', 'baz'); // 'foo:bar baz'
```

If the first argument is not a format string then `util.format()` returns
a string that is the concatenation of all arguments separated by spaces.
Each argument is converted to a string using `util.inspect()`.

```js
util.format(1, 2, 3); // '1 2 3'
```

## util.inherits(constructor, superConstructor)

_Note: usage of `util.inherits()` is discouraged. Please use the ES6 `class` and
`extends` keywords to get language level inheritance support. Also note that
the two styles are [semantically incompatible][]._

* `constructor` {Function}
* `superConstructor` {Function}

Inherit the prototype methods from one [constructor][] into another.  The
prototype of `constructor` will be set to a new object created from
`superConstructor`.

As an additional convenience, `superConstructor` will be accessible
through the `constructor.super_` property.

```js
const util = require('util');
const EventEmitter = require('events');

function MyStream() {
  EventEmitter.call(this);
}

util.inherits(MyStream, EventEmitter);

MyStream.prototype.write = function(data) {
  this.emit('data', data);
};

const stream = new MyStream();

console.log(stream instanceof EventEmitter); // true
console.log(MyStream.super_ === EventEmitter); // true

stream.on('data', (data) => {
  console.log(`Received data: "${data}"`);
});
stream.write('It works!'); // Received data: "It works!"
```

## util.inspect(object[, options])

* `object` {any} Any JavaScript primitive or Object.
* `options` {Object}
  * `showHidden` {boolean} If `true`, the `object`'s non-enumerable symbols and
    properties will be included in the formatted result. Defaults to `false`.
  * `depth` {number} Specifies the number of times to recurse while formatting
    the `object`. This is useful for inspecting large complicated objects.
    Defaults to `2`. To make it recurse indefinitely pass `null`.
  * `colors` {boolean} If `true`, the output will be styled with ANSI color
    codes. Defaults to `false`. Colors are customizable, see
    [Customizing `util.inspect` colors][].
  * `customInspect` {boolean} If `false`, then custom `inspect(depth, opts)`
    functions exported on the `object` being inspected will not be called.
    Defaults to `true`.
  * `showProxy` {boolean} If `true`, then objects and functions that are
    `Proxy` objects will be introspected to show their `target` and `handler`
    objects. Defaults to `false`.
  * `maxArrayLength` {number} Specifies the maximum number of array and
    `TypedArray` elements to include when formatting. Defaults to `100`. Set to
    `null` to show all array elements. Set to `0` or negative to show no array
    elements.
  * `breakLength` {number} The length at which an object's keys are split
    across multiple lines. Set to `Infinity` to format an object as a single
    line. Defaults to 60 for legacy compatibility.

The `util.inspect()` method returns a string representation of `object` that is
primarily useful for debugging. Additional `options` may be passed that alter
certain aspects of the formatted string.

The following example inspects all properties of the `util` object:

```js
const util = require('util');

console.log(util.inspect(util, { showHidden: true, depth: null }));
```

Values may supply their own custom `inspect(depth, opts)` functions, when
called these receive the current `depth` in the recursive inspection, as well as
the options object passed to `util.inspect()`.

### Customizing `util.inspect` colors

<!-- type=misc -->

Color output (if enabled) of `util.inspect` is customizable globally
via the `util.inspect.styles` and `util.inspect.colors` properties.

`util.inspect.styles` is a map associating a style name to a color from
`util.inspect.colors`.

The default styles and associated colors are:

 * `number` - `yellow`
 * `boolean` - `yellow`
 * `string` - `green`
 * `date` - `magenta`
 * `regexp` - `red`
 * `null` - `bold`
 * `undefined` - `grey`
 * `special` - `cyan` (only applied to functions at this time)
 * `name` - (no styling)

The predefined color codes are: `white`, `grey`, `black`, `blue`, `cyan`,
`green`, `magenta`, `red` and `yellow`. There are also `bold`, `italic`,
`underline` and `inverse` codes.

Color styling uses ANSI control codes that may not be supported on all
terminals.

### Custom `inspect()` function on Objects

<!-- type=misc -->

Objects may also define their own `inspect(depth, opts)` function that
`util.inspect()` will invoke and use the result of when inspecting the object:

```js
const util = require('util');

const obj = { name: 'nate' };
obj.inspect = function(depth) {
  return `{${this.name}}`;
};

util.inspect(obj);
  // "{nate}"
```

Custom `inspect(depth, opts)` functions typically return a string but may
return a value of any type that will be formatted accordingly by
`util.inspect()`.

```js
const util = require('util');

const obj = { foo: 'this will not show up in the inspect() output' };
obj.inspect = function(depth) {
  return { bar: 'baz' };
};

util.inspect(obj);
  // "{ bar: 'baz' }"
```

### util.inspect.defaultOptions

The `defaultOptions` value allows customization of the default options used by
`util.inspect`. This is useful for functions like `console.log` or
`util.format` which implicitly call into `util.inspect`. It shall be set to an
object containing one or more valid [`util.inspect()`][] options. Setting
option properties directly is also supported.

```js
const util = require('util');
const arr = Array(101);

console.log(arr); // logs the truncated array
util.inspect.defaultOptions.maxArrayLength = null;
console.log(arr); // logs the full array
```

## Deprecated APIs

The following APIs have been deprecated and should no longer be used. Existing
applications and modules should be updated to find alternative approaches.

### util.debug(string)

> Stability: 0 - Deprecated: Use [`console.error()`][] instead.

* `string` {string} The message to print to `stderr`

Deprecated predecessor of `console.error`.

### util.error([...])

> Stability: 0 - Deprecated: Use [`console.error()`][] instead.

* `string` {string} The message to print to `stderr`

Deprecated predecessor of `console.error`.

### util.isArray(object)

> Stability: 0 - Deprecated

* `object` {any}

Internal alias for [`Array.isArray`][].

Returns `true` if the given `object` is an `Array`. Otherwise, returns `false`.

```js
const util = require('util');

util.isArray([]);
  // true
util.isArray(new Array);
  // true
util.isArray({});
  // false
```

### util.isBoolean(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `Boolean`. Otherwise, returns `false`.

```js
const util = require('util');

util.isBoolean(1);
  // false
util.isBoolean(0);
  // false
util.isBoolean(false);
  // true
```

### util.isBuffer(object)

> Stability: 0 - Deprecated: Use [`Buffer.isBuffer()`][] instead.

* `object` {any}

Returns `true` if the given `object` is a `Buffer`. Otherwise, returns `false`.

```js
const util = require('util');

util.isBuffer({ length: 0 });
  // false
util.isBuffer([]);
  // false
util.isBuffer(Buffer.from('hello world'));
  // true
```

### util.isDate(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `Date`. Otherwise, returns `false`.

```js
const util = require('util');

util.isDate(new Date());
  // true
util.isDate(Date());
  // false (without 'new' returns a String)
util.isDate({});
  // false
```

### util.isError(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is an [`Error`][]. Otherwise, returns
`false`.

```js
const util = require('util');

util.isError(new Error());
  // true
util.isError(new TypeError());
  // true
util.isError({ name: 'Error', message: 'an error occurred' });
  // false
```

Note that this method relies on `Object.prototype.toString()` behavior. It is
possible to obtain an incorrect result when the `object` argument manipulates
`@@toStringTag`.

```js
const util = require('util');
const obj = { name: 'Error', message: 'an error occurred' };

util.isError(obj);
  // false
obj[Symbol.toStringTag] = 'Error';
util.isError(obj);
  // true
```

### util.isFunction(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `Function`. Otherwise, returns
`false`.

```js
const util = require('util');

function Foo() {}
const Bar = function() {};

util.isFunction({});
  // false
util.isFunction(Foo);
  // true
util.isFunction(Bar);
  // true
```

### util.isNull(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is strictly `null`. Otherwise, returns
`false`.

```js
const util = require('util');

util.isNull(0);
  // false
util.isNull(undefined);
  // false
util.isNull(null);
  // true
```

### util.isNullOrUndefined(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is `null` or `undefined`. Otherwise,
returns `false`.

```js
const util = require('util');

util.isNullOrUndefined(0);
  // false
util.isNullOrUndefined(undefined);
  // true
util.isNullOrUndefined(null);
  // true
```

### util.isNumber(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `Number`. Otherwise, returns `false`.

```js
const util = require('util');

util.isNumber(false);
  // false
util.isNumber(Infinity);
  // true
util.isNumber(0);
  // true
util.isNumber(NaN);
  // true
```

### util.isObject(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is strictly an `Object` __and__ not a
`Function`. Otherwise, returns `false`.

```js
const util = require('util');

util.isObject(5);
  // false
util.isObject(null);
  // false
util.isObject({});
  // true
util.isObject(function(){});
  // false
```

### util.isPrimitive(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a primitive type. Otherwise, returns
`false`.

```js
const util = require('util');

util.isPrimitive(5);
  // true
util.isPrimitive('foo');
  // true
util.isPrimitive(false);
  // true
util.isPrimitive(null);
  // true
util.isPrimitive(undefined);
  // true
util.isPrimitive({});
  // false
util.isPrimitive(function() {});
  // false
util.isPrimitive(/^$/);
  // false
util.isPrimitive(new Date());
  // false
```

### util.isRegExp(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `RegExp`. Otherwise, returns `false`.

```js
const util = require('util');

util.isRegExp(/some regexp/);
  // true
util.isRegExp(new RegExp('another regexp'));
  // true
util.isRegExp({});
  // false
```

### util.isString(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `string`. Otherwise, returns `false`.

```js
const util = require('util');

util.isString('');
  // true
util.isString('foo');
  // true
util.isString(String('foo'));
  // true
util.isString(5);
  // false
```

### util.isSymbol(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is a `Symbol`. Otherwise, returns `false`.

```js
const util = require('util');

util.isSymbol(5);
  // false
util.isSymbol('foo');
  // false
util.isSymbol(Symbol('foo'));
  // true
```

### util.isUndefined(object)

> Stability: 0 - Deprecated

* `object` {any}

Returns `true` if the given `object` is `undefined`. Otherwise, returns `false`.

```js
const util = require('util');

const foo = undefined;
util.isUndefined(5);
  // false
util.isUndefined(foo);
  // true
util.isUndefined(null);
  // false
```

### util.log(string)

> Stability: 0 - Deprecated: Use a third party module instead.

* `string` {string}

The `util.log()` method prints the given `string` to `stdout` with an included
timestamp.

```js
const util = require('util');

util.log('Timestamped message.');
```

### util.print([...])

> Stability: 0 - Deprecated: Use [`console.log()`][] instead.

Deprecated predecessor of `console.log`.

### util.puts([...])

> Stability: 0 - Deprecated: Use [`console.log()`][] instead.

Deprecated predecessor of `console.log`.

### util._extend(obj)

> Stability: 0 - Deprecated: Use [`Object.assign()`] instead.

The `util._extend()` method was never intended to be used outside of internal
Node.js modules. The community found and used it anyway.

It is deprecated and should not be used in new code. JavaScript comes with very
similar built-in functionality through [`Object.assign()`].

[`Array.isArray`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/isArray
[constructor]: https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Object/constructor
[semantically incompatible]: https://github.com/nodejs/node/issues/4179
[`util.inspect()`]: #util_util_inspect_object_options
[Customizing `util.inspect` colors]: #util_customizing_util_inspect_colors
[`Error`]: errors.html#errors_class_error
[`console.log()`]: console.html#console_console_log_data
[`console.error()`]: console.html#console_console_error_data
[`Buffer.isBuffer()`]: buffer.html#buffer_class_method_buffer_isbuffer_obj
[`Object.assign()`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Object/assign
