# fault [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Functional errors with formatted output.

## Installation

[npm][]:

```bash
npm install fault
```

## Usage

```js
var fault = require('fault');

throw fault('Hello %s!', 'Eric');
```

Yields:

```text
Error: Hello Eric!
    at FormattedError (~/node_modules/fault/index.js:30:12)
    at Object.<anonymous> (~/example.js:3:7)
    ...
```

Or, format a float in a type error:

```js
var fault = require('fault');

throw fault.type('Who doesn’t like %f? \ud83c\udf70', Math.PI);
```

Yields:

```text
TypeError: Who doesn’t like 3.141593? 🍰
    at Function.FormattedError [as type] (~/node_modules/fault/index.js:30:12)
    at Object.<anonymous> (~/example.js:3:7)
```

## API

### `fault(format?[, values...])`

Create an error with a printf-like formatted message.

###### Parameters

*   `format` (`string`, optional)
*   `values` (`*`, optional)

###### Formatters

*   `%s` — String
*   `%b` — Binary
*   `%c` — Character
*   `%d` — Decimal
*   `%f` — Floating point
*   `%o` — Octal
*   `%x` — Lowercase hexadecimal
*   `%X` — Uppercase hexadecimal
*   `%` followed by any other character, prints that character

See [`samsonjs/format`][fmt] for argument parsing.

###### Returns

An instance of [Error][].

###### Other errors

*   `fault.eval(format?[, values...])` — [EvalError][]
*   `fault.range(format?[, values...])` — [RangeError][]
*   `fault.reference(format?[, values...])` — [ReferenceError][]
*   `fault.syntax(format?[, values...])` — [SyntaxError][]
*   `fault.type(format?[, values...])` — [TypeError][]
*   `fault.uri(format?[, values...])` — [URIError][]

#### `fault.create(Constructor)`

Factory to create instances of `ErrorConstructor` with support for formatting.
Used internally to wrap the global error constructors, exposed for custom
errors.  Returns a function just like `fault`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/fault.svg

[travis]: https://travis-ci.org/wooorm/fault

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/fault.svg

[codecov]: https://codecov.io/github/wooorm/fault

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[fmt]: https://github.com/samsonjs/format

[error]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/Error

[evalerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/EvalError

[rangeerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/RangeError

[referenceerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/ReferenceError

[syntaxerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/SyntaxError

[typeerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/TypeError

[urierror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/URIError.
