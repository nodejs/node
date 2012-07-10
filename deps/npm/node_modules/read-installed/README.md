# read-installed

Read all the installed packages in a folder, and return a tree
structure with all the data.

npm uses this.

## Usage

```javascript
var readInstalled = require("read-installed")
// depth is optional, defaults to Infinity
readInstalled(folder, depth, function (er, data) {
  ...
})
```
