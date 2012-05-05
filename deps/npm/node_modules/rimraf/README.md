A `rm -rf` for node.

Install with `npm install rimraf`, or just drop rimraf.js somewhere.

## API

`rimraf(f, [options,] callback)`

The callback will be called with an error if there is one.  Certain
errors are handled for you:

* `EBUSY` -  rimraf will back off a maximum of opts.maxBusyTries times
  before giving up.
* `EMFILE` - If too many file descriptors get opened, rimraf will
  patiently wait until more become available.

## Options

The options object is optional.  These fields are respected:

* `maxBusyTries` -  The number of times to retry a file or folder in the
  event of an `EBUSY` error.  The default is 3.
* `gently` - If provided a `gently` path, then rimraf will only delete
  files and folders that are beneath this path, and only delete symbolic
  links that point to a place within this path.  (This is very important
  to npm's use-case, and shows rimraf's pedigree.)


## rimraf.sync

It can remove stuff synchronously, too.  But that's not so good.  Use
the async API.  It's better.
