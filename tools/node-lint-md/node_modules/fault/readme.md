# fault

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Functional errors with formatted output.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install fault
```

## Use

```js
import fault from 'fault'

throw fault('Hello %s!', 'Eric')
```

Yields:

```text
Error: Hello Eric!
    at FormattedError (~/node_modules/fault/index.js:30:12)
    at Object.<anonymous> (~/example.js:3:7)
    …
```

Or, format a float in a type error:

```js
import fault from 'fault'

throw fault.type('Who doesn’t like %f? \uD83C\uDF70', Math.PI)
```

Yields:

```text
TypeError: Who doesn’t like 3.141593? 🍰
    at Function.FormattedError [as type] (~/node_modules/fault/index.js:30:12)
    at Object.<anonymous> (~/example.js:3:7)
```

## API

This package exports the following identifiers: `fault`, `create`.
There is no default export.

### `fault(format?[, values…])`

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

An instance of [`Error`][error].

###### Other errors

*   `fault.eval(format?[, values…])` — [EvalError][]
*   `fault.range(format?[, values…])` — [RangeError][]
*   `fault.reference(format?[, values…])` — [ReferenceError][]
*   `fault.syntax(format?[, values…])` — [SyntaxError][]
*   `fault.type(format?[, values…])` — [TypeError][]
*   `fault.uri(format?[, values…])` — [URIError][]

#### `create(Constructor)`

Factory to create instances of `ErrorConstructor` with support for formatting.
Used internally to wrap the global error constructors, exposed for custom
errors.
Returns a function just like `fault`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/fault/workflows/main/badge.svg

[build]: https://github.com/wooorm/fault/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/fault.svg

[coverage]: https://codecov.io/github/wooorm/fault

[downloads-badge]: https://img.shields.io/npm/dm/fault.svg

[downloads]: https://www.npmjs.com/package/fault

[size-badge]: https://img.shields.io/bundlephobia/minzip/fault.svg

[size]: https://bundlephobia.com/result?p=fault

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[fmt]: https://github.com/samsonjs/format

[error]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/Error

[evalerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/EvalError

[rangeerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/RangeError

[referenceerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/ReferenceError

[syntaxerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/SyntaxError

[typeerror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/TypeError

[urierror]: https://developer.mozilla.org/JavaScript/Reference/Global_Objects/URIError.
