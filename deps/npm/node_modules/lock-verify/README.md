# lock-verify

Report if your package.json is out of sync with your package-lock.json.

## USAGE

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
