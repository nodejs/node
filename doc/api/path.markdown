## Path

This module contains utilities for dealing with file paths.  Use
`require('path')` to use it.  It provides the following methods:

### path.join([path1], [path2], [...])

Join all arguments together and resolve the resulting path.

Example:

    node> require('path').join(
    ...   '/foo', 'bar', 'baz/asdf', 'quux', '..')
    '/foo/bar/baz/asdf'

### path.normalizeArray(arr)

Normalize an array of path parts, taking care of `'..'` and `'.'` parts.

Example:

    path.normalizeArray(['', 
      'foo', 'bar', 'baz', 'asdf', 'quux', '..'])
    // returns
    [ '', 'foo', 'bar', 'baz', 'asdf' ]

### path.normalize(p)

Normalize a string path, taking care of `'..'` and `'.'` parts.

Example:

    path.normalize('/foo/bar/baz/asdf/quux/..')
    // returns
    '/foo/bar/baz/asdf'

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

Test whether or not the given path exists.  Then, call the `callback` argument with either true or false.  Example:

    path.exists('/etc/passwd', function (exists) {
      util.debug(exists ? "it's there" : "no passwd!");
    });
