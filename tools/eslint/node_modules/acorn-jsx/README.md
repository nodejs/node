# Acorn-JSX

[![Build Status](https://travis-ci.org/RReverser/acorn-jsx.svg?branch=master)](https://travis-ci.org/RReverser/acorn-jsx)
[![NPM version](https://img.shields.io/npm/v/acorn-jsx.svg)](https://www.npmjs.org/package/acorn-jsx)

This is plugin for [Acorn](http://marijnhaverbeke.nl/acorn/) - a tiny, fast JavaScript parser, written completely in JavaScript.

It was created as an experimental alternative, faster [React.js JSX](http://facebook.github.io/react/docs/jsx-in-depth.html) parser.

According to [benchmarks](https://github.com/RReverser/acorn-jsx/blob/master/test/bench.html), Acorn-JSX is 2x faster than official [Esprima-based parser](https://github.com/facebook/esprima) when location tracking is turned on in both (call it "source maps enabled mode"). At the same time, it consumes all the ES6+JSX syntax that can be consumed by Esprima-FB (this is proved by [official tests](https://github.com/RReverser/acorn-jsx/blob/master/test/tests-jsx.js)).

**UPDATE [14-Apr-2015]**: Facebook implementation started [deprecation process](https://github.com/facebook/esprima/issues/111) in favor of Acorn + Acorn-JSX + Babel for parsing and transpiling JSX syntax.

## Transpiler

Please note that this tool only parses source code to JSX AST, which is useful for various language tools and services. If you want to transpile your code to regular ES5-compliant JavaScript with source map, check out the [babel transpiler](https://babeljs.io/) which uses `acorn-jsx` under the hood.

## Usage

You can use module directly in order to get Acorn instance with plugin installed:

```javascript
var acorn = require('acorn-jsx');
```

Or you can use `inject.js` for injecting plugin into your own version of Acorn like following:

```javascript
var acorn = require('acorn-jsx/inject')(require('./custom-acorn'));
```

Then, use `plugins` option whenever you need to support JSX while parsing:

```javascript
var ast = acorn.parse(code, {
  plugins: { jsx: true }
});
```

## License

This plugin is issued under the [MIT license](./LICENSE).
