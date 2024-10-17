# Parser

<!--introduced_in=REPLACEME-->

> Stability: 1.0 - Early Development

<!-- source_link=lib/parser.js -->

The `node:parser` module provides utilities for working with source code. It can be accessed using:

```mjs
import parser from 'node:parser';
```

```cjs
const parser = require('node:parser');
```

## `parser.stripTypeScriptTypes(code[, options])`

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early development

* `code` {string} The code to strip type annotations from.
* `options` {Object}
  * `mode` {string} **Default:** `'strip-only'`. Possible values are:
    * `'strip-only'` Only strip type annotations without performing the transformation of TypeScript features.
    * `'transform'` Strip type annotations and transform TypeScript features to JavaScript.
  * `sourceMap` {boolean} **Default:** `false`. Only when `mode` is `'transform'`, if `true`, a source map
    will be generated for the transformed code.
  * `sourceUrl` {string}  Specifies the source url used in the source map.
* Returns: {string} The code with type annotations stripped.
  `parser.stripTypeScriptTypes()` removes type annotations from TypeScript code. It
  can be used to strip type annotations from TypeScript code before running it
  with `vm.runInContext()` or `vm.compileFunction()`.
  By default, it will throw an error if the code contains TypeScript features
  that require transformation such as `Enums`,
  see [type-stripping][] for more information.
  When mode is `'transform'`, it also transforms TypeScript features to JavaScript,
  see [transform TypeScript features][] for more information.
  When mode is `'strip-only'`, source maps are not generated, because locations are preserved.
  If `sourceMap` is provided, when mode is `'strip-only'`, an error will be thrown.

_WARNING_: The output of this function should not be considered stable across Node.js versions,
due to changes in the TypeScript parser.

```mjs
import parser from 'node:parser';
const code = 'const a: number = 1;';
const strippedCode = parser.stripTypeScriptTypes(code);
console.log(strippedCode);
// Prints: const a         = 1;
```

```cjs
const parser = require('node:parser');
const code = 'const a: number = 1;';
const strippedCode = parser.stripTypeScriptTypes(code);
console.log(strippedCode);
// Prints: const a         = 1;
```

If `sourceUrl` is provided, it will be used appended as a comment at the end of the output:

```mjs
import parser from 'node:parser';
const code = 'const a: number = 1;';
const strippedCode = parser.stripTypeScriptTypes(code, { mode: 'strip-only', sourceUrl: 'source.ts' });
console.log(strippedCode);
// Prints: const a         = 1\n\n//# sourceURL=source.ts;
```

```cjs
const parser = require('node:parser');
const code = 'const a: number = 1;';
const strippedCode = parser.stripTypeScriptTypes(code, { mode: 'strip-only', sourceUrl: 'source.ts' });
console.log(strippedCode);
// Prints: const a         = 1\n\n//# sourceURL=source.ts;
```

When `mode` is `'transform'`, the code is transformed to JavaScript:

```mjs
import parser from 'node:parser';
const code = `
  namespace MathUtil {
    export const add = (a: number, b: number) => a + b;
  }`;
const strippedCode = parser.stripTypeScriptTypes(code, { mode: 'transform', sourceMap: true });
console.log(strippedCode);
// Prints:
// var MathUtil;
// (function(MathUtil) {
//     MathUtil.add = (a, b)=>a + b;
// })(MathUtil || (MathUtil = {}));
// # sourceMappingURL=data:application/json;base64, ...
```

```cjs
const parser = require('node:parser');
const code = `
  namespace MathUtil {
    export const add = (a: number, b: number) => a + b;
  }`;
const strippedCode = parser.stripTypeScriptTypes(code, { mode: 'transform', sourceMap: true });
console.log(strippedCode);
// Prints:
// var MathUtil;
// (function(MathUtil) {
//     MathUtil.add = (a, b)=>a + b;
// })(MathUtil || (MathUtil = {}));
// # sourceMappingURL=data:application/json;base64, ...
```

[transform TypeScript features]: typescript.md#typescript-features
[type-stripping]: typescript.md#type-stripping
