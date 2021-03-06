# Static class features support for Acorn

[![NPM version](https://img.shields.io/npm/v/acorn-class-fields.svg)](https://www.npmjs.org/package/acorn-static-class-features)

This is a plugin for [Acorn](http://marijnhaverbeke.nl/acorn/) - a tiny, fast JavaScript parser, written completely in JavaScript.

It implements support for static class features as defined in the stage 3 proposal [Static class features](https://github.com/tc39/proposal-static-class-features). The emitted AST follows [an ESTree proposal](https://github.com/estree/estree/pull/180).

## Usage

This module provides a plugin that can be used to extend the Acorn `Parser` class:

```javascript
const {Parser} = require('acorn');
const staticClassFeatures = require('acorn-static-class-features');
Parser.extend(staticClassFeatures).parse('class X { static x = 0 }');
```

## License

This plugin is released under an [MIT License](./LICENSE).
