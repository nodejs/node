# babel-template

> Generate an AST from a string template.

In computer science, this is known as an implementation of quasiquotes.

## Install

```sh
$ npm install babel-template
```

## Usage

```js
import template from 'babel-template';
import generate from 'babel-generator';
import * as t from 'babel-types';

const buildRequire = template(`
  var IMPORT_NAME = require(SOURCE);
`);

const ast = buildRequire({
  IMPORT_NAME: t.identifier('myModule'),
  SOURCE: t.stringLiteral('my-module')
});

console.log(generate(ast).code);
```

```js
var myModule = require('my-module');
```
