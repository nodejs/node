## Path

This module contains utilities for dealing with file paths.  Use
`require('path')` to use it.  It provides the following methods:

### path.normalize(p)

Normalize a string path, taking care of `'..'` and `'.'` parts.

When multiple slashes are found, they're replaces by a single one;
when the path contains a trailing slash, it is preserved.
On windows backslashes are used. 

Example:

    path.normalize('/foo/bar//baz/asdf/quux/..')
    // returns
    '/foo/bar/baz/asdf'

### path.join([path1], [path2], [...])

Join all arguments together and normalize the resulting path.

Example:

    node> require('path').join(
    ...   '/foo', 'bar', 'baz/asdf', 'quux', '..')
    '/foo/bar/baz/asdf'

### path.resolve([from ...], to)

Resolves `to` to an absolute path name and normalizes it.

One ore more `from` arguments may be provided to specify the the starting
point from where the path will be resolved. `resolve` will prepend `from`
arguments from right to left until an absolute path is found. If no `from`
arguments are specified, or after prepending them still no absolute path is
found, the current working directory will be prepended eventually.

Trailing slashes are removed unless the path gets resolved to the root
directory.

Examples:

    path.resolve('index.html')
    // returns
    '/home/tank/index.html'

    path.resolve('/foo/bar', './baz')
    // returns
    '/foo/baz/baz'

    path.resolve('/foo/bar', '/tmp/file/')
    // returns
    '/tmp/file'

    path.resolve('wwwroot', 'static_files/png/', '../gif/image.gif')
    // returns
    '/home/tank/wwwroot/static_files/gif/image.gif'

### path.dirname(p)

Return the directory name of a path.  Similar to the Unix `dirname` command.

Example:

    path.dirname('/foo/bar/baz/asdf/quux')
    // returns
    '/foo/bar/baz/asdf'

### path.basename(p, [ext])

Return the last portion of a path.  Similar to the Unix `basename` command.

Example:

    path.basename('/foo/bar/baz/asdf/quux.html')
    // returns
    'quux.html'

    path.basename('/foo/bar/baz/asdf/quux.html', '.html')
    // returns
    'quux'

### path.extname(p)

Return the extension of the path.  Everything after the last '.' in the last portion
of the path.  If there is no '.' in the last portion of the path or the only '.' is
the first character, then it returns an empty string.  Examples:

    path.extname('index.html')
    // returns
    '.html'

    path.extname('index')
    // returns
    ''

### path.exists(p, [callback])

Test whether or not the given path exists.  Then, call the `callback` argument
with either true or false. Example:

    path.exists('/etc/passwd', function (exists) {
      util.debug(exists ? "it's there" : "no passwd!");
    });
