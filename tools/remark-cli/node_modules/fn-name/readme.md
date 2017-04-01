# fn-name [![Build Status](https://travis-ci.org/sindresorhus/fn-name.svg?branch=master)](https://travis-ci.org/sindresorhus/fn-name)

> Get the name of a named function

There is a non-standard [name](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/name) property on functions, but it's not supported in all browsers.  
This module tries that property then falls back to extracting the name from the function source.


## Install

```sh
$ npm install --save fn-name
```


## Usage

```js
fnName(function foo() {});
//=> foo
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
