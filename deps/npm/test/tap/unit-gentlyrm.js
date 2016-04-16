'use strict'
var test = require('tap').test
var requireInject = require('require-inject')

function error (code) {
  var er = new Error()
  er.code = code
  return er
}

function mockWith (fixture) {
  return {
    '../../lib/npm.js': {},
    'graceful-fs': {
      lstat: function (path, cb) {
        var entry = fixture[path]
        if (!entry) return cb(error('ENOENT'))
        cb(null, {
          isDirectory: function () { return entry.type === 'directory' },
          isSymbolicLink: function () { return entry.type === 'symlink' },
          isFile: function () { return entry.type === 'file' || entry.type === 'cmdshim' || entry.type === 'error' }
        })
      },
      readlink: function (path, cb) {
        var entry = fixture[path]
        if (!entry) return cb(error('ENOENT'))
        if (entry.type !== 'symlink') return cb(error('EINVAL'))
        cb(null, entry.dest)
      }
    },
    'read-cmd-shim': function (path, cb) {
      var entry = fixture[path]
      if (!entry) return cb(error('ENOENT'))
      if (entry.type === 'directory') return cb(error('EISDIR'))
      if (entry.type === 'error') return cb(error(entry.code))
      if (entry.type !== 'cmdshim') return cb(error('ENOTASHIM'))
      cb(null, entry.dest)
    }
  }
}

test('readLinkOrShim', function (t) {
  t.plan(10)

  var mocks = mockWith({
    '/path/to/directory': { type: 'directory' },
    '/path/to/link': { type: 'symlink', dest: '../to/file' },
    '/path/to/file': { type: 'file' },
    '/path/to/cmdshim': { type: 'cmdshim', dest: '../to/file' },
    '/path/to/invalid': { type: 'error', code: 'EINVAL' }
  })

  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mocks)
  var readLinkOrShim = gentlyRm._readLinkOrShim

  readLinkOrShim('/path/to/nowhere', function (er, path) {
    t.is(er && er.code, 'ENOENT', 'missing files are errors')
  })
  readLinkOrShim('/path/to/invalid', function (er, path) {
    t.is(er && er.code, 'EINVAL', 'other errors pass through too')
  })
  readLinkOrShim('/path/to/directory', function (er, path) {
    t.ifError(er, "reading dirs isn't an error")
    t.is(path, null, 'reading non links/cmdshims gives us null')
  })
  readLinkOrShim('/path/to/file', function (er, path) {
    t.ifError(er, "reading non-cmdshim files isn't an error")
    t.is(path, null, 'reading non links/cmdshims gives us null')
  })
  readLinkOrShim('/path/to/link', function (er, path) {
    t.ifError(er, "reading links isn't an error")
    t.is(path, '../to/file', 'reading links works')
  })
  readLinkOrShim('/path/to/cmdshim', function (er, path) {
    t.ifError(er, "reading cmdshims isn't an error")
    t.is(path, '../to/file', 'reading cmdshims works')
  })
  t.done()
})

test('resolveSymlink', function (t) {
  t.plan(9)

  var mocks = mockWith({
    '/path/to/directory': { type: 'directory' },
    '/path/to/link': { type: 'symlink', dest: '../to/file' },
    '/path/to/file': { type: 'file' },
    '/path/to/cmdshim': { type: 'cmdshim', dest: '../to/file' }
  })

  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mocks)
  var resolveSymlink = gentlyRm._resolveSymlink

  resolveSymlink('/path/to/nowhere', function (er, path) {
    t.is(er && er.code, 'ENOENT', 'missing files are errors')
  })
  resolveSymlink('/path/to/directory', function (er, path) {
    t.ifError(er, "reading dirs isn't an error")
    t.is(path, '/path/to/directory', 'reading non links/cmdshims gives us path we passed in')
  })
  resolveSymlink('/path/to/file', function (er, path) {
    t.ifError(er, "reading non-cmdshim files isn't an error")
    t.is(path, '/path/to/file', 'reading non links/cmdshims gives us the path we passed in')
  })
  resolveSymlink('/path/to/link', function (er, path) {
    t.ifError(er, "reading links isn't an error")
    t.is(path, '/path/to/file', 'reading links works')
  })
  resolveSymlink('/path/to/cmdshim', function (er, path) {
    t.ifError(er, "reading cmdshims isn't an error")
    t.is(path, '/path/to/file', 'reading cmdshims works')
  })
  t.done()
})

