# unherit [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Create a custom constructor which can be modified without affecting the
original class.

## Installation

[npm][npm-install]:

```bash
npm install unherit
```

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

### `unherit(Super)`

Create a custom constructor which can be modified without affecting the
original class.

###### Parameters

*   `Super` (`Function`) — Super-class.

###### Returns

`Function` — Constructor acting like `Super`, which can be modified
without affecting the original class.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/unherit.svg

[travis]: https://travis-ci.org/wooorm/unherit

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/unherit.svg

[codecov]: https://codecov.io/github/wooorm/unherit

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
