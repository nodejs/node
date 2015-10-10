var common = require('../common-tap.js')
var test = require('tap').test
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var tmp = osenv.tmpdir()
var t1dir = path.resolve(tmp, 'view-local-no-pkg')
var t2dir = path.resolve(tmp, 'view-local-notmine')
var t3dir = path.resolve(tmp, 'view-local-mine')
var mr = require('npm-registry-mock')

test('setup', function (t) {
  mkdirp.sync(t1dir)
  mkdirp.sync(t2dir)
  mkdirp.sync(t3dir)

  fs.writeFileSync(t2dir + '/package.json', JSON.stringify({
    author: 'Evan Lucas',
    name: 'test-repo-url-https',
    version: '0.0.1'
  }), 'utf8')

  fs.writeFileSync(t3dir + '/package.json', JSON.stringify({
    author: 'Evan Lucas',
    name: 'biscuits',
    version: '0.0.1'
  }), 'utf8')

  t.pass('created fixtures')
  t.end()
})

function plugin (server) {
  server
    .get('/biscuits')
    .many()
    .reply(404, {'error': 'version not found'})
}

test('npm view . in global mode', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry,
    '--global'
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Cannot use view command in global mode./m)
    t.end()
  })
})

test('npm view --global', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '--registry=' + common.registry,
    '--global'
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Cannot use view command in global mode./m)
    t.end()
  })
})

test('npm view . with no package.json', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Invalid package.json/m)
    t.end()
  })
})

test('npm view . with no published package', function (t) {
  process.chdir(t3dir)
  mr({ port: common.port, plugin: plugin}, function (er, s) {
    common.npm([
      'view',
      '.',
      '--registry=' + common.registry
    ], { cwd: t3dir }, function (err, code, stdout, stderr) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 1, 'exit not ok')
      t.similar(stderr, /version not found/m)
      s.close()
      t.end()
    })
  })
})

test('npm view .', function (t) {
  process.chdir(t2dir)
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      '.',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      var re = new RegExp("name: 'test-repo-url-https'")
      t.similar(stdout, re)
      s.close()
      t.end()
    })
  })
})

test('npm view . select fields', function (t) {
  process.chdir(t2dir)
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      '.',
      'main',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout.trim(), 'index.js', 'should print `index.js`')
      s.close()
      t.end()
    })
  })
})

test('npm view .@<version>', function (t) {
  process.chdir(t2dir)
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      '.@0.0.0',
      'version',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout.trim(), '0.0.0', 'should print `0.0.0`')
      s.close()
      t.end()
    })
  })
})

test('npm view .@<version> --json', function (t) {
  process.chdir(t2dir)
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      '.@0.0.0',
      'version',
      '--json',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout.trim(), '"0.0.0"', 'should print `"0.0.0"`')
      s.close()
      t.end()
    })
  })
})

test('npm view <package name>', function (t) {
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      'underscore',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      var re = new RegExp("name: 'underscore'")
      t.similar(stdout, re, 'should have name `underscore`')
      s.close()
      t.end()
    })
  })
})

test('npm view <package name> --global', function (t) {
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      'underscore',
      '--global',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      var re = new RegExp("name: 'underscore'")
      t.similar(stdout, re, 'should have name `underscore`')
      s.close()
      t.end()
    })
  })
})

test('npm view <package name> --json', function (t) {
  t.plan(3)
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      'underscore',
      '--json',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      s.close()
      try {
        var out = JSON.parse(stdout.trim())
        t.similar(out, {
          maintainers: ['jashkenas <jashkenas@gmail.com>']
        }, 'should have the same maintainer')
      } catch (er) {
        t.fail('Unable to parse JSON')
      }
    })
  })
})

test('npm view <package name> <field>', function (t) {
  mr({ port: common.port, plugin: plugin }, function (er, s) {
    common.npm([
      'view',
      'underscore',
      'homepage',
      '--registry=' + common.registry
    ], { cwd: t2dir }, function (err, code, stdout) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout.trim(), 'http://underscorejs.org',
        'homepage should equal `http://underscorejs.org`')
      s.close()
      t.end()
    })
  })
})

test('npm view with invalid package name', function (t) {
  var invalidName = 'InvalidPackage'
  var obj = {}
  obj['/' + invalidName] = [404, {'error': 'not found'}]

  mr({ port: common.port, mocks: { 'get': obj } }, function (er, s) {
    common.npm([
      'view',
      invalidName,
      '--registry=' + common.registry
    ], {}, function (err, code, stdout, stderr) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 1, 'exit not ok')

      t.similar(stderr, new RegExp('is not in the npm registry'),
        'Package should NOT be found')

      t.dissimilar(stderr, new RegExp('use the name yourself!'),
        'Suggestion should not be there')

      t.similar(stderr, new RegExp('name can no longer contain capital letters'),
        'Suggestion about Capital letter should be there')

      s.close()
      t.end()
    })
  })
})

test('npm view with valid but non existent package name', function (t) {
  mr({ port: common.port, mocks: {
      'get': {
          '/valid-but-non-existent-package': [404, {'error': 'not found'}]
      }
  }}, function (er, s) {
    common.npm([
      'view',
      'valid-but-non-existent-package',
      '--registry=' + common.registry
    ], {}, function (err, code, stdout, stderr) {
      t.ifError(err, 'view command finished successfully')
      t.equal(code, 1, 'exit not ok')

      t.similar(stderr,
        new RegExp("'valid-but-non-existent-package' is not in the npm registry\."),
        'Package should NOT be found')

      t.similar(stderr, new RegExp('use the name yourself!'),
        'Suggestion should be there')

      s.close()
      t.end()
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(t1dir)
  rimraf.sync(t2dir)
  rimraf.sync(t3dir)
  t.pass('cleaned up')
  t.end()
})
