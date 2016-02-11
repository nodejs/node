# ![remark](https://cdn.rawgit.com/wooorm/remark/master/logo.svg)

[![Build Status](https://img.shields.io/travis/wooorm/remark.svg)](https://travis-ci.org/wooorm/remark) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/remark.svg)](https://codecov.io/github/wooorm/remark) [![Inline docs](https://img.shields.io/badge/docs-A-brightgreen.svg)](http://inch-ci.org/github/wooorm/remark) [![Chat](https://img.shields.io/gitter/room/wooorm/remark.svg)](https://gitter.im/wooorm/remark)

> **remark** recently changed its name from **mdast**. [Read more about
> what changed and how to migrate »](https://github.com/wooorm/remark/releases/tag/3.0.0)

**remark** is a markdown processor powered by plugins. Lots of tests. Node,
io.js, and the browser. 100% coverage.

**remark** is not just another markdown to HTML compiler. It can generate,
and reformat, markdown too. It is powered by [plugins](doc/plugins.md) to do
all kinds of things: [validate your markdown](https://github.com/wooorm/remark-lint),
[add links for GitHub references](https://github.com/wooorm/remark-github), or
[add a table of contents](https://github.com/wooorm/remark-toc).

The project contains both an extensive [JavaScript API](doc/remark.3.md) for
parsing, modifying, and compiling markdown, and a friendly [Command Line
Interface](doc/remark.1.md) making it easy to validate, prepare, and compile
markdown in a build step.

## Table of Contents

*   [Installation](#installation)

*   [Usage](#usage)

*   [API](#api)

    *   [remark.process(value\[, options\]\[, done\])](#remarkprocessvalue-options-done)
    *   [remark.use(plugin\[, options\])](#remarkuseplugin-options)

*   [CLI](#cli)

*   [License](#license)

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install remark
```

[Read more about alternative ways to install and use »](doc/installation.md)

## Usage

Load dependencies:

```javascript
var remark = require('remark');
var html = require('remark-html');
var yamlConfig = require('remark-yaml-config');
```

Use plugins:

```javascript
var processor = remark().use(yamlConfig).use(html);
```

Process the document:

```javascript
var doc = processor.process([
    '---',
    'remark:',
    '  commonmark: true',
    '---',
    '',
    '2) Some *emphasis*, **strongness**, and `code`.'
].join('\n'));
```

Yields:

```html
<ol start="2">
<li>Some <em>emphasis</em>, <strong>strongness</strong>, and <code>code</code>.</li>
</ol>
```

## API

[**Get Started with the API** »](doc/getting-started.md#application-programming-interface)

### `remark.process(value[, options][, done])`

Parse a markdown document, apply plugins to it, and compile it into
something else.

**Signatures**

*   `doc = remark.process(value, options?, done?)`.

**Parameters**

*   `value` (`string`) — Markdown document;

*   `options` (`Object`) — Settings:

    *   `gfm` (`boolean`, default: `true`) — See [GitHub Flavoured Markdown](doc/remarksetting.7.md#github-flavoured-markdown);
    *   `yaml` (`boolean`, default: `true`) — See [YAML](doc/remarksetting.7.md#yaml);
    *   `commonmark` (`boolean`, default: `false`) — See [CommonMark](doc/remarksetting.7.md#commonmark);
    *   `footnotes` (`boolean`, default: `false`) — See [Footnotes](doc/remarksetting.7.md#footnotes);
    *   `pedantic` (`boolean`, default: `false`) — See [Pedantic](doc/remarksetting.7.md#pedantic);
    *   `breaks` (`boolean`, default: `false`) — See [Breaks](doc/remarksetting.7.md#breaks);
    *   `entities` (`boolean`, default: `false`) — See [Encoding Entities](doc/remarksetting.7.md#encoding-entities);
    *   `setext` (`boolean`, default: `false`) — See [Setext Headings](doc/remarksetting.7.md#setext-headings);
    *   `closeAtx` (`boolean`, default: `false`) — See [Closed ATX Headings](doc/remarksetting.7.md#closed-atx-headings);
    *   `looseTable` (`boolean`, default: `false`) — See [Loose Tables](doc/remarksetting.7.md#loose-tables);
    *   `spacedTable` (`boolean`, default: `true`) — See [Spaced Tables](doc/remarksetting.7.md#spaced-tables);
    *   `fence` (`"~"` or ``"`"``, default: ``"`"``) — See [Fence](doc/remarksetting.7.md#fence);
    *   `fences` (`boolean`, default: `false`) — See [Fences](doc/remarksetting.7.md#fences);
    *   `bullet` (`"-"`, `"*"`, or `"+"`, default: `"-"`) — See [List Item Bullets](doc/remarksetting.7.md#list-item-bullets);
    *   `listItemIndent` (`"tab"`, `"mixed"` or `"1"`, default: `"tab"`) — See [List Item Indent](doc/remarksetting.7.md#list-item-indent);
    *   `incrementListMarker` (`boolean`, default: `true`) — See [List Marker Increase](doc/remarksetting.7.md#list-marker-increase);
    *   `rule` (`"-"`, `"*"`, or `"_"`, default: `"*"`) — See [Horizontal Rules](doc/remarksetting.7.md#horizontal-rules);
    *   `ruleRepetition` (`number`, default: `3`) — See [Horizontal Rules](doc/remarksetting.7.md#horizontal-rules);
    *   `ruleSpaces` (`boolean`, default `true`) — See [Horizontal Rules](doc/remarksetting.7.md#horizontal-rules);
    *   `strong` (`"_"`, or `"*"`, default `"*"`) — See [Emphasis Markers](doc/remarksetting.7.md#emphasis-markers);
    *   `emphasis` (`"_"`, or `"*"`, default `"_"`) — See [Emphasis Markers](doc/remarksetting.7.md#emphasis-markers).
    *   `position` (`boolean`, default: `true`) — See [Position](doc/remarksetting.7.md#position);

*   `done` (`function(Error?, string?)`) — Callback invoked when the output
    is generated with either an error, or a result. Only strictly needed when
    asynchronous plugins are used.

All options (including the options object itself) can be `null` or `undefined`
to default to their default values.

**Returns**

`string` or `null`: A document. Formatted in markdown by default, or in
whatever a plugin generates.
The result is `null` if a plugin is asynchronous, in which case the callback
`done` should’ve been passed (do not worry: plugin creators make sure you know
its asynchronous).

### `remark.use(plugin[, options])`

Change the way [`remark`](#api) works by using a [`plugin`](doc/plugins.md).

**Signatures**

*   `processor = remark.use(plugin, options?)`;
*   `processor = remark.use(plugins)`.

**Parameters**

*   `plugin` (`Function`) — A [**Plugin**](doc/plugins.md);
*   `plugins` (`Array.<Function>`) — A list of [**Plugin**](doc/plugins.md)s;
*   `options` (`Object?`) — Passed to plugin. Specified by its documentation.

**Returns**

`Object`: an instance of Remark: The returned object functions just like
**remark** (it has the same methods), but caches the `use`d plugins. This
provides the ability to chain `use` calls to use multiple plugins, but
ensures the functioning of the **remark** module does not change for other
dependents.

## CLI

[**Get Started with the CLI** »](doc/getting-started.md#command-line-interface)

Install:

```bash
npm install --global remark
```

Use:

```text
Usage: remark [options] <pathspec...>

Markdown processor powered by plugins

Options:

  -h, --help                output usage information
  -V, --version             output the version number
  -o, --output [path]       specify output location
  -c, --config-path <path>  specify configuration location
  -i, --ignore-path <path>  specify ignore location
  -s, --setting <settings>  specify settings
  -u, --use <plugins>       use transform plugin(s)
  -e, --ext <extensions>    specify extensions
  -w, --watch               watch for changes and reprocess
  -a, --ast                 output AST information
  -q, --quiet               output only warnings and errors
  -S, --silent              output only errors
  -f, --frail               exit with 1 on warnings
  --file-path <path>        specify file path to process as
  --no-stdout               disable writing to stdout
  --no-color                disable color in output
  --no-rc                   disable configuration from .remarkrc
  --no-ignore               disable ignore from .remarkignore

See also: man 1 remark, man 3 remark,
  man 3 remarkplugin, man 5 remarkrc,
  man 5 remarkignore, man 7 remarksetting,
  man 7 remarkconfig, man 7 remarkplugin.

Examples:

  # Process `readme.md`
  $ remark readme.md -o readme-new.md

  # Pass stdin(4) through remark, with settings, to stdout(4)
  $ remark --setting "setext: true, bullet: \"*\"" < readme.md > readme-new.md

  # Use a plugin (with options)
  $ npm install remark-toc
  $ remark readme.md --use "toc=heading:\"contents\"" -o

  # Rewrite markdown in a directory
  $ remark . -o
```

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)

> This project was initially a fork of [marked](https://github.com/chjj/marked).

Copyright (c) 2011-2014, Christopher Jeffrey. (MIT License)
