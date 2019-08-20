# Private methods and getter/setters support for Acorn

[![NPM version](https://img.shields.io/npm/v/acorn-private-methods.svg)](https://www.npmjs.org/package/acorn-private-methods)

This is a plugin for [Acorn](http://marijnhaverbeke.nl/acorn/) - a tiny, fast JavaScript parser, written completely in JavaScript.

It implements support for private methods, getters and setters as defined in the stage 3 proposal [Private methods and getter/setters for JavaScript classes](https://github.com/tc39/proposal-private-methods). The emitted AST follows [an ESTree proposal](https://github.com/estree/estree/pull/180).

## Usage

This module provides a plugin that can be used to extend the Acorn `Parser` class:

```javascript
const {Parser} = require('acorn');
const privateMethods = require('acorn-private-methods');
Parser.extend(privateMethods).parse('class X { #a() {} }');
```

## License

This plugin is released under an [MIT License](./LICENSE).
