# fstream-npm

This is an fstream DirReader class that will read a directory and filter
things according to the semantics of what goes in an npm package.

For example:

```javascript
// This will print out all the files that would be included
// by 'npm publish' or 'npm install' of this directory.

var FN = require("fstream-npm")
FN({ path: "./" })
  .on("child", function (e) {
    console.error(e.path.substr(e.root.path.length + 1))
  })
```

