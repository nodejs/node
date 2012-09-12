commondir
=========

Compute the closest common parent directory among an array of directories.

example
=======

dir
---

    > var commondir = require('commondir');
    > commondir([ '/x/y/z', '/x/y', '/x/y/w/q' ])
    '/x/y'

base
----

    > var commondir = require('commondir')
    > commondir('/foo/bar', [ '../baz', '../../foo/quux', './bizzy' ])
    '/foo'

methods
=======

var commondir = require('commondir');

commondir(absolutePaths)
------------------------

Compute the closest common parent directory for an array `absolutePaths`.

commondir(basedir, relativePaths)
---------------------------------

Compute the closest common parent directory for an array `relativePaths` which
will be resolved for each `dir` in `relativePaths` according to:
`path.resolve(basedir, dir)`.

installation
============

Using [npm](http://npmjs.org), just do:

    npm install commondir
