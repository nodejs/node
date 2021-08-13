# unified-engine

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Engine to process multiple files with [**unified**][unified], allowing users to
[configure][] from the file system.

## Projects

The following projects wrap the engine:

*   [`unified-args`][args] — Create CLIs for processors
*   [`unified-engine-gulp`][gulp] — Create Gulp plugins
*   [`unified-engine-atom`][atom] — Create Atom Linters for processors

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install unified-engine
```

## Use

The following example processes all files in the current directory with a
Markdown extension with [**remark**][remark], allows [configuration][configure]
from `.remarkrc` and `package.json` files, ignoring files from `.remarkignore`
files, and more.

```js
import {engine} from 'unified-engine'
import {remark} from 'remark'

engine(
  {
    processor: remark,
    files: ['.'],
    extensions: ['md', 'markdown', 'mkd', 'mkdn', 'mkdown'],
    pluginPrefix: 'remark',
    rcName: '.remarkrc',
    packageField: 'remarkConfig',
    ignoreName: '.remarkignore',
    color: true
  },
  done
)

function done(error) {
  if (error) throw error
}
```

## Contents

*   [API](#api)
    *   [`engine(options, callback)`](#engineoptions-callback)
*   [Plugins](#plugins)
*   [Configuration](#configuration)
*   [Ignoring](#ignoring)
*   [Contribute](#contribute)
*   [License](#license)

## API

This package exports the following identifiers: `engine`.
There is no default export.

### `engine(options, callback)`

Process files according to `options` and call [`callback`][callback] when
done.

###### [`options`][options]

*   [`processor`][processor] ([`Processor`][unified-processor])
    — unified processor to transform files
*   [`cwd`][cwd] (`string`, default: `process.cwd()`)
    — Directory to search files in, load plugins from, and more
*   [`files`][files] (`Array.<string|VFile>`, optional)
    — Paths or globs to files and directories, or virtual files, to process
*   [`extensions`][extensions] (`Array.<string>`, optional)
    — If `files` matches directories, include files with `extensions`
*   [`streamIn`][stream-in] (`ReadableStream`, default: `process.stdin`)
    — Stream to read from if no files are found or given
*   [`filePath`][file-path] (`string`, optional)
    — File path to process the given file on `streamIn` as
*   [`streamOut`][stream-out] (`WritableStream`, default: `process.stdout`)
    — Stream to write processed files to
*   [`streamError`][stream-error] (`WritableStream`, default: `process.stderr`)
    — Stream to write the report (if any) to
*   [`out`][out] (`boolean`, default: depends)
    — Whether to write the processed file to `streamOut`
*   [`output`][output] (`boolean` or `string`, default: `false`)
    — Whether to write successfully processed files, and where to
*   [`alwaysStringify`][always-stringify] (`boolean`, default: `false`)
    — Whether to always serialize successfully processed files
*   [`tree`][tree] (`boolean`, default: `false`)
    — Whether to treat both input and output as a syntax tree
*   [`treeIn`][tree-in] (`boolean`, default: `tree`)
    — Whether to treat input as a syntax tree
*   [`treeOut`][tree-out] (`boolean`, default: `tree`)
    — Whether to treat output as a syntax tree
*   [`inspect`][inspect] (`boolean`, default: `false`)
    — Whether to output a formatted syntax tree
*   [`rcName`][rc-name] (`string`, optional)
    — Name of configuration files to load
*   [`packageField`][package-field] (`string`, optional)
    — Property at which configuration can be found in `package.json` files
*   [`detectConfig`][detect-config] (`boolean`, default: whether `rcName` or
    `packageField` is given)
    — Whether to search for configuration files
*   [`rcPath`][rc-path] (`string`, optional)
    — Filepath to a configuration file to load
*   [`settings`][settings] (`Object`, optional)
    — Configuration for the parser and compiler of the processor
*   [`ignoreName`][ignore-name] (`string`, optional)
    — Name of ignore files to load
*   [`detectIgnore`][detect-ignore] (`boolean`, default: whether `ignoreName`
    is given)
    — Whether to search for ignore files
*   [`ignorePath`][ignore-path] (`string`, optional)
    — Filepath to an ignore file to load
*   [`ignorePathResolveFrom`][ignore-path-resolve-from] (`'dir'` or `'cwd'`,
    default: `'dir'`)
    — Resolve patterns in `ignorePath` from the current working directory or the
    file’s directory
*   [`ignorePatterns`][ignore-patterns] (`Array.<string>`, optional)
    — Patterns to ignore in addition to ignore files, if any
*   [`silentlyIgnore`][silently-ignore] (`boolean`, default: `false`)
    — Skip given files if they are ignored
*   [`plugins`][options-plugins] (`Array|Object`, optional)
    — Plugins to use
*   [`pluginPrefix`][plugin-prefix] (`string`, optional)
    — Optional prefix to use when searching for plugins
*   [`configTransform`][config-transform] (`Function`, optional)
    — Transform config files from a different schema
*   [`reporter`][reporter] (`string` or `function`, default:
    `import {reporter} from 'vfile-reporter'`)
    — Reporter to use
*   [`reporterOptions`][reporteroptions] (`Object?`, optional)
    — Config to pass to the used reporter
*   [`color`][color] (`boolean`, default: `false`)
    — Whether to report with ANSI color sequences
*   [`silent`][silent] (`boolean`, default: `false`)
    — Report only fatal errors
*   [`quiet`][quiet] (`boolean`, default: `silent`)
    — Do not report successful files
*   [`frail`][frail] (`boolean`, default: `false`)
    — Call back with an unsuccessful (`1`) code on warnings as well as errors

#### `function callback(error[, code, context])`

Called when processing is complete, either with a fatal error if processing went
horribly wrong (probably due to incorrect configuration), or a status code and
the processing context.

###### Parameters

*   `error` (`Error`) — Fatal error
*   `code` (`number`) — Either `0` if successful, or `1` if unsuccessful.
    The latter occurs if [fatal][] errors happen when processing individual
    files, or if [`frail`][frail] is set and warnings occur
*   `context` (`Object`) — Processing context, containing internally used
    information and a `files` array with the processed files

## Plugins

[`doc/plugins.md`][plugins] describes in detail how plugins can add more files
to be processed and handle all transformed files.

## Configuration

[`doc/configure.md`][configure] describes in detail how configuration files
work.

## Ignoring

[`doc/ignore.md`][ignore] describes in detail how ignore files work.

## Contribute

See [`contributing.md`][contributing] in [`unifiedjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/unifiedjs/unified-engine/workflows/main/badge.svg