test('readAllLinks', function (t) {
  t.plan(16)

  var mocks = mockWith({
    '/path/to/directory': { type: 'directory' },
    '/path/to/link': { type: 'symlink', dest: '../to/file' },
    '/path/to/file': { type: 'file' },
    '/path/to/cmdshim': { type: 'cmdshim', dest: '../to/file' },
    '/path/to/linktolink': { type: 'symlink', dest: 'link' },
    '/path/to/linktolink^2': { type: 'symlink', dest: 'linktolink' },
    '/path/to/linktocmdshim': { type: 'symlink', dest: 'cmdshim' },
    '/path/to/linktobad': { type: 'symlink', dest: '/does/not/exist' }
  })

  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mocks)
  var readAllLinks = gentlyRm._readAllLinks

  readAllLinks('/path/to/nowhere', function (er, path) {
    t.is(er && er.code, 'ENOENT', 'missing files are errors')
  })
  readAllLinks('/path/to/directory', function (er, path) {
    t.ifError(er, "reading dirs isn't an error")
    t.isDeeply(path, ['/path/to/directory'], 'reading non links/cmdshims gives us path we passed in')
  })
  readAllLinks('/path/to/file', function (er, path) {
    t.ifError(er, "reading non-cmdshim files isn't an error")
    t.isDeeply(path, ['/path/to/file'], 'reading non links/cmdshims gives us the path we passed in')
  })
  readAllLinks('/path/to/linktobad', function (er, path) {
    t.is(er && er.code, 'ENOENT', 'links to missing files are errors')
  })
  readAllLinks('/path/to/link', function (er, path) {
    t.ifError(er, "reading links isn't an error")
    t.isDeeply(path, ['/path/to/link', '/path/to/file'], 'reading links works')
  })
  readAllLinks('/path/to/cmdshim', function (er, path) {
    t.ifError(er, "reading cmdshims isn't an error")
    t.isDeeply(path, ['/path/to/cmdshim', '/path/to/file'], 'reading cmdshims works')
  })
  readAllLinks('/path/to/linktolink', function (er, path) {
    t.ifError(er, "reading link to link isn't an error")
    t.isDeeply(path, ['/path/to/linktolink', '/path/to/link', '/path/to/file'], 'reading link to link works')
  })
  readAllLinks('/path/to/linktolink^2', function (er, path) {
    t.ifError(er, "reading link to link to link isn't an error")
    t.isDeeply(path, ['/path/to/linktolink^2', '/path/to/linktolink', '/path/to/link', '/path/to/file'], 'reading link to link to link works')
  })
  readAllLinks('/path/to/linktocmdshim', function (er, path) {
    t.ifError(er, "reading link to cmdshim isn't an error")
    t.isDeeply(path, ['/path/to/linktocmdshim', '/path/to/cmdshim', '/path/to/file'], 'reading link to cmdshim works')
  })
  t.done()
})

