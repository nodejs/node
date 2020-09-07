# CJS Module Lexer

[![Build Status][travis-image]][travis-url]

A [very fast](#benchmarks) JS CommonJS module syntax lexer used to detect the most likely list of named exports of a CommonJS module.

Outputs the list of named exports (`exports.name = ...`), whether the `__esModule` interop flag is used, and possible module reexports (`module.exports = require('...')`).

For an example of the performance, Angular 1 (720KiB) is fully parsed in 5ms, in comparison to the fastest JS parser, Acorn which takes over 100ms.

_Comprehensively handles the JS language grammar while remaining small and fast. - ~10ms per MB of JS cold and ~5ms per MB of JS warm, [see benchmarks](#benchmarks) for more info._

### Usage

```
npm install cjs-module-lexer
```

For use in CommonJS:

```js
const { init, parse } = require('cjs-module-lexer');

(async () => {
  // Init must be called first.
  await init();

  const { exports, reexports, esModule } = parse(`
    // named exports detection
    module.exports.a = 'a';
    (function () {
      exports.b = 'b';
    })();
    Object.defineProperty(exports, 'c', { value: 'c' });
    /* exports.d = 'not detected'; */

    // reexports detection
    if (maybe) module.exports = require('./dep1.js');
    if (another) module.exports = require('./dep2.js');

    // literal exports assignments
    module.exports = { a, b: c, d, 'e': f }

    // __esModule detection
    Object.defineProperty(module.exports, '__esModule', { value: true })
  `);

  // exports === ['a', 'b', 'c', '__esModule']
  // reexports === ['./dep1.js', './dep2.js']
})();
```

### Grammar

CommonJS exports matches are run against the source token stream.

The token grammar is:

```
IDENTIFIER: As defined by ECMA-262, without support for identifier `\` escapes, filtered to remove strict reserved words:
            "implements", "interface", "let", "package", "private", "protected", "public", "static", "yield", "enum"

STRING_LITERAL: A `"` or `'` bounded ECMA-262 string literal.

IDENTIFIER_STRING: ( `"` IDENTIFIER `"` | `'` IDENTIFIER `'` )

COMMENT_SPACE: Any ECMA-262 whitespace, ECMA-262 block comment or ECMA-262 line comment

MODULE_EXPORTS: `module` COMMENT_SPACE `.` COMMENT_SPACE `exports`

EXPORTS_IDENTIFIER: MODULE_EXPORTS_IDENTIFIER | `exports`

EXPORTS_DOT_ASSIGN: EXPORTS_IDENTIFIER COMMENT_SPACE `.` COMMENT_SPACE IDENTIFIER COMMENT_SPACE `=`

EXPORTS_LITERAL_COMPUTED_ASSIGN: EXPORTS_IDENTIFIER COMMENT_SPACE `[` COMMENT_SPACE IDENTIFIER_STRING COMMENT_SPACE `]` COMMENT_SPACE `=`

EXPORTS_LITERAL_PROP: (IDENTIFIER (COMMENT_SPACE `:` COMMENT_SPACE IDENTIFIER)?) | (IDENTIFIER_STRING COMMENT_SPACE `:` COMMENT_SPACE IDENTIFIER)

EXPORTS_MEMBER: EXPORTS_DOT_ASSIGN | EXPORTS_LITERAL_COMPUTED_ASSIGN

EXPORTS_DEFINE: `Object` COMMENT_SPACE `.` COMMENT_SPACE `defineProperty COMMENT_SPACE `(` EXPORTS_IDENTIFIER COMMENT_SPACE `,` COMMENT_SPACE IDENTIFIER_STRING

EXPORTS_LITERAL: MODULE_EXPORTS COMMENT_SPACE `=` COMMENT_SPACE `{` COMMENT_SPACE (EXPORTS_LITERAL_PROP COMMENT_SPACE `,` COMMENT_SPACE)+ `}`

REQUIRE: `require` COMMENT_SPACE `(` COMMENT_SPACE STRING_LITERAL COMMENT_SPACE `)`

EXPORTS_ASSIGN: (`var` | `const` | `let`) IDENTIFIER `=` REQUIRE

MODULE_EXPORTS_ASSIGN: MODULE_EXPORTS COMMENT_SPACE `=` COMMENT_SPACE REQUIRE

EXPORT_STAR: (`__export` | `__exportStar`) `(` REQUIRE

EXPORT_STAR_LIB: `Object.keys(` IDENTIFIER$1 `).forEach(function (` IDENTIFIER$2 `) {`
  (
    `if (` IDENTIFIER$2 `===` ( `'default'` | `"default"` ) `||` IDENTIFIER$2 `===` ( '__esModule' | `"__esModule"` ) `) return` `;`? |
    `if (` IDENTIFIER$2 `!==` ( `'default'` | `"default"` ) `)`
  )
  (
    EXPORTS_IDENTIFIER `[` IDENTIFIER$2 `] =` IDENTIFIER$1 `[` IDENTIFIER$2 `]` `;`? |
    `Object.defineProperty(` EXPORTS_IDENTIFIER `, ` IDENTIFIER$2 `, { enumerable: true, get: function () { return ` IDENTIFIER$1 `[` IDENTIFIER$2 `]` `;`? } })` `;`?
  )
  `})`
```

* The returned export names are the matched `IDENTIFIER` and `IDENTIFIER_STRING` slots for all `EXPORTS_MEMBER`, `EXPORTS_DEFINE` and `EXPORTS_LITERAL` matches.
* The reexport specifiers are taken to be the `STRING_LITERAL` slots of all top-level `MODULE_EXPORTS_ASSIGN` and `EXPORT_STAR` `REQUIRE` matches as well as all `EXPORTS_ASSIGN` matches whose `IDENTIFIER` also matches the first `IDENTIFIER` in `EXPORT_STAR_LIB`

### Not Supported

#### No scope analysis:

```js
// "a" WILL be detected as an export
(function (exports) {
  exports.a = 'a'; 
})(notExports);

// "b" WONT be detected as an export
(function (m) {
  m.a = 'a';
})(exports);
```

#### `module.exports` require assignment only handled at the base-level

```js
// OK
module.exports = require('./a.js');

// OK
if (condition)
  module.exports = require('./b.js');

// NOT OK -> nested top-level detections not implemented
if (condition) {
  module.exports = require('./c.js');
}
(function () {
  module.exports = require('./d.js');
})();
```

#### No object expression parsing

```js
// These WONT be detected as exports
Object.defineProperties(exports, {
  a: { value: 'a' },
  b: { value: 'b' }
});

module.exports = {
  // These WILL be detected as exports
  a: a,
  b: b,
  
  // This WILL be detected as an export
  e: require('d'),

  // These WONT be detected as exports
  // because the object parser stops on the non-identifier
  // expression "require('d')"
  f: 'f'
}
```

#### Only specific transpiler-style star export patterns match

```js
// './x' detected as star export
var x = require('./x');
Object.keys(x).forEach(function (k) {
	if (k !== 'default') Object.defineProperty(exports, k, {
		enumerable: true,
		get: function () {
			return x[k];
		}
	});
});

// './y' detected as star export
let y = require('./y');
Object.keys(y).forEach(function (kk) {
	if (kk !== 'default') exports[kk] = y[kk];
});

// './z' NOT detected as star export
let z = require('./z');
for (const key of Object.keys(x)) {
  exports[key] = x[key];
}
```

These patterns can be updated over time to match modern transpiler outputs.

### Environment Support

Node.js 10+, and [all browsers with Web Assembly support](https://caniuse.com/#feat=wasm).

### Grammar Support

* Token state parses all line comments, block comments, strings, template strings, blocks, parens and punctuators.
* Division operator / regex token ambiguity is handled via backtracking checks against punctuator prefixes, including closing brace or paren backtracking.
* Always correctly parses valid JS source, but may parse invalid JS source without errors.

### Benchmarks

Benchmarks can be run with `npm run bench`.

Current results:

```
Cold Run, All Samples
test/samples/*.js (3057 KiB)
> 24ms

Warm Runs (average of 25 runs)
test/samples/angular.js (719 KiB)
> 5.12ms
test/samples/angular.min.js (188 KiB)
> 3.04ms
test/samples/d3.js (491 KiB)
> 4.08ms
test/samples/d3.min.js (274 KiB)
> 2.04ms
test/samples/magic-string.js (34 KiB)
> 0ms
test/samples/magic-string.min.js (20 KiB)
> 0ms
test/samples/rollup.js (902 KiB)
> 5.92ms
test/samples/rollup.min.js (429 KiB)
> 3.08ms

Warm Runs, All Samples (average of 25 runs)
test/samples/*.js (3057 KiB)
> 17.4ms
```

### Building

To build download the WASI SDK from https://github.com/CraneStation/wasi-sdk/releases.

The Makefile assumes that the `clang` in PATH corresponds to LLVM 8 (provided by WASI SDK as well, or a standard clang 8 install can be used as well), and that `../wasi-sdk-6` contains the SDK as extracted above, which is important to locate the WASI sysroot.

The build through the Makefile is then run via `make lib/lexer.wasm`, which can also be triggered via `npm run build-wasm` to create `dist/lexer.js`.

On Windows it may be preferable to use the Linux subsystem.

After the Web Assembly build, the CJS build can be triggered via `npm run build`.

Optimization passes are run with [Binaryen](https://github.com/WebAssembly/binaryen) prior to publish to reduce the Web Assembly footprint.

### License

MIT

[travis-url]: https://travis-ci.org/guybedford/es-module-lexer
[travis-image]: https://travis-ci.org/guybedford/es-module-lexer.svg?branch=master
