# @npmcli/name-from-folder

Get the package name from a folder path, including the scope if the
basename of the dirname starts with `@`.

For a path like `/x/y/z/@scope/pkg` it'll return `@scope/pkg`.  If the path
name is something like `/x/y/z/pkg`, then it'll return `pkg`.

## USAGE

```js
const nameFromFolder = require('@npmcli/name-from-folder')
const name = nameFromFolder('/some/folder/path')
```
