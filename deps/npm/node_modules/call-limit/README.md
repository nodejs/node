call-limit
----------

Limit the number of simultaneous executions of a async function.

```javascript
const fs = require('fs')
const limit = require('call-limit')
const limitedStat = limit(fs.stat, 5)
```

Or with promise returning functions:

```javascript
const fs = Bluebird.promisifyAll(require('fs'))
const limit = require('call-limit')
const limitedStat = limit.promise(fs.statAsync, 5)
```

### USAGE:

Given that:

```javascript
const limit = require('call-limit')
```

### limit(func, maxRunning) → limitedFunc

The returned function will execute up to maxRunning calls of `func` at once. 
Beyond that they get queued and called when the previous call completes.

`func` must accept a callback as the final argument and must call it when
it completes, or `call-limit` won't know to dequeue the next thing to run.

By contrast, callers to `limitedFunc` do NOT have to pass in a callback, but
if they do they'll be called when `func` calls its callback.

### limit.promise(func, maxRunning) → limitedFunc

The returned function will execute up to maxRunning calls of `func` at once.
Beyond that they get queued and called when the previous call completes.

`func` must return a promise.

`limitedFunc` will return a promise that resolves with the promise returned
from the call to `func`.

### limit.method(class, methodName, maxRunning)

This is sugar for:

```javascript
class.prototype.methodName = limit(class.prototype.methodName, maxRunning)
```

### limit.method(object, methodName, maxRunning)

This is sugar for:

```javascript
object.methodName = limit(object.methodName, maxRunning)
```

For example `limit.promise.method(fs, 'stat', 5)` is the same as
`fs.stat = limit.promise(fs.stat, 5)`.

### limit.promise.method(class, methodName, maxRunning)

This is sugar for:

```javascript
class.prototype.methodName = limit.promise(class.prototype.methodName, maxRunning)
```

### limit.promise.method(object, methodName, maxRunning)

This is sugar for:

```javascript
object.methodName = limit.promise(object.methodName, maxRunning)
```

For example `limit.promise.method(fs, 'statAsync', 5)` is the same as
`fs.statAsync = limit.promise(fs.statAsync, 5)`.
