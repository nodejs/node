# signal-exit

[![Build Status](https://travis-ci.org/bcoe/signal-exit.png)](https://travis-ci.org/bcoe/signal-exit)
[![Coverage Status](https://coveralls.io/repos/bcoe/signal-exit/badge.svg?branch=)](https://coveralls.io/r/bcoe/signal-exit?branch=)
[![NPM version](https://img.shields.io/npm/v/signal-exit.svg)](https://www.npmjs.com/package/signal-exit)

When you want to fire an event no matter how a process exits:

* reaching the end of execution.
* explicitly having `process.exit(code)` called.
* having `process.kill(pid, sig)` called.
* receiving a fatal signal from outside the process

Use `signal-exit`.

```js
var onExit = require('signal-exit')

onExit(function (code, signal) {
  console.log('process exited!')
})
```

## API

`var remove = onExit(function (code, signal) {}, options)`

The return value of the function is a function that will remove the
handler.

Note that the function *only* fires for signals if the signal would
cause the proces to exit.  That is, there are no other listeners, and
it is a fatal signal.

## Options

* `alwaysLast`: Run this handler after any other signal or exit
  handlers.  This causes `process.emit` to be monkeypatched.
