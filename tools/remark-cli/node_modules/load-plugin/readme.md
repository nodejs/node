# load-plugin [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Load a submodule, plugin, or file.  Like Node’s `require` and
`require.resolve`, but from one or more places, and optionally
global too.

## Installation

[npm][npm-install]:

```bash
npm install load-plugin
```

## Usage

Say we’re in this project (with dependencies installed):

```javascript
var load = require('load-plugin');

load.resolve('lint', {prefix: 'remark'});
// '/Users/tilde/projects/oss/load-plugin/node_modules/remark-lint/index.js'

load.resolve('./index.js', {prefix: 'remark'});
// '/Users/tilde/projects/oss/load-plugin/index.js'

load.require('lint', {prefix: 'remark'});
// [Function: lint]
```

## API

### `loadPlugin(name[, options])`

Uses the standard node module loading strategy to require `name`
in each given `cwd` (and optionally the global `node_modules`
directory).

If a prefix is given and `name` is not a path, `prefix-name`
is also searched (preferring these over non-prefixed modules).

###### `options`

*   `prefix` (`string`, optional)
    — Prefix to search for;
*   `cwd` (`string`, `Array.<string>`, default: `process.cwd()`)
    — Place or places to search from;
*   `global` (`boolean`, optional, defaults to whether global is detected)
    — Whether to look for `name` in [global places][global].
    If this is nully, `load-plugin` will detect if it’s currently
    running in global mode: either because it’s in Electron, or because
    a globally installed package is running it.

###### Returns

The results of `require`ing the first path that exists.

###### Throws

If `require`ing an existing path fails, or if no existing path exists.

### `loadPlugin.resolve(name[, options])`

Search for `name`.  Accepts the same parameters as [`loadPlugin`][load-plugin]
but returns an absolute path for `name` instead of requiring it,
and `null` if it cannot be found.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/load-plugin.svg

[travis]: https://travis-ci.org/wooorm/load-plugin

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/load-plugin.svg

[codecov]: https://codecov.io/github/wooorm/load-plugin

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[global]: https://docs.npmjs.com/files/folders#node-modules

[load-plugin]: #loadpluginname-options
