# jsdoctypeparser

[![Build Status](https://travis-ci.org/jsdoctypeparser/jsdoctypeparser.svg?branch=master)](https://travis-ci.org/jsdoctypeparser/jsdoctypeparser)
[![NPM version](https://badge.fury.io/js/jsdoctypeparser.svg)](http://badge.fury.io/js/jsdoctypeparser)

The parser can parse:

* [JSDoc type expressions](https://jsdoc.app/tags-type.html)
  * `foo.bar`, `String[]`
* [Closure Compiler type expressions](https://developers.google.com/closure/compiler/docs/js-for-compiler)
  * `Array<string>`, `function(arg1, arg2): ret`
* [some Typescript types](https://github.com/Microsoft/TypeScript)
  * `(x: number) => string`, `typeof x`, `import("./some-module")`
* Complex type expressions
  * `Array<Array<string>>`, `function(function(Function))`

## Live demo

The [live demo](https://jsdoctypeparser.github.io) is available.

## Usage (Programmatic)

### Parsing

```javascript
const {parse} = require('jsdoctypeparser');

const ast = parse('Array<MyClass>');
```

The `ast` becomes:

```json
{
  "type": "GENERIC",
  "subject": {
    "type": "NAME",
    "name": "Array"
  },
  "objects": [
    {
      "type": "NAME",
      "name": "MyClass"
    }
  ],
  "meta": {
    "syntax": "ANGLE_BRACKET"
  }
}
```

See the [AST specifications](https://github.com/Kuniwak/jsdoctypeparser/blob/update-readme/README.md#ast-specifications).

### Publishing

We can stringify the AST nodes by using `publish`.

```javascript
const {publish} = require('jsdoctypeparser');

const ast = {
  type: 'GENERIC',
  subject: {
    type: 'NAME',
    name: 'Array'
  },
  objects: [
    {
      type: 'NAME',
      name: 'MyClass'
    }
  ]
};

const string = publish(ast);
```

The `string` becomes:

```json
"Array<MyClass>"
```

#### Custom publishing

We can change the stringification strategy by using the 2nd parameter of `publish(node, publisher)`.
The `publisher` MUST have handlers for all node types (see `lib/NodeType.js`).

And we can override default behavior by using `createDefaultPublisher`.

```javascript
const {publish, createDefaultPublisher} = require('jsdoctypeparser');

const ast = {
  type: 'NAME',
  name: 'MyClass',
};

const customPublisher = createDefaultPublisher();
customPublisher.NAME = (node, pub) =>
  `<a href="./types/${node.name}.html">${node.name}</a>`;

const string = publish(ast, customPublisher);
```

The `string` becomes:

```html
<a href="./types/MyClass.html">MyClass</a>
```

### Traversing

We can traverse the AST by using `traverse`.
This function takes 3 parameters (a node and an onEnter handler, an onLeave handler).
The handlers take a visiting node.

```javascript
const {parse, traverse} = require('jsdoctypeparser');
const ast = parse('Array<{ key1: function(), key2: A.B.C }>');

function onEnter(node, parentName, parentNode) {
  console.log('enter', node.type, parentName, parentNode.type);
}

function onLeave(node, parentName, parentNode) {
  console.log('leave', node.type, parentName, parentNode.type);
}

traverse(ast, onEnter, onLeave);
```

The output will be:

```
enter GENERIC null null
enter NAME subject GENERIC
leave NAME subject GENERIC
enter RECORD objects GENERIC
enter RECORD_ENTRY entries RECORD
enter FUNCTION value RECORD_ENTRY
leave FUNCTION value RECORD_ENTRY
leave RECORD_ENTRY entries RECORD
enter RECORD_ENTRY entries RECORD
enter MEMBER value RECORD_ENTRY
enter MEMBER owner MEMBER
enter NAME owner MEMBER
leave NAME owner MEMBER
leave MEMBER owner MEMBER
leave MEMBER value RECORD_ENTRY
leave RECORD_ENTRY entries RECORD
leave RECORD objects GENERIC
leave GENERIC null null
```

## AST Specifications

### `NAME`

Example:

```javascript
/**
 * @type {name}
 */
```

Structure:

```javascript
{
  "type": "NAME",
  "name": string
}
```

### `MEMBER`

Example:

```javascript
/**
 * @type {owner.name}
 * @type {superOwner.owner.name}
 */
```

Structure:

```javascript
{
  "type": "MEMBER",
  "name": string,
  "quoteStyle": "none",
  "owner": node,
  "hasEventPrefix": boolean
}
```

### `INNER_MEMBER`

Example:

```javascript
/**
 * @type {owner~name}
 */
```

Structure:

```javascript
{
  "type": "INNER_MEMBER",
  "name": string,
  "quoteStyle": "none",
  "owner": node,
  "hasEventPrefix": boolean
}
```

### `INSTANCE_MEMBER`

Example:

```javascript
/**
 * @type {owner#name}
 */
```

Structure:

```javascript
{
  "type": "INSTANCE_MEMBER",
  "name": string,
  "quoteStyle": "none",
  "owner": node,
  "hasEventPrefix": boolean
}
```

### `UNION`

Example:

```javascript
/**
 * @type {left|right}
 * @type {(left|right)}
 */
```

Structure:

```javascript
{
  "type": "UNION",
  "left": node,
  "right": node
}
```

### `INTERSECTION`

Example:

```javascript
/**
 * @type {left&right}
 * @type {(left&right)}
 */
```

Structure:

```javascript
{
  "type": "INTERSECTION",
  "left": node,
  "right": node
}
```

### `RECORD`

Example:

```javascript
/**
 * @type {{}}
 * @type {{ key: value }}
 * @type {{ key: value, anyKey }}
 */
```

Structure:

```javascript
{
  "type": "RECORD",
  "entries": [
    recordEntryNode,
    recordEntryNode,
    ...
  ]
}
```

### `RECORD_ENTRY`

Structure:

```javascript
{
  "type": "RECORD_ENTRY",
  "key": string,
  "value": node (or null)
}
```

### `GENERIC`

Example:

```javascript
/**
 * @type {Subject<Object, Object>}
 * @type {Object[]}
 */
```

Structure:

```javascript
{
  "type": "GENERIC",
  "subject": node,
  "objects": [
    node,
    node,
    ...
  ],
  "meta": {
    "syntax": ("ANGLE_BRACKET" or "ANGLE_BRACKET_WITH_DOT" or "SQUARE_BRACKET")
  }
}
```

### `FUNCTION`

Example:

```javascript
/**
 * @type {function()}
 * @type {function(param, param): return}
 * @type {function(this: Context)}
 * @type {function(new: Class)}
 */
```

Structure:

```javascript
{
  "type": "FUNCTION",
  "params": [
    node,
    node,
    ...
  ],
  "returns": node (or null),
  "new": node (or null),
  "this": node (or null)
}
```

### `OPTIONAL`

Example:

```javascript
/**
 * @type {Optional=}
 */
```

Structure:

```javascript
{
  "type": "OPTIONAL",
  "value": node,
  "meta": {
    "syntax": ("PREFIX_EQUALS_SIGN" or "SUFFIX_EQUALS_SIGN")
  }
}
```

### `NULLABLE`

Example:

```javascript
/**
 * @type {?Nullable}
 */
```

Structure:

```javascript
{
  "type": "NULLABLE",
  "value": node,
  "meta": {
    "syntax": ("PREFIX_QUESTION_MARK" or "SUFFIX_QUESTION_MARK")
  }
}
```

### `NOT_NULLABLE`

Example:

```javascript
/**
 * @type {!NotNullable}
 */
```

Structure:

```javascript
{
  "type": "NOT_NULLABLE",
  "value": node,
  "meta": {
    "syntax": ("PREFIX_BANG" or "SUFFIX_BANG")
  }
}
```

### `VARIADIC`

Example:

```javascript
/**
 * @type {...Variadic}
 * @type {Variadic...}
 * @type {...}
 */
```

Structure:

```javascript
{
  "type": "VARIADIC",
  "value": node (or null),
  "meta": {
    "syntax": ("PREFIX_DOTS" or "SUFFIX_DOTS" or "ONLY_DOTS")
  }
}
```

### `MODULE`

Example:

```javascript
/**
 * @type {module:path/to/file.Module}
 */
```

Structure:

```javascript
{
  "type": "MODULE",
  "value": node
}
```

### `FILE_PATH`

Example:

```javascript
/**
 * @type {module:path/to/file.Module}
 *               ^^^^^^^^^^^^
 */
```

Structure:

```javascript
{
  "type": "FILE_PATH",
  "path": string
}
```

### `EXTERNAL`

Example:

```javascript
/**
 * @type {external:External}
 */
```

Structure:

```javascript
{
  "type": "EXTERNAL",
  "value": node
}
```

### `STRING_VALUE`

Example:

```javascript
/**
 * @type {"abc"}
 * @type {"can\"escape"}
 */
```

Structure:

```javascript
{
  "type": "STRING_VALUE",
  "quoteStyle": "double",
  "string": string
}
```

### `NUMBER_VALUE`

Example:

```javascript
/**
 * @type {123}
 * @type {0b11}
 * @type {0o77}
 * @type {0xff}
 */
```

Structure:

```javascript
{
  "type": "NUMBER_VALUE",
  "number": string
}
```

### `ANY`

Example:

```javascript
/**
 * @type {*}
 */
```

Structure:

```javascript
{
  "type": "ANY"
}
```

### `UNKNOWN`

Example:

```javascript
/**
 * @type {?}
 */
```

Structure:

```javascript
{
  "type": "UNKNOWN"
}
```

### `PARENTHESIS`

Example:

```javascript
/**
 * @type {(Foo)}
 */
```

Structure:

```javascript
{
  "type": "PARENTHESIS",
  "value": node
}
```

### Others

We can use a parenthesis to change operator orders.

```javascript
/**
 * @type {(module:path/to/file.js).foo}
 */
```

## Usage (CLI)

To parse a type into a JSON structure, you may pass a string argument
containing the structure to parse (with the JSON results equivalent to the
parsing example above):

```
jsdoctypeparser 'Array<MyClass>'
```

Note: There is no need to prefix the path to the `jsdoctypeparser` binary,
e.g., with `./node_modules/.bin/` when you are running within one of the
`package.json` `scripts` or if you have installed the package globally.

## License

[This script is licensed under the MIT](./LICENSE-MIT.txt).
