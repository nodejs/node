# read-installed

Read all the installed packages in a folder, and return a tree
structure with all the data.

npm uses this.

## 1.0.0

Breaking changes in `1.0.0`:

The second argument is now an `Object` that contains the following keys:

 * `depth` optional, defaults to Infinity
 * `log` optional log Function
 * `dev` optional, dev=true to mark devDeps as extraneous

## Usage

```javascript
var readInstalled = require("read-installed")
readInstalled(folder, { depth, log, dev }, function (er, data) {
  ...
})
```
