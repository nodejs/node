# tap-mocha-reporter

Format a TAP stream using Mocha's set of reporters

## USAGE

On the command line, pipe TAP in, and it'll do its thing.

```bash
tap test/*.js | tap-mocha-reporter
```

You can also specify a reporter with the first argument.  The default
is `spec`.

```bash
tap test/*.js | tap-mocha-reporter nyan
```

Programmatically, you can use this as a transform stream.

```javascript
var TSR = require('tap-mocha-reporter')

fs.createReadStream('saved-test-output.tap')
  .pipe(TSR('dot'))
```
