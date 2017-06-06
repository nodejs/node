# ECMAScript Modules

<!--introduced_in=v9.x.x-->

> Stability: 1 - Experimental

<!--name=esm-->

Node contains support for ES Modules based upon the [the Node EP for ES Modules][].

Not all features of the EP are complete and will be landing as both VM support and implementation is ready. Error messages are still being polished.

## Enabling

<!-- type=misc -->

The `--experimental-modules` flag can be used to enable features for loading ESM modules.

Once this has been set, files ending with `.mjs` will be able to be loaded as ES Modules.

```sh
node --experimental-modules my-app.mjs
```

## Features

<!-- type=misc -->

### Supported

Only the CLI argument for the main entry point to the program can be an entry point into an ESM graph. In the future `import()` can be used to create entry points into ESM graphs at run time.

### Unsupported

| Feature | Reason |
| --- | --- |
| `require('./foo.mjs')` | ES Modules have differing resolution and timing, use language standard `import()` |
| `import()` | pending newer V8 release used in Node.js |
| `import.meta` | pending V8 implementation |
| Loader Hooks | pending Node.js EP creation/consensus |

## Notable differences between `import` and `require`

### No NODE_PATH

`NODE_PATH` is not part of resolving `import` specifiers. Please use symlinks if this behavior is desired.

### No `require.extensions`

`require.extensions` is not used by `import`. The expectation is that loader hooks can provide this workflow in the future.

### No `require.cache`

`require.cache` is not used by `import`. It has a separate cache.

### URL based paths

ESM are resolved and cached based upon [URL](url.spec.whatwg.org) semantics. This means that files containing special characters such as `#` and `?` need to be escaped.

Modules will be loaded multiple times if the `import` specifier used to resolve them have a different query or fragment.

```js
import './foo?query=1'; // loads ./foo with query of "?query=1"
import './foo?query=2'; // loads ./foo with query of "?query=2"
```

For now, only modules using the `file:` protocol can be loaded.

## Interop with existing modules

All CommonJS, JSON, and C++ modules can be used with `import`.

Modules loaded this way will only be loaded once, even if their query or fragment string differs between `import` statements.

When loaded via `import` these modules will provide a single `default` export representing the value of `module.exports` at the time they finished evaluating.

```js
import fs from 'fs';
fs.readFile('./foo.txt', (err, body) => {
  if (err) {
    console.error(err);
  } else {
    console.log(body);
  }
});
```

[the Node EP for ES Modules]: https://github.com/nodejs/node-eps/blob/master/002-es-modules.md
