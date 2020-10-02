# promise-call-limit

Call an array of promise-returning functions, restricting concurrency to a
specified limit.

## USAGE

```js
const promiseCallLimit = require('promise-call-limit')
const things = getLongListOfThingsToFrobulate()

// frobulate no more than 4 things in parallel
promiseCallLimit(things.map(thing => () => frobulateThing(thing)), 4)
  .then(results => console.log('frobulated 4 at a time', results))
```

## API

### promiseCallLimit(queue Array<() => Promise>, limit = defaultLimit)

The default limit is the number of CPUs on the system - 1, or 1.

The reason for subtracting one is that presumably the main thread is taking
up a CPU as well, so let's not be greedy.

Note that the array should be a list of Promise-_returning_ functions, not
Promises themselves.  If you have a bunch of Promises already, you're best
off just calling `Promise.all()`.

The functions in the queue are called without any arguments.
