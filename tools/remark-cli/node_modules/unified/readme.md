# unified [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

**unified** is an interface for processing text using syntax trees.
It’s what powers [**remark**][remark], [**retext**][retext], and
[**rehype**][rehype], but it also allows for processing between
multiple syntaxes.

## Installation

[npm][npm-install]:

```bash
npm install unified
```

**unified** is also available as an AMD, CommonJS, and globals module,
[uncompressed and compressed][releases].

## Usage

```js
var unified = require('unified');
var markdown = require('remark-parse');
var toc = require('remark-toc');
var remark2rehype = require('remark-rehype');
var document = require('rehype-document');
var html = require('rehype-stringify');

process.stdin
  .pipe(unified())
  .use(markdown)
  .use(toc)
  .use(remark2rehype)
  .use(document)
  .use(html)
  .pipe(process.stdout);
```

## Table of Contents

*   [Description](#description)
*   [API](#api)
    *   [processor()](#processor)
    *   [processor.use(plugin\[, options\])](#processoruseplugin-options)
    *   [processor.parse(file|value\[, options\])](#processorparsefilevalue-options)
    *   [processor.stringify(node\[, file\]\[, options\])](#processorstringifynode-file-options)
    *   [processor.run(node\[, file\]\[, done\])](#processorrunnode-file-done)
    *   [processor.process(file|value\[, options\]\[, done\])](#processorprocessfilevalue-options-done)
    *   [processor.write(chunk\[, encoding\]\[, callback\])](#processorwritechunk-encoding-callback)
    *   [processor.end()](#processorend)
    *   [processor.pipe(stream\[, options\])](#processorpipestream-options)
    *   [processor.data(key\[, value\])](#processordatakey-value)
    *   [processor.abstract()](#processorabstract)
*   [License](#license)

## Description

**unified** is an interface for processing text using syntax trees.
Syntax trees are a representation understandable to programs.
Those programs, called [**plug-in**][plugin]s, take these trees and
modify them, amongst other things.  To get to the syntax tree from
input text, there’s a [**parser**][parser], and, to get from that
back to text, there’s a [**compiler**][compiler].  This is the
[**process**][process] of a **processor**.

```ascii
                     ┌──────────────┐
                  ┌─ │ Transformers │ ─┐
                  ▲  └──────────────┘  ▼
                  └────────┐  ┌────────┘
                           │  │
            ┌────────┐     │  │     ┌──────────┐
  Input ──▶ │ Parser │ ──▶ Tree ──▶ │ Compiler │ ──▶ Output
            └────────┘              └──────────┘
```

###### Processors

Every processor implements another processor.  To create a new
processor, invoke another processor.  This creates a processor that is
configured to function the same as its ancestor.  But, when
the descendant processor is configured in the future, that
configuration does not change the ancestral processor.

Often, when processors are exposed from a library (for example,
unified itself), they should not be modified directly, as that
would change their behaviour for all users.  Those processors are
[**abstract**][abstract], and they should be made concrete before
they are used, by invoking them.

###### Node

The syntax trees used in **unified** are [**Unist**][unist] nodes,
which are plain JavaScript objects with a `type` property.  The
semantics of those `type`s are defined by other projects.

There are several [utilities][unist-utilities] for working with these
nodes.

###### List of Processors

The following projects process different syntax trees.  They parse
text to their respective syntax tree, and they compile their syntax
trees back to text.  These processors can be used as-is, or their
parsers and compilers can be mixed and matched with other plug-ins
to process between different syntaxes.

*   [**rehype**][rehype] ([**HAST**][hast]) — HTML;
*   [**remark**][remark] ([**MDAST**][mdast]) — Markdown;
*   [**retext**][retext] ([**NLCST**][nlcst]) — Natural language.

###### File

When processing documents, metadata is often gathered about that
document.  [**VFile**][vfile] is a virtual file format which stores
data, and handles metadata and messages for **unified** and its
plug-ins.

There are several [utilities][vfile-utilities] for working with these
files.

###### Configuration

To configure a processor, invoke its [`use`][use] method, supply it a
[**plug-in**][plugin], and optionally settings.

###### Integrations

**unified** can integrate with the file-system through
[**unified-engine**][engine].  On top of that, CLI apps can be created
with [**unified-args**][args], Gulp plug-ins with
[**unified-engine-gulp**][gulp], and Atom Linters with
[**unified-engine-atom**][atom].

###### Streaming

**unified** provides a streaming interface which allows it to plug
into transformations outside of itself.  An example, which reads
markdown as input, adds a table of content, and writes it out, would
be as follows:

```js
var unified = require('unified');
var markdown = require('remark-parse');
var stringify = require('remark-stringify');
var toc = require('remark-toc');

process.stdin
  .pipe(unified())
  .use(parse)
  .use(toc)
  .use(stringify)
  .pipe(process.stdout);
```

Which when given on **stdin**(4):

```md
# Alpha

## Table of Content

## Bravo
```

Yields on **stdout**(4):

```md
# Alpha

## Table of Content

*   [Bravo](#bravo)

## Bravo
```

###### Programming interface

Next to streaming, there’s also a programming interface, which gives
access to processing metadata (such as lint messages), and supports
multiple passed through files:

```js
var unified = require('unified');
var markdown = require('remark-parse');
var lint = require('remark-lint');
var remark2retext = require('remark-retext');
var english = require('retext-english');
var equality = require('retext-equality');
var remark2rehype = require('remark-rehype');
var html = require('rehype-stringify');
var report = require('vfile-reporter');

unified()
  .use(markdown)
  .use(lint)
  .use(remark2retext, unified().use(english).use(equality))
  .use(remark2rehype)
  .use(html)
  .process('## Hey guys', function (err, file) {
    console.err(report(err || file));
    console.log(file.toString());
  });
```

Which yields:

```txt
   1:1-1:12  warning  First heading level should be `1`                                    first-heading-level
   1:8-1:12  warning  `guys` may be insensitive, use `people`, `persons`, `folks` instead  gals-men

⚠ 3 warnings
<h2>Hey guys</h2>
```

###### Processing between syntaxes

The processors can be combined in two modes.

**Bridge** mode transforms the syntax tree from one flavour (the origin)
to another (the destination).  Then, transformations are applied on that
tree.  Finally, the origin processor continues transforming the original
syntax tree.

**Mutate** mode also transforms the syntax tree from one flavour to
another.  But then the origin processor continues transforming the
destination syntax tree.

In the previous example (“Programming interface”), `remark-retext` is
used in bridge mode: the origin syntax tree is kept after retext is
finished; whereas `remark-rehype` is used in mutate mode: it sets a
new syntax tree and discards the original.

*   [**remark-retext**][remark-retext].
*   [**remark-rehype**][remark-rehype].

## API

### `processor()`

Object describing how to process text.

###### Returns

`Function` — A new [**concrete**][abstract] processor which is
configured to function the same as its ancestor.  But, when the
descendant processor is configured in the future, that configuration
does not change the ancestral processor.

###### Example

The following example shows how a new processor can be created (from
the remark processor) and linked to **stdin**(4) and **stdout**(4).

```js
var remark = require('remark');

process.stdin.pipe(remark()).pipe(process.stdout);
```

### `processor.use(plugin[, options])`

Configure the processor to use a [**plug-in**][plugin], and configure
that plug-in with optional options.

###### Signatures

*   `processor.use(plugin[, options])`;
*   `processor.use(plugins[, options])`;
*   `processor.use(list)`;
*   `processor.use(matrix)`.

###### Parameters

*   `plugin` ([`Plugin`][plugin]);
*   `options` (`*`, optional) — Configuration for `plugin`.
*   `plugins` (`Array.<Function>`) — List of plugins;
*   `list` (`Array`) — `plugin` and `options` in an array;
*   `matrix` (`Array`) — array where each entry is a `list`;

###### Returns

`processor` — The processor on which `use` is invoked.

#### `Plugin`

A **unified** plugin changes the way the applied-on processor works,
in the following ways:

*   It modifies the [**processor**][processor]: such as changing the
    parser, the compiler, or linking the processor to other processors;
*   It transforms the [**syntax tree**][node] representation of a file;
*   It modifies metadata of a file.

Plug-in’s are a concept which materialise as [**attacher**][attacher]s.

#### `function attacher(processor[, options])`

An attacher is the thing passed to [`use`][use].  It configures the
processor and in turn can receive options.

Attachers can configure processors, such as by interacting with parsers
and compilers, linking them to other processors, or by specifying how
the syntax tree is handled.

###### Parameters

*   `processor` ([`processor`][processor]) — Context on which it’s used;
*   `options` (`*`, optional) — Configuration.

###### Returns

[`transformer`][transformer] — Optional.

#### `function transformer(node, file[, next])`

Transformers modify the syntax tree or metadata of a file.
A transformer is a function which is invoked each time a file is
passed through the transform phase.  If an error occurs (either
because it’s thrown, returned, rejected, or passed to [`next`][next]),
the process stops.

The transformation process in **unified** is handled by [`trough`][trough],
see it’s documentation for the exact semantics of transformers.

###### Parameters

*   `node` ([**Node**][node]);
*   `file` ([**VFile**][file]);
*   `next` ([`Function`][next], optional).

###### Returns

*   `Error` — Can be returned to stop the process;
*   [**Node**][node] — Can be returned and results in further
    transformations and `stringify`s to be performed on the new
    tree;
*   `Promise` — If a promise is returned, the function is asynchronous,
    and **must** be resolved (optionally with a [**Node**][node]) or
    rejected (optionally with an `Error`).

##### `function next(err[, tree[, file]])`

If the signature of a transformer includes `next` (third argument),
the function **may** finish asynchronous, and **must** invoke `next()`.

###### Parameters

*   `err` (`Error`, optional) — Stop the process;
*   `node` ([**Node**][node], optional) — New syntax tree;
*   `file` ([**VFile**][file], optional) — New virtual file.

### `processor.parse(file|value[, options])`

Parse text to a syntax tree.

###### Parameters

*   `file` ([**VFile**][file]);
    — Or anything which can be given to `vfile()`.
*   `options` (`Object`, optional) — Configuration given to the parser.

###### Returns

[**Node**][node] — Syntax tree representation of input.

#### `processor.Parser`

Constructor handling the parsing of text to a syntax tree.
It’s instantiated by the [**parse**][parse] phase in the process
with a [**VFile**][file], `settings`, and the processor.

The instance must expose a `parse` method which is invoked without
arguments, and must return a syntax tree representation of the
[**VFile**][file].

### `processor.stringify(node[, file][, options])`

Compile a syntax tree to text.

###### Parameters

*   `node` ([**Node**][node]);
*   `file` ([**VFile**][file], optional);
    — Or anything which can be given to `vfile()`.
*   `options` (`Object`, optional) — Configuration given to the parser.

###### Returns

`string` — String representation of the syntax tree file.

#### `processor.Compiler`

Constructor handling the compilation of a syntax tree to text.
It’s instantiated by the [**stringify**][stringify] phase in the
process with a [**VFile**][file], `settings`, and the processor.

The instance must expose a `compile` method which is invoked with
the syntax tree, and must return a string representation of that
syntax tree.

### `processor.run(node[, file][, done])`

Transform a syntax tree by applying [**plug-in**][plugin]s to it.

If asynchronous [**plug-in**][plugin]s are configured, an error
is thrown if [`done`][run-done] is not supplied.

###### Parameters

*   `node` ([**Node**][node]);
*   `file` ([**VFile**][file], optional);
    — Or anything which can be given to `vfile()`.
*   `done` ([`Function`][run-done], optional).

###### Returns

[**Node**][node] — The given syntax tree.

##### `function done(err[, node, file])`

Invoked when transformation is complete.  Either invoked with an
error, or a syntax tree and a file.

###### Parameters

*   `err` (`Error`) — Fatal error;
*   `node` ([**Node**][node]);
*   `file` ([**VFile**][file]).

### `processor.process(file|value[, options][, done])`

Process the given representation of a file as configured on the
processor.  The process invokes `parse`, `run`, and `stringify`
internally.

If asynchronous [**plug-in**][plugin]s are configured, an error
is thrown if [`done`][process-done] is not supplied.

###### Parameters

*   `file` ([**VFile**][file]);
*   `value` (`string`) — String representation of a file;
*   `options` (`Object`, optional) — Configuration for both the parser
    and compiler;
*   `done` ([`Function`][process-done], optional).

###### Returns

[**VFile**][file] — Virtual file with modified [`contents`][vfile-contents].

#### `function done(err, file)`

Invoked when the process is complete.  Invoked with a fatal error, if
any, and the [**VFile**][file].

###### Parameters

*   `err` (`Error`, optional) — Fatal error;
*   `file` ([**VFile**][file]).

### `processor.write(chunk[, encoding][, callback])`

> **Note**: Although the interface is compatible with streams,
> all data is currently buffered and passed through in one go.
> This might be changed later.

Write data to the in-memory buffer.

###### Parameters

*   `chunk` ([`Buffer`][buffer] or `string`);
*   `encoding` (`string`, defaults to `utf8`);
*   `callback` (`Function`) — Invoked on successful write.

###### Returns

`boolean` — Whether the write was successful (currently, always true).

### `processor.end()`

Signal the writing is complete.  Passes all arguments to a final
[`write`][write], and starts the process (using, when available,
options given to [`pipe`][pipe]).

###### Events

*   `data` (`string`)
    — When the process was successful, triggered with the compiled
    file;
*   `error` (`Error`)
    — When the process was unsuccessful, triggered with the fatal
    error;
*   `warning` ([`VFileMessage`][vfilemessage])
    — Each message created by the plug-ins in the process is triggered
    and separately passed.

###### Returns

`boolean` — Whether the write was successful (currently, always true).

### `processor.pipe(stream[, options])`

> **Note**: This does not pass all processed data (e.g., from loose
> `process()` calls) to the destination stream.  There’s one process
> created internally especially for streams.  Only data piped into
> the processor is piped out.

Pipe data streamed into the processor, processed, to the destination
stream.  Optionally also set the configuration for how the data
is processed.  Calls [`Stream#pipe`][stream-pipe] with the given
arguments under the hood.

###### Parameters

*   `stream` ([`WritableStream`][writable-stream]);
*   `options` (`Object`, optional) — Configuration for process and
    `stream.pipe`.

###### Returns

[`WritableStream`][writable-stream] — The given stream.

### `processor.data(key[, value])`

Get or set information in an in-memory key-value store accessible to
all phases of the process.  An example is a list of HTML elements
which are self-closing (i.e., do not need a closing tag), which is
needed when parsing, transforming, and compiling HTML.

###### Parameters

*   `key` (`string`) — Identifier;
*   `value` (`*`, optional) — Value to set.  Omit if getting `key`.

###### Returns

*   `processor` — If setting, the processor on which `data` is invoked;
*   `*` — If getting, the value at `key`.

###### Example

The following example show how to get and set information:

```js
var unified = require('unified');

console.log(unified().data('alpha', 'bravo').data('alpha'))
```

Yields:

```txt
bravo
```

### `processor.abstract()`

Turn a processor into an abstract processor.  Abstract processors
are meant to be extended, and not to be configured or processed
directly (as concrete processors are).

Once a processor is abstract, it cannot be made concrete again.
But, a new concrete processor functioning just like it can be
created by invoking the processor.

###### Returns

`Processor` — The processor on which `abstract` is invoked.

###### Example

The following example, `index.js`, shows how [**rehype**][rehype]
prevents extensions to itself:

```js
var unified = require('unified');
var parse = require('rehype-parse');
var stringify = require('rehype-stringify');

module.exports = unified().use(parse).use(stringify).abstract();
```

The below example, `a.js`, shows how that processor can be used to
create a command line interface which reformats markdown passed on
**stdin**(4) and outputs it on **stdout**(4).

```js
var rehype = require('rehype');

process.stdin.pipe(rehype()).pipe(process.stdout);
```

The below example, `b.js`, shows a similar looking example which
operates on the abstract [**rehype**][rehype] interface.  If this
behaviour was allowed it would result in unexpected behaviour, so
an error is thrown.  **This is invalid**:

```js
var rehype = require('rehype');

process.stdin.pipe(rehype).pipe(process.stdout);
```

Yields:

```txt
~/index.js:118
      throw new Error(
      ^

Error: Cannot pipe into abstract processor.
To make the processor concrete, invoke it: use `processor()` instead of `processor`.
    at assertConcrete (~/index.js:118:13)
    at Function.<anonymous> (~/index.js:135:7)
    ...
    at Object.<anonymous> (~/b.js:76:15)
    ...
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/unified.svg

[travis]: https://travis-ci.org/wooorm/unified

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/unified.svg

[codecov]: https://codecov.io/github/wooorm/unified

[npm-install]: https://docs.npmjs.com/cli/install

[releases]: https://github.com/wooorm/unified/releases

[license]: LICENSE

[author]: http://wooorm.com

[rehype]: https://github.com/wooorm/rehype

[remark]: https://github.com/wooorm/remark

[retext]: https://github.com/wooorm/retext

[hast]: https://github.com/wooorm/hast

[mdast]: https://github.com/wooorm/mdast

[nlcst]: https://github.com/wooorm/nlcst

[unist]: https://github.com/wooorm/unist

[engine]: https://github.com/wooorm/unified-engine

[args]: https://github.com/wooorm/unified-args

[gulp]: https://github.com/wooorm/unified-engine-gulp

[atom]: https://github.com/wooorm/unified-engine-atom

[remark-rehype]: https://github.com/wooorm/remark-rehype

[remark-retext]: https://github.com/wooorm/remark-retext

[unist-utilities]: https://github.com/wooorm/unist#list-of-utilities

[vfile]: https://github.com/wooorm/vfile

[vfile-contents]: https://github.com/wooorm/vfile#vfilecontents

[vfile-utilities]: https://github.com/wooorm/vfile#related-tools

[vfilemessage]: https://github.com/wooorm/vfile#vfilemessage

[writable-stream]: https://nodejs.org/api/stream.html#stream_class_stream_writable_1

[stream-pipe]: https://nodejs.org/api/stream.html#stream_readable_pipe_destination_options

[buffer]: https://nodejs.org/api/buffer.html#buffer_buffer

[file]: #file

[node]: #node

[processor]: #processor

[process]: #processorprocessfilevalue-options-done

[parse]: #processorparsefilevalue-options

[parser]: #processorparser

[stringify]: #processorstringifynode-file-options

[compiler]: #processorcompiler

[use]: #processoruseplugin-options

[attacher]: #function-attacherprocessor-options

[transformer]: #function-transformernode-file-next

[next]: #function-nexterr-tree-file

[abstract]: #processorabstract

[plugin]: #plugin

[run-done]: #function-doneerr-node-file

[process-done]: #function-doneerr-file

[write]: #processorwritechunk-encoding-callback

[pipe]: #processorpipestream-options

[trough]: https://github.com/wooorm/trough#function-fninput-next
