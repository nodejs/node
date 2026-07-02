# @npmcli/redact

Redact sensitive npm information from output.

## API

This will redact `npm_` prefixed tokens and UUIDs from values.

It will also replace passwords in stringified URLs.

### `redact(string)`

Redact values from a single value

```js
const { redact } = require('@npmcli/redact')

redact('https://user:pass@registry.npmjs.org/')
// https://user:***@registry.npmjs.org/

redact(`https://registry.npmjs.org/path/npm_${'a'.repeat('36')}`)
// https://registry.npmjs.org/path/npm_***
```

### `redactLog(string | string[])`

Redact values from a string or array of strings.

This method will also split all strings on `\s` and `=` and iterate over them.

```js
const { redactLog } = require('@npmcli/redact')

redactLog([
  'Something --x=https://user:pass@registry.npmjs.org/ foo bar',
  '--url=http://foo:bar@registry.npmjs.org',
])
// [
//   'Something --x=https://user:***@registry.npmjs.org/ foo bar',
//   '--url=http://foo:***@registry.npmjs.org/',
// ]
```
