# Amaro

Amaro is a Node.js wrapper around the [SWC](https://swc.rs) TypeScript parser and type stripper. Its Rust core in `crates/amaro` depends on the published SWC crates and is compiled to WebAssembly.
It's used as an internal in Node.js for [Type Stripping](https://nodejs.org/api/typescript.html#type-stripping) but can also be used as a standalone package.

> Amaro means "bitter" in Italian. It's a reference to [Mount Amaro](https://en.wikipedia.org/wiki/Monte_Amaro_(Abruzzo)) on whose slopes this package was conceived.

This package provides a stable API for the TypeScript parser and allows users to upgrade to the latest version of TypeScript transpiler independently from the one used internally in Node.js.

## Building from source

The WebAssembly module is not checked in. Building it requires Docker, which
runs the Rust build inside the [Node.js wasm-builder](https://github.com/nodejs/wasm-builder)
container:

```bash
npm run build:wasm
npm run build
npm test
```

Releases build the module in CI before publishing.

## How to Install

To install Amaro, run:

```shell
npm install amaro
```

## How to Use

By default Amaro exports a `transformSync` function that performs type stripping.
Stack traces are preserved, by replacing removed types with white spaces.

```javascript
const amaro = require('amaro');
const { code } = amaro.transformSync("const foo: string = 'bar';", { mode: "strip-only" });
console.log(code); // "const foo         = 'bar';"
```

### Loader

It is possible to use Amaro as an external loader to execute TypeScript files.
This allows the installed Amaro to override the Amaro version used by Node.js.
In order to use Amaro as an external loader, type stripping needs to be enabled.

```bash
node --import="amaro/strip" file.ts
```

> In node v22.18.0 and later you can omit the `--experimental-strip-types` flag, as it is enabled by default.

Enabling TypeScript feature transformation:

```bash
node --enable-source-maps --import="amaro/transform" file.ts
```

> Note that the `amaro/transform` loader should be used with `--enable-source-maps`
> to preserve accurate source-mapped stack traces.

#### Programmatic registration with `module.register()`

If you want TypeScript to "just work" in an existing codebase without passing `--import` every time, create a small bootstrap file and register Amaro once before loading your TS entrypoint.

```mjs
// bootstrap.mjs
import { register } from "node:module";

register("amaro/strip", import.meta.url);
await import("./src/index.ts");
```

Then start your app through the bootstrap file:

```bash
node --watch ./bootstrap.mjs
```

For transform mode, swap `amaro/strip` with `amaro/transform` and run Node with `--enable-source-maps`.

### Monorepo usage

Amaro makes working in monorepos smoother by removing the need to rebuild internal packages during development. When used with the [`--conditions`](https://nodejs.org/docs/latest/api/cli.html#-c-condition---conditionscondition) flag, you can reference TypeScript source files directly in exports:

```json
"exports": {
  ".": {
    "typescript": "./src/index.ts",
    "types": "./dist/index.d.ts",
    "require": "./dist/index.js",
    "import": "./dist/index.js"
 }
}
```

Then run your app with:

```bash
node --watch --import="amaro/strip" --conditions=typescript ./src/index.ts
```

This setup allows Node.js to load TypeScript files from linked packages without a build step. Changes to any package are picked up immediately, speeding up and simplifying local development in monorepos.

### TypeScript Version

The supported TypeScript version is 5.8.

## Node.js internals

`dist/internal.js` also exports the helpers Node.js uses for its REPL,
`assert` error messages and top-level await diagnostics. They are not part of
Amaro's public API and may change between releases.

- `isValidSyntax(source)` is true when the input parses as a JavaScript or
  TypeScript program, as a script or as a module. Like the REPL, it accepts
  top-level `await`, `import` and `export`. It is a parser check only: early
  errors that need scope analysis, such as duplicate `let` declarations or
  `super` outside a method, are left to the JavaScript engine. Node.js uses it
  only to decide whether an input starting with `{` is an object literal.
- `isRecoverableError(source)` is true when the input fails to parse only
  because it ended too early: every parse error is reported at the end of the
  input, or the input ends inside a template literal, a block comment, or a
  string literal whose last line ends with a backslash. Partial tokens such as
  `2e`, unterminated strings without a line continuation, errors before the end
  of the input and valid input are not recoverable.
- `tokenize(source)` lexes the whole input without parsing it and returns
  `{ kind, start, end }` tokens with UTF-16 offsets, comments included in
  source order. It stops only at a lexical error, such as an unterminated
  string, template, comment or regexp, and returns the tokens before it.
  Regexp literals are told apart from division with Acorn's token-context
  rules.
- `findTopLevelAwaits(source)` returns the one-based line and zero-based
  UTF-16 column of every `await` expression, `for await` loop and
  `await using` declaration in module scope, skipping function bodies and
  class methods. Invalid modules yield no locations.
- `getFirstExpression(source, column)` and `transformModuleSyntax(source)`
  back `assert` messages and the REPL's `import`/`export` support.

## License (MIT)

See [`LICENSE.md`](./LICENSE.md).
