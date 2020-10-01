# promise-all-reject-late

Like Promise.all, but save rejections until all promises are resolved.

This is handy when you want to do a bunch of things in parallel, and
rollback on failure, without clobbering or conflicting with those parallel
actions that may be in flight.  For example, creating a bunch of files,
and deleting any if they don't all succeed.

Example:

```js
const lateReject = require('promise-all-reject-late')

const { promisify } = require('util')
const fs = require('fs')
const writeFile = promisify(fs.writeFile)

const createFilesOrRollback = (files) => {
  return lateReject(files.map(file => writeFile(file, 'some data')))
    .catch(er => {
      // try to clean up, then fail with the initial error
      // we know that all write attempts are finished at this point
      return lateReject(files.map(file => rimraf(file)))
        .catch(er => {
          console.error('failed to clean up, youre on your own i guess', er)
        })
        .then(() => {
          // fail with the original error
          throw er
        })
    })
}
```

## API

* `lateReject([array, of, promises])` - Resolve all the promises,
  returning a promise that rejects with the first error, or resolves with
  the array of results, but only after all promises are settled.
