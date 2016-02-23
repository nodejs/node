var fs = require('fs')
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')

var test = require('tap').test
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'access')
var server

var scoped = {
  name: '@scoped/pkg',
  version: '1.1.1'
}

test('setup', function (t) {
  mkdirp(pkg, function (er) {
    t.ifError(er, pkg + ' made successfully')

    mr({port: common.port}, function (err, s) {
      t.ifError(err, 'registry mocked successfully')
      server = s

      fs.writeFile(
        path.join(pkg, 'package.json'),
        JSON.stringify(scoped),
        function (er) {
          t.ifError(er, 'wrote package.json')
          t.end()
        }
      )
    })
  })
})

test('npm access public on current package', function (t) {
  server.post('/-/package/%40scoped%2Fpkg/access', JSON.stringify({
    access: 'public'
  })).reply(200, {
    accessChanged: true
  })
  common.npm(
    [
      'access',
      'public',
      '--registry', common.registry,
      '--loglevel', 'silent'
    ], {
      cwd: pkg
    },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access')
      t.equal(code, 0, 'exited OK')
      t.equal(stderr, '', 'no error output')
      t.end()
    }
  )
})

test('npm access public when no package passed and no package.json', function (t) {
  // need to simulate a missing package.json
  var missing = path.join(__dirname, 'access-public-missing-guard')
  mkdirp.sync(path.join(missing, 'node_modules'))

  common.npm([
    'access',
    'public',
    '--registry', common.registry
  ], {
    cwd: missing
  },
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm access')
    t.match(stderr, /no package name passed to command and no package.json found/)
    rimraf.sync(missing)
    t.end()
  })
})

test('npm access public when no package passed and invalid package.json', function (t) {
  // need to simulate a missing package.json
  var invalid = path.join(__dirname, 'access-public-invalid-package')
  mkdirp.sync(path.join(invalid, 'node_modules'))
  // it's hard to force `read-package-json` to break w/o ENOENT, but this will do it
  fs.writeFileSync(path.join(invalid, 'package.json'), '{\n')

  common.npm([
    'access',
    'public',
    '--registry', common.registry
  ], {
    cwd: invalid
  },
  function (er, code, stdout, stderr) {
    t.ifError(er, 'npm access')
    t.match(stderr, /Failed to parse json/)
    rimraf.sync(invalid)
    t.end()
  })
})

test('npm access restricted on current package', function (t) {
  server.post('/-/package/%40scoped%2Fpkg/access', JSON.stringify({
    access: 'restricted'
  })).reply(200, {
    accessChanged: true
  })
  common.npm(
    [
      'access',
      'restricted',
      '--registry', common.registry,
      '--loglevel', 'silent'
    ], {
      cwd: pkg
    },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access')
      t.equal(code, 0, 'exited OK')
      t.equal(stderr, '', 'no error output')
      t.end()
    }
  )
})

test('npm access on named package', function (t) {
  server.post('/-/package/%40scoped%2Fanother/access', {
    access: 'public'
  }).reply(200, {
    accessChaged: true
  })
  common.npm(
    [
      'access',
      'public', '@scoped/another',
      '--registry', common.registry,
      '--loglevel', 'silent'
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access')
      t.equal(code, 0, 'exited OK')
      t.equal(stderr, '', 'no error output')

      t.end()
    }
  )
})

test('npm change access on unscoped package', function (t) {
  common.npm(
    [
      'access',
      'restricted', 'yargs',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.matches(
        stderr, /access commands are only accessible for scoped packages/)
      t.end()
    }
  )
})

test('npm access grant read-only', function (t) {
  server.put('/-/team/myorg/myteam/package', {
    permissions: 'read-only',
    package: '@scoped/another'
  }).reply(201, {
    accessChaged: true
  })
  common.npm(
    [
      'access',
      'grant', 'read-only',
      'myorg:myteam',
      '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access grant')
      t.equal(code, 0, 'exited with Error')
      t.end()
    }
  )
})

test('npm access grant read-write', function (t) {
  server.put('/-/team/myorg/myteam/package', {
    permissions: 'read-write',
    package: '@scoped/another'
  }).reply(201, {
    accessChaged: true
  })
  common.npm(
    [
      'access',
      'grant', 'read-write',
      'myorg:myteam',
      '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access grant')
      t.equal(code, 0, 'exited with Error')
      t.end()
    }
  )
})

test('npm access grant others', function (t) {
  common.npm(
    [
      'access',
      'grant', 'rerere',
      'myorg:myteam',
      '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.matches(stderr, /read-only/)
      t.matches(stderr, /read-write/)
      t.end()
    }
  )
})

