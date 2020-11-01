# @npmcli/promise-spawn

Spawn processes the way the npm cli likes to do.  Give it some options,
it'll give you a Promise that resolves or rejects based on the results of
the execution.

Note: When the current user is root, this will use
[`infer-owner`](http://npm.im/infer-owner) to find the owner of the current
working directory, and run with that effective uid/gid.  Otherwise, it runs
as the current user always.  (This helps prevent doing git checkouts and
such, and leaving root-owned files lying around in user-owned locations.)

## USAGE

```js
const promiseSpawn = require('@npmcli/promise-spawn')

promiseSpawn('ls', [ '-laF', 'some/dir/*.js' ], {
  cwd: '/tmp/some/path', // defaults to process.cwd()
  stdioString: false, // stdout/stderr as strings rather than buffers
  stdio: 'pipe', // any node spawn stdio arg is valid here
  // any other arguments to node child_process.spawn can go here as well,
  // but uid/gid will be ignored and set by infer-owner if relevant.
}, {
  extra: 'things',
  to: 'decorate',
  the: 'result',
}).then(result => {
  // {code === 0, signal === null, stdout, stderr, and all the extras}
  console.log('ok!', result)
}).catch(er => {
  // er has all the same properties as the result, set appropriately
  console.error('failed!', er)
})
```

## API

### `promiseSpawn(cmd, args, opts, extra)` -> `Promise`

Run the command, return a Promise that resolves/rejects based on the
process result.

Result or error will be decorated with the properties in the `extra`
object.  You can use this to attach some helpful info about _why_ the
command is being run, if it makes sense for your use case.

If `stdio` is set to anything other than `'inherit'`, then the result/error
will be decorated with `stdout` and `stderr` values.  If `stdioString` is
set to `true`, these will be strings.  Otherwise they will be Buffer
objects.

Returned promise is decorated with the `stdin` stream if the process is set
to pipe from `stdin`.  Writing to this stream writes to the `stdin` of the
spawned process.

#### Options

- `stdioString` Boolean, default `false`.  Return stdout/stderr output as
  strings rather than buffers.
- `cwd` String, default `process.cwd()`.  Current working directory for
  running the script.  Also the argument to `infer-owner` to determine
  effective uid/gid when run as root on Unix systems.
- Any other options for `child_process.spawn` can be passed as well, but
  note that `uid` and `gid` will be overridden by the owner of the cwd when
  run as root on Unix systems, or `null` otherwise.
