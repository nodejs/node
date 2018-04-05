# lock-verify

Report if your package.json is out of sync with your package-lock.json.

## USAGE

Call it with no arguments to verify the current project's lock file.  Errors
are printed out to stdout and the status code set to 1.

```
$ npx lock-verify
Invalid: lock file's example@2.1.0 does not satisfy example@^1.1.0
Errors found
$
```


Call it with a path to a project to verify that project's lock file. If there
are no errors, it prints nothing and the status code is 0.


```
$ npx lock-verify /path/to/my/project
$
```

## OR AS A LIBRARY

```
const lockVerify = require('lock-verify')
lockVerify(moduleDir).then(result => {
  result.warnings.forEach(w => console.error('Warning:', w))
  if (!result.status) {
    result.errors.forEach(e => console.error(e))
    process.exit(1)
  }
})
```

As a library it's a function that takes the path to a module and returns a
promise that resolves to an object with `.status`, `.warnings` and `.errors`
properties.  The first will be true if everything was ok (though warnings
may exist). If there's no `package.json` or no lockfile in `moduleDir` or they're
unreadable then the promise will be rejected.