test('npm access revoke', function (t) {
  server.delete('/-/team/myorg/myteam/package', {
    package: '@scoped/another'
  }).reply(200, {
    accessChaged: true
  })
  common.npm(
    [
      'access',
      'revoke',
      'myorg:myteam',
      '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access grant')
      t.equal(code, 0, 'exited with Error')
      t.end()
    }
  )
})

test('npm access ls-packages with no team', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  server.get(
    '/-/org/username/package?format=cli'
  ).reply(200, serverPackages)
  common.npm(
    [
      'access',
      'ls-packages',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-packages')
      t.same(JSON.parse(stdout), clientPackages)
      t.end()
    }
  )
})

test('npm access ls-packages on team', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  server.get(
    '/-/team/myorg/myteam/package?format=cli'
  ).reply(200, serverPackages)
  common.npm(
    [
      'access',
      'ls-packages',
      'myorg:myteam',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-packages')
      t.same(JSON.parse(stdout), clientPackages)
      t.end()
    }
  )
})

test('npm access ls-packages on org', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  server.get(
    '/-/org/myorg/package?format=cli'
  ).reply(200, serverPackages)
  common.npm(
    [
      'access',
      'ls-packages',
      'myorg',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-packages')
      t.same(JSON.parse(stdout), clientPackages)
      t.end()
    }
  )
})

test('npm access ls-packages on user', function (t) {
  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  server.get(
    '/-/org/myorg/package?format=cli'
  ).reply(404, {error: 'nope'})
  server.get(
    '/-/user/myorg/package?format=cli'
  ).reply(200, serverPackages)
  common.npm(
    [
      'access',
      'ls-packages',
      'myorg',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-packages')
      t.same(JSON.parse(stdout), clientPackages)
      t.end()
    }
  )
})

test('npm access ls-packages with no package specified or package.json', function (t) {
  // need to simulate a missing package.json
  var missing = path.join(__dirname, 'access-missing-guard')
  mkdirp.sync(path.join(missing, 'node_modules'))

  var serverPackages = {
    '@foo/bar': 'write',
    '@foo/util': 'read'
  }
  var clientPackages = {
    '@foo/bar': 'read-write',
    '@foo/util': 'read-only'
  }
  server.get(
    '/-/org/myorg/package?format=cli'
  ).reply(404, {error: 'nope'})
  server.get(
    '/-/user/myorg/package?format=cli'
  ).reply(200, serverPackages)
  common.npm(
    [
      'access',
      'ls-packages',
      'myorg',
      '--registry', common.registry
    ],
    { cwd: missing },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-packages')
      t.same(JSON.parse(stdout), clientPackages)
      rimraf.sync(missing)
      t.end()
    }
  )
})

test('npm access ls-collaborators on current', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  server.get(
    '/-/package/%40scoped%2Fpkg/collaborators?format=cli'
  ).reply(200, serverCollaborators)
  common.npm(
    [
      'access',
      'ls-collaborators',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-collaborators')
      t.same(JSON.parse(stdout), clientCollaborators)
      t.end()
    }
  )
})

test('npm access ls-collaborators on package', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  server.get(
    '/-/package/%40scoped%2Fanother/collaborators?format=cli'
  ).reply(200, serverCollaborators)
  common.npm(
    [
      'access',
      'ls-collaborators',
      '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-collaborators')
      t.same(JSON.parse(stdout), clientCollaborators)
      t.end()
    }
  )
})

test('npm access ls-collaborators on current w/user filter', function (t) {
  var serverCollaborators = {
    'myorg:myteam': 'write',
    'myorg:anotherteam': 'read'
  }
  var clientCollaborators = {
    'myorg:myteam': 'read-write',
    'myorg:anotherteam': 'read-only'
  }
  server.get(
    '/-/package/%40scoped%2Fanother/collaborators?format=cli&user=zkat'
  ).reply(200, serverCollaborators)
  common.npm(
    [
      'access',
      'ls-collaborators',
      '@scoped/another',
      'zkat',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm access ls-collaborators')
      t.same(JSON.parse(stdout), clientCollaborators)
      t.end()
    }
  )
})

test('npm access edit', function (t) {
  common.npm(
    [
      'access',
      'edit', '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.match(stderr, /edit subcommand is not implemented yet/)
      t.end()
    }
  )
})

test('npm access blerg', function (t) {
  common.npm(
    [
      'access',
      'blerg', '@scoped/another',
      '--registry', common.registry
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.matches(stderr, /Usage:/)
      t.end()
    }
  )
})

test('cleanup', function (t) {
  t.pass('cleaned up')
  rimraf.sync(pkg)
  server.done()
  server.close()
  t.end()
})
