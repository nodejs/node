# @babel/template

> Generate an AST from a string template or template literal.

In computer science, this is known as an implementation of quasiquotes.

## Install

```sh
npm install --save-dev @babel/template
```

## String Usage

```js
import template from "@babel/template";
import generate from "@babel/generator";
import * as t from "@babel/types";

const buildRequire = template(`
  var IMPORT_NAME = require(SOURCE);
`);

const ast = buildRequire({
  IMPORT_NAME: t.identifier("myModule"),
  SOURCE: t.stringLiteral("my-module")
});

console.log(generate(ast).code);
```

```js
const myModule = require("my-module");
```

### `.ast`

If no placeholders are in use and you just want a simple way to parse a
string into an AST, you can use the `.ast` version of the template.

```js
const ast = template.ast(`
  var myModule = require("my-module");
`);
```
which will parse and return the AST directly.


## Template Literal Usage

```js
import template from "@babel/template";
import generate from "@babel/generator";
import * as t from "@babel/types";

const fn = template`
  var IMPORT_NAME = require('${"my-module"}');
`);

const ast = fn({
  IMPORT_NAME: t.identifier("myModule");
});

console.log(generate(ast).code);
```

Note that placeholders can be passed directly as part of the template literal
in order to make things as readable as possible, or they can be passed into
the template function.

### `.ast`

If no placeholders are in use and you just want a simple way to parse a
string into an AST, you can use the `.ast` version of the template.

```js
const name = "my-module";
const mod = "myModule";

const ast = template.ast`
  var ${mod} = require("${name}");
`;
```
which will parse and return the AST directly. Note that unlike the string-based
version mentioned earlier, since this is a template literal, it is still
valid to perform replacements using template literal replacements.


## AST results

The `@babel/template` API exposes a few flexible APIs to make it as easy as
possible to create ASTs with an expected structure. Each of these also has
the `.ast` property mentioned above.

### `template`

`template` returns either a single statement, or an array of
statements, depending on the parsed result.

### `template.smart`

This is the same as the default `template` API, returning either a single
node, or an array of nodes, depending on the parsed result.

### `template.statement`

`template.statement("foo;")()` returns a single statement node, and throw
an exception if the result is anything but a single statement.

### `template.statements`

`template.statements("foo;foo;")()` returns an array of statement nodes.

### `template.expression`

`template.expression("foo")()` returns the expression node.

### `template.program`

`template.program("foo;")()` returns the `Program` node for the template.


## API

### `template(code, [opts])`

#### code

Type: `string`

#### options

`@babel/template` accepts all of the options from [babylon](https://github.com/babel/babel/tree/master/packages/babylon), and specifies
some defaults of its own:

* `allowReturnOutsideFunction` is set to `true` by default.
* `allowSuperOutsideMethod` is set to `true` by default.
* `sourceType` is set to `module` by default.

##### placeholderWhitelist

Type: `Set<string>`
Default: `undefined`

A set of placeholder names to automatically accept. Items in this list do
not need to match the given placeholder pattern.

##### placeholderPattern

Type: `RegExp | false`
Default: `/^[_$A-Z0-9]+$/`

A pattern to search for when looking for Identifier and StringLiteral
nodes that should be considered placeholders.
'false' will disable placeholder searching entirely, leaving only the
'placeholderWhitelist' value to find placeholders.

##### preserveComments

Type: `boolean`
Default: `false`

Set this to `true` to preserve any comments from the `code` parameter.

#### Return value

By default `@babel/template` returns a `function` which is invoked with an
optional object of replacements. See the usage section for an example.

When using `.ast`, the AST will be returned directly.

[babylon]: https://github.com/babel/babylon#options