test('areAnyInsideAny', function (t) {
  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mockWith({}))
  var areAnyInsideAny = gentlyRm._areAnyInsideAny

  var noneOneToOne = areAnyInsideAny(['/abc'], ['/xyz'])
  t.is(noneOneToOne, false, 'none inside: one to one')
  var noneOneToMany = areAnyInsideAny(['/abc'], ['/rst', '/uvw', '/xyz'])
  t.is(noneOneToMany, false, 'none inside: one to many')
  var noneManyToOne = areAnyInsideAny(['/abc', '/def', '/ghi'], ['/xyz'])
  t.is(noneManyToOne, false, 'none inside: many to one')
  var noneManyToMany = areAnyInsideAny(['/abc', '/def', '/ghi'], ['/rst', '/uvw', '/xyz'])
  t.is(noneManyToMany, false, 'none inside: many to many')

  var oneToOne = areAnyInsideAny(['/one/toOne'], ['/one'])
  t.isDeeply(oneToOne, {target: '/one/toOne', path: '/one'}, 'first: one to one')

  var firstOneToMany = areAnyInsideAny(['/abc/def'], ['/abc', '/def', '/ghi'])
  t.isDeeply(firstOneToMany, {target: '/abc/def', path: '/abc'}, 'first: one to many')
  var secondOneToMany = areAnyInsideAny(['/def/ghi'], ['/abc', '/def', '/ghi'])
  t.isDeeply(secondOneToMany, {target: '/def/ghi', path: '/def'}, 'second: one to many')
  var lastOneToMany = areAnyInsideAny(['/ghi/jkl'], ['/abc', '/def', '/ghi'])
  t.isDeeply(lastOneToMany, {target: '/ghi/jkl', path: '/ghi'}, 'last: one to many')

  var firstManyToOne = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/abc'])
  t.isDeeply(firstManyToOne, {target: '/abc/def', path: '/abc'}, 'first: many to one')
  var secondManyToOne = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/uvw'])
  t.isDeeply(secondManyToOne, {target: '/uvw/def', path: '/uvw'}, 'second: many to one')
  var lastManyToOne = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/xyz'])
  t.isDeeply(lastManyToOne, {target: '/xyz/def', path: '/xyz'}, 'last: many to one')

  var firstToFirst = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/abc', '/uvw', '/xyz'])
  t.isDeeply(firstToFirst, {target: '/abc/def', path: '/abc'}, 'first to first: many to many')
  var firstToSecond = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/nope', '/abc', '/xyz'])
  t.isDeeply(firstToSecond, {target: '/abc/def', path: '/abc'}, 'first to second: many to many')
  var firstToLast = areAnyInsideAny(['/abc/def', '/uvw/def', '/xyz/def'], ['/nope', '/nooo', '/abc'])
  t.isDeeply(firstToLast, {target: '/abc/def', path: '/abc'}, 'first to last: many to many')

  var secondToFirst = areAnyInsideAny(['/!!!', '/abc/def', '/xyz/def'], ['/abc', '/uvw', '/xyz'])
  t.isDeeply(secondToFirst, {target: '/abc/def', path: '/abc'}, 'second to first: many to many')
  var secondToSecond = areAnyInsideAny(['/!!!', '/abc/def', '/xyz/def'], ['/nope', '/abc', '/xyz'])
  t.isDeeply(secondToSecond, {target: '/abc/def', path: '/abc'}, 'second to second: many to many')
  var secondToLast = areAnyInsideAny(['/!!!', '/abc/def', '/uvw/def'], ['/nope', '/nooo', '/abc'])
  t.isDeeply(secondToLast, {target: '/abc/def', path: '/abc'}, 'second to last: many to many')

  var lastToFirst = areAnyInsideAny(['/!!!', '/???', '/abc/def'], ['/abc', '/uvw', '/xyz'])
  t.isDeeply(lastToFirst, {target: '/abc/def', path: '/abc'}, 'last to first: many to many')
  var lastToSecond = areAnyInsideAny(['/!!!', '/???', '/abc/def'], ['/nope', '/abc', '/xyz'])
  t.isDeeply(lastToSecond, {target: '/abc/def', path: '/abc'}, 'last to second: many to many')
  var lastToLast = areAnyInsideAny(['/!!!', '/???', '/abc/def'], ['/nope', '/nooo', '/abc'])
  t.isDeeply(lastToLast, {target: '/abc/def', path: '/abc'}, 'last to last: many to many')

  t.done()
})