[build]: https://github.com/unifiedjs/unified-engine/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/unifiedjs/unified-engine.svg

[coverage]: https://codecov.io/github/unifiedjs/unified-engine

[downloads-badge]: https://img.shields.io/npm/dm/unified-engine.svg

[downloads]: https://www.npmjs.com/package/unified-engine

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/unifiedjs/unified/discussions

[npm]: https://docs.npmjs.com/cli/install

[health]: https://github.com/unifiedjs/.github

[contributing]: https://github.com/unifiedjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/unifiedjs/.github/blob/HEAD/support.md

[coc]: https://github.com/unifiedjs/.github/blob/HEAD/code-of-conduct.md

[license]: license

[author]: https://wooorm.com

[unified]: https://github.com/unifiedjs/unified

[unified-processor]: https://github.com/unifiedjs/unified#processor

[remark]: https://github.com/remarkjs/remark

[fatal]: https://github.com/vfile/vfile#vfilefailreason-position-ruleid

[callback]: #function-callbackerror-code-context

[options]: doc/options.md#options

[processor]: doc/options.md#optionsprocessor

[cwd]: doc/options.md#optionscwd

[extensions]: doc/options.md#optionsextensions

[stream-in]: doc/options.md#optionsstreamin

[file-path]: doc/options.md#optionsfilepath

[stream-out]: doc/options.md#optionsstreamout

[stream-error]: doc/options.md#optionsstreamerror

[out]: doc/options.md#optionsout

[output]: doc/options.md#optionsoutput

[always-stringify]: doc/options.md#optionsalwaysstringify

[tree]: doc/options.md#optionstree

[tree-in]: doc/options.md#optionstreein

[tree-out]: doc/options.md#optionstreeout

[inspect]: doc/options.md#optionsinspect

[detect-config]: doc/options.md#optionsdetectconfig

[rc-name]: doc/options.md#optionsrcname

[package-field]: doc/options.md#optionspackagefield

[rc-path]: doc/options.md#optionsrcpath

[settings]: doc/options.md#optionssettings

[detect-ignore]: doc/options.md#optionsdetectignore

[ignore-name]: doc/options.md#optionsignorename

[ignore-path]: doc/options.md#optionsignorepath

[ignore-path-resolve-from]: doc/options.md#optionsignorepathresolvefrom

[ignore-patterns]: doc/options.md#optionsignorepatterns

[silently-ignore]: doc/options.md#optionssilentlyignore

[plugin-prefix]: doc/options.md#optionspluginprefix

[config-transform]: doc/options.md#optionsconfigtransform

[options-plugins]: doc/options.md#optionsplugins

[reporter]: doc/options.md#optionsreporter

[reporteroptions]: doc/options.md#optionsreporteroptions

[color]: doc/options.md#optionscolor

[silent]: doc/options.md#optionssilent

[quiet]: doc/options.md#optionsquiet

[frail]: doc/options.md#optionsfrail

[files]: doc/options.md#optionsfiles

[configure]: doc/configure.md

[ignore]: doc/ignore.md

[plugins]: doc/plugins.md

[atom]: https://github.com/unifiedjs/unified-engine-atom

[gulp]: https://github.com/unifiedjs/unified-engine-gulp

[args]: https://github.com/unifiedjs/unified-args
