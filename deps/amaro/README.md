# Amaro

Amaro is a wrapper around `@swc/wasm-typescript`, a WebAssembly port of the SWC TypeScript parser.
It's used as an internal in Node.js for [Type Stripping](https://nodejs.org/api/typescript.html#type-stripping) but can also be used as a standalone package.

> Amaro means "bitter" in Italian. It's a reference to [Mount Amaro](https://en.wikipedia.org/wiki/Monte_Amaro_(Abruzzo)) on whose slopes this package was conceived.

This package provides a stable API for the TypeScript parser and allows users to upgrade to the latest version of TypeScript transpiler independently from the one used internally in Node.js.

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

In node v23 and later you can omit the `--experimental-strip-types` flag, as it is enabled by default.

```bash
node --experimental-strip-types --import="amaro/strip" file.ts
```

Enabling TypeScript feature transformation:

```bash
node --experimental-transform-types --import="amaro/transform" file.ts
```

> Note that the "amaro/transform" loader should be used with `--experimental-transform-types` flag, or
> at least with `--enable-source-maps` flag, to preserve the original source maps.

#### Type stripping in dependencies

Contrary to the Node.js [TypeScript support](https://nodejs.org/docs/latest/api/typescript.html#type-stripping-in-dependencies), when used as a loader, Amaro handles TypeScript files inside folders under a `node_modules` path.

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

## License (MIT)

See [`LICENSE.md`](./LICENSE.md).
