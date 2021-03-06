# walk-up-path

Given a path string, return a generator that walks up the path, emitting
each dirname.

So, to get a platform-portable walk up, instead of doing something like
this:

```js
for (let p = dirname(path); p;) {

  // ... do stuff ...

  const pp = dirname(p)
  if (p === pp)
    p = null
  else
    p = pp
}
```

Or this:

```js
for (let p = dirname(path); !isRoot(p); p = dirname(p)) {
  // ... do stuff ...
}
```

You can do this:

```js
const walkUpPath = require('walk-up-path')
for (const p of walkUpPath(path)) {
  // ... do stuff ..
}
```

## API

```js
const walkUpPath = require('walk-up-path')
```

Give the fn a string, it'll yield all the directories walking up to the
root.
