# read-package-tree

[![Build Status](https://travis-ci.org/npm/read-package-tree.svg?branch=master)](https://travis-ci.org/npm/read-package-tree)

Read the contents of node_modules.

## USAGE

```javascript
var rpt = require ('read-package-tree')
rpt('/path/to/pkg/root', function (node, kidName) {
  // optional filter function– if included, each package folder found is passed to
  // it to see if it should be included in the final tree
  // node is what we're adding children to
  // kidName is the directory name of the module we're considering adding
  // return true -> include, false -> skip
}, function (er, data) {
  // er means that something didn't work.
  // data is a structure like:
  // {
  //   package: <package.json data, or an empty object>
  //   package.name: defaults to `basename(path)`
  //   children: [ <more things like this> ]
  //   parent: <thing that has this in its children property, or null>
  //   path: <path loaded>
  //   realpath: <the real path on disk>
  //   isLink: <set if this is a Link>
  //   target: <if a Link, then this is the actual Node>
  //   error: <if set, the error we got loading/parsing the package.json>
  // }
})

// or promise-style
rpt('/path/to/pkg/root').then(data => { ... })
```

That's it.  It doesn't figure out if dependencies are met, it doesn't
mutate package.json data objects (beyond what
[read-package-json](http://npm.im/read-package-json) already does), it
doesn't limit its search to include/exclude `devDependencies`, or
anything else.

Just follows the links in the `node_modules` hierarchy and reads the
package.json files it finds therein.

## Symbolic Links

When there are symlinks to packages in the `node_modules` hierarchy, a
`Link` object will be created, with a `target` that is a `Node`
object.

For the most part, you can treat `Link` objects just the same as
`Node` objects.  But if your tree-walking program needs to treat
symlinks differently from normal folders, then make sure to check the
object.

In a given `read-package-tree` run, a specific `path` will always
correspond to a single object, and a specific `realpath` will always
correspond to a single `Node` object.  This means that you may not be
able to pass the resulting data object to `JSON.stringify`, because it
may contain cycles.

## Errors

Errors parsing or finding a package.json in node_modules will result in a
node with the error property set.  We will still find deeper node_modules
if any exist. *Prior to `5.0.0` these aborted tree reading with an error
callback.*

Only a few classes of errors are fatal (result in an error callback):

* If the top level location is entirely missing, that will error.
* if `fs.realpath` returns an error for any path its trying to resolve.
