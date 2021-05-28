# @npmcli/run-script

Run a lifecycle script for a package (descendant of npm-lifecycle)

## USAGE

```js
const runScript = require('@npmcli/run-script')

runScript({
  // required, the script to run
  event: 'install',

  // extra args to pass to the command, defaults to []
  args: [],

  // required, the folder where the package lives
  path: '/path/to/package/folder',

  // optional, defaults to /bin/sh on unix, or cmd.exe on windows
  scriptShell: '/bin/bash',

  // optional, defaults to false
  // return stdout and stderr as strings rather than buffers
  stdioString: true,

  // optional, additional environment variables to add
  // note that process.env IS inherited by default
  // Always set:
  // - npm_package_json The package.json file in the folder
  // - npm_lifecycle_event The event that this is being run for
  // - npm_lifecycle_script The script being run
  // The fields described in https://github.com/npm/rfcs/pull/183
  env: {
    npm_package_from: 'foo@bar',
    npm_package_resolved: 'https://registry.npmjs.org/foo/-/foo-1.2.3.tgz',
    npm_package_integrity: 'sha512-foobarbaz',
  },

  // defaults to 'pipe'.  Can also pass an array like you would to node's
  // exec or spawn functions.  Note that if it's anything other than
  // 'pipe' then the stdout/stderr values on the result will be missing.
  // npm cli sets this to 'inherit' for explicit run-scripts (test, etc.)
  // but leaves it as 'pipe' for install scripts that run in parallel.
  stdio: 'inherit',

  // print the package id and script, and the command to be run, like:
  // > somepackage@1.2.3 postinstall
  // > make all-the-things
  // Defaults true when stdio:'inherit', otherwise suppressed
  banner: true,
})
  .then(({ code, signal, stdout, stderr, pkgid, path, event, script }) => {
    // do something with the results
  })
  .catch(er => {
    // command did not work.
    // er is decorated with:
    // - code
    // - signal
    // - stdout
    // - stderr
    // - path
    // - pkgid (name@version string)
    // - event
    // - script
  })
```

## API

Call the exported `runScript` function with an options object.

Returns a promise that resolves to the result of the execution.  Promise
rejects if the execution fails (exits non-zero) or has any other error.
Rejected errors are decorated with the same values as the result object.

If the stdio options mean that it'll have a piped stdin, then the stdin is
ended immediately on the child process.  If stdin is shared with the parent
terminal, then it is up to the user to end it, of course.

### Results

- `code` Process exit code
- `signal` Process exit signal
- `stdout` stdout data (Buffer, or String when `stdioString` set to true)
- `stderr` stderr data (Buffer, or String when `stdioString` set to true)
- `path` Path to the package executing its script
- `event` Lifecycle event being run
- `script` Command being run

### Options

- `path` Required.  The path to the package having its script run.
- `event` Required.  The event being executed.
- `args` Optional, default `[]`.  Extra arguments to pass to the script.
- `env` Optional, object of fields to add to the environment of the
  subprocess.  Note that process.env IS inherited by default These are
  always set:
  - `npm_package_json` The package.json file in the folder
  - `npm_lifecycle_event` The event that this is being run for
  - `npm_lifecycle_script` The script being run
  - The `package.json` fields described in
    [RFC183](https://github.com/npm/rfcs/pull/183/files).
- `scriptShell` Optional, defaults to `/bin/sh` on Unix, defaults to
  `env.ComSpec` or `cmd` on Windows.  Custom script to use to execute the
  command.
- `stdio` Optional, defaults to `'pipe'`.  The same as the `stdio` argument
  passed to `child_process` functions in Node.js.  Note that if a stdio
  output is set to anything other than `pipe`, it will not be present in
  the result/error object.
- `cmd` Optional.  Override the script from the `package.json` with
  something else, which will be run in an otherwise matching environment.
- `stdioString` Optional, defaults to `false`.  Return string values for
  `stderr` and `stdout` rather than Buffers.
- `banner` Optional, defaults to `true`.  If the `stdio` option is set to
  `'inherit'`, then print a banner with the package name and version, event
  name, and script command to be run.  Set explicitly to `false` to disable
  for inherited stdio.

Note that this does _not_ run pre-event and post-event scripts.  The
caller has to manage that process themselves.

## Differences from [npm-lifecycle](https://github.com/npm/npm-lifecycle)

This is an implementation to satisfy [RFC
90](https://github.com/npm/rfcs/pull/90), [RFC
77](https://github.com/npm/rfcs/pull/77), and [RFC
73](https://github.com/npm/rfcs/pull/73).

Apart from those behavior changes in npm v7, this is also just refresh of
the codebase, with modern coding techniques and better test coverage.

Functionally, this means:

- Output is not dumped to the top level process's stdio by default.
- Less stuff is put into the environment.
- It is not opinionated about logging.  (So, at least with the logging
  framework in npm v7.0 and before, the caller has to call
  `log.disableProgress()` and `log.enableProgress()` at the appropriate
  times, if necessary.)
- The directory containing the `node` executable is _never_ added to the
  `PATH` environment variable.  (Ie, `--scripts-prepend-node-path` is
  effectively always set to `false`.)  Doing so causes more unintended side
  effects than it ever prevented.
- Hook scripts are not run by this module.  If the caller wishes to run
  hook scripts, then they can override the default package script with an
  explicit `cmd` option pointing to the `node_modules/.hook/${event}`
  script.
