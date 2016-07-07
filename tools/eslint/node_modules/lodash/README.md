# lodash v4.13.1

The [Lodash](https://lodash.com/) library exported as [Node.js](https://nodejs.org/) modules.

## Installation

Using npm:
```bash
$ {sudo -H} npm i -g npm
$ npm i --save lodash
```

In Node.js:
```js
// Load the full build.
var _ = require('lodash');
// Load the core build.
var _ = require('lodash/core');
// Load the fp build for immutable auto-curried iteratee-first data-last methods.
var fp = require('lodash/fp');

// Load a method category.
var array = require('lodash/array');
var object = require('lodash/fp/object');

// Load a single method for smaller builds with browserify/rollup/webpack.
var chunk = require('lodash/chunk');
var extend = require('lodash/fp/extend');
```

See the [package source](https://github.com/lodash/lodash/tree/4.13.1-npm) for more details.

**Note:**<br>
Don’t assign values to the [special variable](http://nodejs.org/api/repl.html#repl_repl_features) `_` in the Node.js < 6 REPL.<br>
Install [n_](https://www.npmjs.com/package/n_) for a REPL that includes `lodash` by default.

## Support

Tested in Chrome 49-50, Firefox 45-46, IE 9-11, Edge 13, Safari 8-9, Node.js 0.10-6, & PhantomJS 1.9.8.<br>
Automated [browser](https://saucelabs.com/u/lodash) & [CI](https://travis-ci.org/lodash/lodash/) test runs are available.
