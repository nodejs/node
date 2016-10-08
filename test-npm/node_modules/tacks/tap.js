'use strict'
exports.areTheSame = function (tap, actual, expected, msg) {
  return tap.test(msg, function (t) {
    compare(t, '/', actual.fixture, expected.fixture)
    t.done()
  })
}

function join (p1, p2) {
  if (p1 === '/') return p1 + p2
  return p1 + '/' + p2
}

function compare (t, path, actual, expected) {
  t.is(actual.type, expected.type, path + ': type')
  if (actual.type !== expected.type) return
  if (expected.type === 'dir') {
    return compareDir(t, path, actual, expected)
  } else if (expected.type === 'file') {
    return compareFile(t, path, actual, expected)
  } else if (expected.type === 'symlink') {
    return compareSymlink(t, path, actual, expected)
  } else {
    throw new Error('what? ' + expected.type)
  }
}

function compareDir (t, path, actual, expected) {
  expected.forContents(function (expectedContent, filename) {
    if (!actual.contents[filename]) {
      t.fail(join(path, filename) + ' missing file')
      return
    }
    compare(t, join(path, filename), actual.contents[filename], expectedContent)
  })
  actual.forContents(function (_, filename) {
    if (!expected.contents[filename]) {
      t.fail(join(path, filename) + ' extraneous file')
    }
  })
}

function compareFile (t, path, actual, expected) {
  if (Buffer.isBuffer(expected.contents)) {
    t.same(Buffer(actual.contents), expected.contents, path + ': file buffer content')
  } else {
    t.is(actual.contents.toString(), expected.contents, path + ': file string content')
  }
}

function compareSymlink(t, path, actual, expected) {
  t.is(actual.contents, expected.contents, path + ': symlink destination')
}
