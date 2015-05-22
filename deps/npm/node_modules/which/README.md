# which

Like the unix `which` utility.

Finds the first instance of a specified executable in the PATH
environment variable.  Does not cache the results, so `hash -r` is not
needed when the PATH changes.

## USAGE

```javascript
var which = require('which')

// async usage
which('node', function (er, resolvedPath) {
  // er is returned if no "node" is found on the PATH
  // if it is found, then the absolute path to the exec is returned
})

// sync usage
// throws if not found
var resolved = which.sync('node')

// Pass options to override the PATH and PATHEXT environment vars.
which('node', { path: someOtherPath }, function (er, resolved) {
  if (er)
    throw er
  console.log('found at %j', resolved)
})
```

## OPTIONS

If you pass in options, then `path` and `pathExt` are relevant.
