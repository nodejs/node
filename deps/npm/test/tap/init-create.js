/* eslint-disable standard/no-callback-literal */
var test = require('tap').test
var requireInject = require('require-inject')

var npm = require('../../lib/npm.js')

test('npm init <name>', function (t) {
  var initJsonMock = function () {
    t.ok(false, 'should not run initJson()')
  }
  initJsonMock.yes = function () {
    t.ok(false, 'should not run initJson.yes()')
    return false
  }
  var libnpxMock = function () {
    return Promise.resolve()
  }
  libnpxMock.parseArgs = function (argv, defaultNpm) {
    t.ok(argv[0].includes('node'), 'node is the first arg')
    t.equals(argv[2], '--always-spawn', 'set npx opts.alwaysSpawn')
    t.equals(argv[3], 'create-name', 'expands name')
    t.ok(defaultNpm.endsWith('npm-cli.js'), 'passes npm bin path')
  }

  npm.load({ loglevel: 'silent' }, function () {
    var init = requireInject('../../lib/init', {
      'init-package-json': initJsonMock,
      'libnpx': libnpxMock
    })

    init(['name'], function () {})

    t.end()
  })
})

test('npm init expands scopes', function (t) {
  var libnpxMock = function () {
    return Promise.resolve()
  }

  npm.load({ loglevel: 'silent' }, function () {
    var init = requireInject('../../lib/init', {
      'libnpx': libnpxMock
    })

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], '@scope/create', 'expands @scope')
    }

    init(['@scope'], function () {})

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], '@scope/create-name', 'expands @scope/name')
    }

    init(['@scope/name'], function () {})

    t.end()
  })
})

test('npm init expands version names', function (t) {
  var libnpxMock = function () {
    return Promise.resolve()
  }

  npm.load({ loglevel: 'silent' }, function () {
    var init = requireInject('../../lib/init', {
      'libnpx': libnpxMock
    })

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], 'create-name@1.2.3', 'expands name@1.2.3')
    }

    init(['name@1.2.3'], function () {})

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], 'create-name@^1.2.3', 'expands name@^1.2.3')
    }

    init(['name@^1.2.3'], function () {})

    t.end()
  })
})

test('npm init expands git names', function (t) {
  var libnpxMock = function () {
    return Promise.resolve()
  }

  npm.load({ loglevel: 'silent' }, function () {
    var init = requireInject('../../lib/init', {
      'libnpx': libnpxMock
    })

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], 'user/create-foo', 'expands git repo')
    }

    init(['user/foo'], function () {})

    libnpxMock.parseArgs = function (argv) {
      t.equals(argv[3], 'git+https://github.com/user/create-foo', 'expands git url')
    }

    init(['git+https://github.com/user/foo'], function () {})

    t.end()
  })
})

test('npm init errors on folder and tarballs', function (t) {
  npm.load({ loglevel: 'silent' }, function () {
    var init = require('../../lib/init')

    try {
      init(['../foo/bar/'], function () {})
    } catch (e) {
      t.equals(e.code, 'EUNSUPPORTED')
    }

    t.throws(
      () => init(['../foo/bar/'], function () {}),
      /Unrecognized initializer: \.\.\/foo\/bar\//
    )

    t.throws(
      () => init(['file:foo.tar.gz'], function () {}),
      /Unrecognized initializer: file:foo\.tar\.gz/
    )

    t.throws(
      () => init(['http://x.com/foo.tgz'], function () {}),
      /Unrecognized initializer: http:\/\/x\.com\/foo\.tgz/
    )

    t.end()
  })
})

test('npm init forwards arguments', function (t) {
  var libnpxMock = function () {
    return Promise.resolve()
  }

  npm.load({ loglevel: 'silent' }, function () {
    var origArgv = process.argv
    var init = requireInject('../../lib/init', {
      'libnpx': libnpxMock
    })

    libnpxMock.parseArgs = function (argv) {
      process.argv = origArgv
      t.same(argv.slice(4), ['a', 'b', 'c'])
    }
    process.argv = [
      process.argv0,
      'NPM_CLI_PATH',
      'init',
      'name',
      'a', 'b', 'c'
    ]

    init(['name'], function () {})

    t.end()
  })
})
