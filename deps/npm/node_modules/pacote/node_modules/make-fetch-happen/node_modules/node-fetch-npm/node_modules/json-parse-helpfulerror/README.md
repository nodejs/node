# json-parse-helpfulerror

A drop-in replacement for `JSON.parse` that uses
<https://npmjs.org/jju> to provide more useful error messages in the
event of a parse error.

# Example

## Installation

```
npm i -S json-parse-helpfulerror
```

## Use

```js
var jph = require('json-parse-helpfulerror');

var notJSON = "{'foo': 3}";     // keys must be double-quoted in JSON

JSON.parse(notJSON);            // throws unhelpful error

jph.parse("{'foo': 3}")         // throws more helpful error: "Unexpected token '\''..."
```

# License

MIT