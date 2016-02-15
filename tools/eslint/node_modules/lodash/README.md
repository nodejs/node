# lodash v4.3.0

The [lodash](https://lodash.com/) library exported as [Node.js](https://nodejs.org/) modules.

## Installation

Using npm:
```bash
$ {sudo -H} npm i -g npm
$ npm i --save lodash
```

In Node.js:
```js
// load the full build
var _ = require('lodash');
// load the core build
var _ = require('lodash/core');
// load the fp build for immutable auto-curried iteratee-first data-last methods
var _ = require('lodash/fp');

// or a method category
var array = require('lodash/array');
var object = require('lodash/fp/object');

// or method for smaller builds with browserify/rollup/webpack
var chunk = require('lodash/chunk');
var extend = require('lodash/fp/extend');
```

See the [package source](https://github.com/lodash/lodash/tree/4.3.0-npm) for more details.

**Note:**<br>
Don’t assign values to the [special variable](http://nodejs.org/api/repl.html#repl_repl_features) `_` when in the REPL.<br>
Install [n_](https://www.npmjs.com/package/n_) for a REPL that includes lodash by default.

## Support

Tested in Chrome 47-48, Firefox 43-44, IE 9-11, Edge 13, Safari 8-9, Node.js 0.10, 0.12, 4, & 5, & PhantomJS 1.9.8.<br>
Automated [browser](https://saucelabs.com/u/lodash) & [CI](https://travis-ci.org/lodash/lodash/) test runs are available.
