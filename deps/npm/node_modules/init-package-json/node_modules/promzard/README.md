# promzard

A reimplementation of @SubStack's
[prompter](https://github.com/substack/node-prompter), which does not
use AST traversal.

From another point of view, it's a reimplementation of
[@Marak](https://github.com/marak)'s
[wizard](https://github.com/Marak/wizard) which doesn't use schemas.

The goal is a nice drop-in enhancement for `npm init`.

## Usage

```javascript
var promzard = require('promzard')
promzard(inputFile, optionalContextAdditions, function (er, data) {
  // .. you know what you doing ..
})
```

In the `inputFile` you can have something like this:

```javascript
var fs = require('fs')
module.exports = {
  "greeting": prompt("Who shall you greet?", "world", function (who) {
    return "Hello, " + who
  }),
  "filename": __filename,
  "directory": function (cb) {
    fs.readdir(__dirname, cb)
  }
}
```

When run, promzard will display the prompts and resolve the async
functions in order, and then either give you an error, or the resolved
data, ready to be dropped into a JSON file or some other place.


### promzard(inputFile, ctx, callback)

The inputFile is just a node module.  You can require() things, set
module.exports, etc.  Whatever that module exports is the result, and it
is walked over to call any functions as described below.

The only caveat is that you must give PromZard the full absolute path
to the module (you can get this via Node's `require.resolve`.)  Also,
the `prompt` function is injected into the context object, so watch out.

Whatever you put in that `ctx` will of course also be available in the
module.  You can get quite fancy with this, passing in existing configs
and so on.

### Class: promzard.PromZard(file, ctx)

Just like the `promzard` function, but the EventEmitter that makes it
all happen.  Emits either a `data` event with the data, or a `error`
event if it blows up.

If `error` is emitted, then `data` never will be.

### prompt(...)

In the promzard input module, you can call the `prompt` function.
This prompts the user to input some data.  The arguments are interpreted
based on type:

1. `string`  The first string encountered is the prompt.  The second is
   the default value.
2. `function` A transformer function which receives the data and returns
   something else.  More than meets the eye.
3. `object` The `prompt` member is the prompt, the `default` member is
   the default value, and the `transform` is the transformer.

Whatever the final value is, that's what will be put on the resulting
object.

### Functions

If there are any functions on the promzard input module's exports, then
promzard will call each of them with a callback.  This way, your module
can do asynchronous actions if necessary to validate or ascertain
whatever needs verification.

The functions are called in the context of the ctx object, and are given
a single argument, which is a callback that should be called with either
an error, or the result to assign to that spot.

In the async function, you can also call prompt() and return the result
of the prompt in the callback.

For example, this works fine in a promzard module:

```
exports.asyncPrompt = function (cb) {
  fs.stat(someFile, function (er, st) {
    // if there's an error, no prompt, just error
    // otherwise prompt and use the actual file size as the default
    cb(er, prompt('file size', st.size))
  })
}
```

You can also return other async functions in the async function
callback.  Though that's a bit silly, it could be a handy way to reuse
functionality in some cases.

### Sync vs Async

The `prompt()` function is not synchronous, though it appears that way.
It just returns a token that is swapped out when the data object is
walked over asynchronously later, and returns a token.

For that reason, prompt() calls whose results don't end up on the data
object are never shown to the user.  For example, this will only prompt
once:

```
exports.promptThreeTimes = prompt('prompt me once', 'shame on you')
exports.promptThreeTimes = prompt('prompt me twice', 'um....')
exports.promptThreeTimes = prompt('you cant prompt me again')
```

### Isn't this exactly the sort of 'looks sync' that you said was bad about other libraries?

Yeah, sorta.  I wouldn't use promzard for anything more complicated than
a wizard that spits out prompts to set up a config file or something.
Maybe there are other use cases I haven't considered.
