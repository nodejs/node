# unherit [![Build Status](https://img.shields.io/travis/wooorm/unherit.svg)](https://travis-ci.org/wooorm/unherit) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/unherit.svg)](https://codecov.io/github/wooorm/unherit?branch=master)

Create a custom constructor which can be modified without affecting the
original class.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install unherit
```

**unherit** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), and [duo](http://duojs.org/#getting-started),
and as an AMD, CommonJS, and globals module, [uncompressed](unherit.js) and [compressed](unherit.min.js).

## Usage

```js
var EventEmitter = require('events').EventEmitter;

/* Create a private class which acts just like
 * `EventEmitter`. */
var Emitter = unherit(EventEmitter);

Emitter.prototype.defaultMaxListeners = 0;
/* Now, all instances of `Emitter` have no maximum
 * listeners, without affecting other `EventEmitter`s. */

assert(new Emitter().defaultMaxListeners === 0); // true
assert(new EventEmitter().defaultMaxListeners === undefined); // true
assert(new Emitter() instanceof EventEmitter); // true
```

## API

### unherit(Super)

Create a custom constructor which can be modified without affecting the
original class.

**Parameters**

*   `Super` (`Function`) — Super-class.

**Returns**

(`Function`) — Constructor acting like `Super`, which can be modified
without affecting the original class.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