test('isEverInside', function (t) {
  t.plan(15)

  var mocks = mockWith({
    '/path/other/link': { type: 'symlink', dest: '../to/file' },
    '/path/to/file': { type: 'file' },
    '/path/to': { type: 'directory' },
    '/linkpath': { type: 'symlink', dest: '../path/to' },
    '/path/to/invalid': { type: 'error', code: 'EINVAL' }
  })

  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mocks)
  var isEverInside = gentlyRm._isEverInside

  isEverInside('/path/to/file', ['/path/to', '/path/to/invalid'], function (er, inside) {
    t.ifError(er)
    t.isDeeply(inside, {target: '/path/to/file', path: '/path/to'}, 'bad paths are ignored if something matches')
  })

  isEverInside('/path/to/invalid', ['/path/to/invalid'], function (er, inside) {
    t.is(er && er.code, 'EINVAL', 'errors bubble out')
  })

  isEverInside('/path/to/file', ['/ten'], function (er, inside) {
    t.ifError(er)
    t.is(inside, false, 'not inside')
  })
  isEverInside('/path/to/nowhere', ['/ten'], function (er, inside) {
    t.ifError(er)
    t.is(inside, false, 'missing target')
  })

  isEverInside('/path/to/file', ['/path/to'], function (er, inside) {
    t.ifError(er)
    t.isDeeply(inside, {target: '/path/to/file', path: '/path/to'}, 'plain file in plain path')
  })
  isEverInside('/path/other/link', ['/path/to'], function (er, inside) {
    t.ifError(er)
    t.isDeeply(inside, {target: '/path/to/file', path: '/path/to'}, 'link in plain path')
  })

  isEverInside('/path/to/file', ['/linkpath'], function (er, inside) {
    t.ifError(er)
    t.isDeeply(inside, {target: '/path/to/file', path: '/path/to'}, 'plain file in link path')
  })
  isEverInside('/path/other/link', ['/linkpath'], function (er, inside) {
    t.ifError(er)
    t.isDeeply(inside, {target: '/path/to/file', path: '/path/to'}, 'link in link path')
  })

  t.done()
})

test('isSafeToRm', function (t) {
  var gentlyRm = requireInject('../../lib/utils/gently-rm.js', mockWith({}))
  var isSafeToRm = gentlyRm._isSafeToRm

  t.plan(12)

  function testIsSafeToRm (t, parent, target, shouldPath, shouldBase, msg) {
    isSafeToRm(parent, target, function (er, path, base) {
      t.ifError(er, msg + ' no error')
      t.is(path, shouldPath, msg + ' path')
      t.is(base, shouldBase, msg + ' base')
    })
  }

  function testNotIsSafeToRm (t, parent, target, msg) {
    isSafeToRm(parent, target, function (er) {
      t.is(er && er.code, 'EEXIST', msg + ' error')
    })
  }

  var unmanagedParent = {path: '/foo', managed: false}
  var managedParent = {path: '/foo', managed: true}
  var targetInParent = {
    path: '/foo/bar/baz',
    inParent: {
      target: '/foo/bar/baz',
      path: '/foo'
    }
  }
  var targetLinkInParent = {
    path: '/foo/bar/baz',
    inParent: {
      target: '/other/area/baz',
      path: '/other/area'
    }
  }
  var targetManagedLinkNotInParent = {
    path: '/foo/bar/baz',
    managed: true,
    inParent: false,
    symlink: '/foo/bar/bark'
  }
  var targetUnmanagedLink = {
    path: '/not/managed/baz',
    managed: false,
    inParent: false,
    symlink: '/not/managed/foo'
  }
  var targetUnmanagedFile = {
    path: '/not/managed/baz',
    managed: false,
    inParent: false,
    symlink: false
  }
  testNotIsSafeToRm(t, unmanagedParent, targetInParent, 'unmanaged parent')
  testIsSafeToRm(t, managedParent, targetInParent, '/foo/bar/baz', '/foo', 'path is in parent')
  testIsSafeToRm(t, managedParent, targetLinkInParent, '/foo/bar/baz', '/foo/bar', 'path links to parent')
  testIsSafeToRm(t, managedParent, targetManagedLinkNotInParent, undefined, undefined, 'managed but not owned by package')
  testNotIsSafeToRm(t, managedParent, targetUnmanagedLink, 'unmanaged link')
  testNotIsSafeToRm(t, managedParent, targetUnmanagedFile, 'unmanaged file')
})
