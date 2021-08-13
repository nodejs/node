
# builtins

  List of node.js [builtin modules](http://nodejs.org/api/).

  [![build status](https://secure.travis-ci.org/juliangruber/builtins.svg)](http://travis-ci.org/juliangruber/builtins) [![Greenkeeper badge](https://badges.greenkeeper.io/juliangruber/builtins.svg)](https://greenkeeper.io/)

## Example

Get list of core modules for current Node.js version:

```js
var builtins = require('builtins')()

assert(builtins.indexOf('http') > -1)
```

Get list of core modules for specific Node.js version:

```js
var builtins = require('builtins')({ version: '6.0.0' })

assert(builtins.indexOf('http') > -1)
```

Add experimental modules to the list:

```js
var builtins = require('builtins')({ experimental: true })

assert(builtins.indexOf('wasi') > -1)
```

## License

  MIT
