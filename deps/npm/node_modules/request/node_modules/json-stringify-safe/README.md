# json-stringify-safe

Like JSON.stringify, but doesn't throw on circular references.

## Usage

Takes the same arguments as `JSON.stringify`.

```javascript
var stringify = require('json-stringify-safe');
var circularObj = {};
circularObj.circularRef = circularObj;
circularObj.list = [ circularObj, circularObj ];
console.log(stringify(circularObj, null, 2));
```

Output:

```json
{
  "circularRef": "[Circular]",
  "list": [
    "[Circular]",
    "[Circular]"
  ]
}
```
