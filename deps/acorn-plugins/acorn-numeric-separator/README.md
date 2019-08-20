# Numeric separator support for Acorn

[![NPM version](https://img.shields.io/npm/v/acorn-numeric-separator.svg)](https://www.npmjs.org/package/acorn-numeric-separator)

This is a plugin for [Acorn](http://marijnhaverbeke.nl/acorn/) - a tiny, fast JavaScript parser, written completely in JavaScript.

It implements support for numeric separators as defined in the stage 3 proposal [Numeric Separators](https://github.com/tc39/proposal-numeric-separator).

## Usage

This module provides a plugin that can be used to extend the Acorn `Parser` class to parse numeric separators:

```javascript
var acorn = require('acorn');
var numericSeparator = require('acorn-numeric-separator');
acorn.Parser.extend(numericSeparator).parse('100_000');
```

## License

This plugin is released under an [MIT License](./LICENSE).
