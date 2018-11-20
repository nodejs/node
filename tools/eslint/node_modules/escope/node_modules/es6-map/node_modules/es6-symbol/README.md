# es6-symbol
## ECMAScript6 Symbol polyfill

### Limitations

- Underneath it uses real string property names which can easily be retrieved (however accidental collision with other property names is unlikely)
- As it needs custom `toString` behavior to work properly. Original `Symbol.prototype.toString` couldn't be implemented as specified, still it's accessible as `Symbol.prototoype.properToString`

### Usage

If you want to make sure your environment implements `Symbol`, do:

```javascript
require('es6-symbol/implement');
```

If you'd like to use native version when it exists and fallback to polyfill if it doesn't, but without implementing `Symbol` on global scope, do:

```javascript
var Symbol = require('es6-symbol');
```

If you strictly want to use polyfill even if native `Symbol` exists (hard to find a good reason for that), do:

```javascript
var Symbol = require('es6-symbol/polyfill');
```

#### API

Best is to refer to [specification](http://people.mozilla.org/~jorendorff/es6-draft.html#sec-symbol-objects). Still if you want quick look, follow examples:

```javascript
var Symbol = require('es6-symbol');

var symbol = Symbol('My custom symbol');
var x = {};

x[symbol] = 'foo';
console.log(x[symbol]); 'foo'

// Detect iterable:
var iterator, result;
if (possiblyIterable[Symbol.iterator]) {
  iterator = possiblyIterable[Symbol.iterator]();
  result = iterator.next();
  while(!result.done) {
    console.log(result.value);
    result = iterator.next();
  }
}
```

### Installation
#### NPM

In your project path:

	$ npm install es6-symbol

##### Browser

You can easily bundle _es6-symbol_ for browser with [modules-webmake](https://github.com/medikoo/modules-webmake)

## Tests [![Build Status](https://travis-ci.org/medikoo/es6-symbol.png)](https://travis-ci.org/medikoo/es6-symbol)

	$ npm test
