# find-npm-prefix

Find the npm project directory associated with for a given directory

## USAGE

```
const findPrefix = require('find-npm-prefix')

findPrefix(process.cwd).then(prefix => {
  …
})
```

## findPrefix(dir) → Promise(prefix)

This computes the npm prefix, that is, the directory that npm adds and
removes modules from for a given path. 

It takes a directory as an argument and returns a promise of the associated
prefix directory.

## Algorithm

1. If the directory is a `node_modules` folder, scan up the tree till you find a non-`node_modules` directory and return that.
2. Else, look for the first parent directory that contains a `node_modules` or a `package.json`
  1. If one is found, that's the prefix.
  2. If none are found, return the original directory we were given
