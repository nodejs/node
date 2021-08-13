# load-plugin

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]

Load a submodule, plugin, or file.
Like Node’s `require` and `require.resolve`, but from one or more places, and
optionally global too.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install load-plugin
```

## Use

Say we’re in this project (with dependencies installed):

```js
import {loadPlugin, resolvePlugin} from 'load-plugin'

main()

async function main() {
  await resolvePlugin('lint', {prefix: 'remark'})
  // => '/Users/tilde/projects/oss/load-plugin/node_modules/remark-lint/index.js'

  await resolvePlugin('@babel/function-name', {prefix: 'helper'})
  // => '/Users/tilde/projects/oss/load-plugin/node_modules/@babel/helper-function-name/lib/index.js'

  await resolvePlugin('./index.js', {prefix: 'remark'})
  // => '/Users/tilde/projects/oss/load-plugin/index.js'

  await loadPlugin('lint', {prefix: 'remark'})
  // => [Function: lint]
}
```

## API

This package exports the following identifiers: `loadPlugin`, `resolvePlugin`.
There is no default export.

### `loadPlugin(name[, options])`

Uses Node’s [resolution algorithm][algo] (through
[`import-meta-resolve`][import-meta-resolve]) to load CJS and ESM packages and
files to import `name` in each given `cwd` (and optionally the global
`node_modules` directory).

If a `prefix` is given and `name` is not a path, `$prefix-$name` is also
searched (preferring these over non-prefixed modules).
If `name` starts with a scope (`@scope/name`), the prefix is applied after it:
`@scope/$prefix-name`.

##### `options`

###### `options.prefix`

Prefix to search for (`string`, optional).

###### `options.cwd`

Place or places to search from (`string`, `Array.<string>`, default:
`process.cwd()`).

###### `options.global`

Whether to look for `name` in [global places][global] (`boolean`, optional,
defaults to whether global is detected).
If this is nullish, `load-plugin` will detect if it’s currently running in
global mode: either because it’s in Electron, or because a globally installed
package is running it.

Note: Electron runs its own version of Node instead of your system Node.
That means global packages cannot be found, unless you’ve [set-up][] a [`prefix`
in your `.npmrc`][prefix] or are using [nvm][] to manage your system node.

###### `options.key`

Identifier to take from the exports (`string` or `false`, default: `'default'`).
For example when given `'whatever'`, the value of `export const whatever = 1`
will be returned, when given `'default'`, the value of `export default …` is
used, and when `false` the whole module object is returned.

###### Returns

`Promise.<unknown>` — Promise yielding the results of `require`ing the first
path that exists.
The promise rejects if `require`ing an existing path fails, or if no existing
path exists.

### `resolvePlugin(name[, options])`

Search for `name`.
Accepts the same parameters as [`loadPlugin`][load-plugin] (except `key`) but
returns a promise resolving to an absolute path for `name` instead of importing
it.
Throws if `name` cannot be found.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/load-plugin/actions/workflows/main.yml/badge.svg

[build]: https://github.com/wooorm/load-plugin/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/load-plugin.svg

[coverage]: https://codecov.io/github/wooorm/load-plugin

[downloads-badge]: https://img.shields.io/npm/dm/load-plugin.svg

[downloads]: https://www.npmjs.com/package/load-plugin

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[global]: https://docs.npmjs.com/files/folders#node-modules

[load-plugin]: #loadpluginname-options

[prefix]: https://docs.npmjs.com/misc/config#prefix

[set-up]: https://github.com/sindresorhus/guides/blob/master/npm-global-without-sudo.md

[nvm]: https://github.com/creationix/nvm

[algo]: https://nodejs.org/api/esm.html#esm_resolution_algorithm

[import-meta-resolve]: https://github.com/wooorm/import-meta-resolve
