# Class fields support for Acorn

[![NPM version](https://img.shields.io/npm/v/acorn-class-fields.svg)](https://www.npmjs.org/package/acorn-class-fields)

This is a plugin for [Acorn](http://marijnhaverbeke.nl/acorn/) - a tiny, fast JavaScript parser, written completely in JavaScript.

It implements support for class fields as defined in the stage 3 proposal [Class field declarations for JavaScript](https://github.com/tc39/proposal-class-fields). The emitted AST follows [an ESTree proposal](https://github.com/estree/estree/pull/180).

## Usage

This module provides a plugin that can be used to extend the Acorn `Parser` class:

```javascript
const {Parser} = require('acorn');
const classFields = require('acorn-class-fields');
Parser.extend(classFields).parse('class X { x = 0 }');
```

## License

This plugin is released under an [MIT License](./LICENSE).
