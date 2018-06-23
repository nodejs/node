# state-toggle [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Enter/exit a state.

## Installation

[npm][]:

```bash
npm install state-toggle
```

## Usage

```javascript
var toggle = require('state-toggle')

var ctx = {on: false}
var enter = toggle('on', ctx.on, ctx)
var exit

// Entering:
exit = enter()
console.log(ctx.on) // => true

// Exiting:
exit()
console.log(ctx.on) // => false
```

## API

### `toggle(key, initial[, ctx])`

Create a toggle, which when entering toggles `key` on `ctx` (or `this`,
if `ctx` is not given) to `!initial`, and when exiting, sets `key` on
the context back to the value it had before entering.

###### Returns

`Function` — [`enter`][enter].

### `enter()`

Enter the state.

###### Context

If no `ctx` was given to `toggle`, the context object (`this`) of `enter()`
is used to toggle.

###### Returns

`Function` — [`exit`][exit].

### `exit()`

Exit the state, reverting `key` to the value it had before entering.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/state-toggle.svg

[travis]: https://travis-ci.org/wooorm/state-toggle

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/state-toggle.svg

[codecov]: https://codecov.io/github/wooorm/state-toggle

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[enter]: #enter

[exit]: #exit
