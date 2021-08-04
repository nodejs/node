const t = require('tap')

const Access = require('../../lib/access.js')

const npm = {
  output: () => null,
}

t.test('completion', t => {
  const access = new Access({ flatOptions: {} })
  const testComp = (argv, expect) => {
    const res = access.completion({conf: {argv: {remain: argv}}})
    t.resolves(res, expect, argv.join(' '))
  }

  testComp(['npm', 'access'], [
    'public', 'restricted', 'grant', 'revoke', 'ls-packages',
    'ls-collaborators', 'edit', '2fa-required', '2fa-not-required',
  ])
  testComp(['npm', 'access', 'grant'], ['read-only', 'read-write'])
  testComp(['npm', 'access', 'grant', 'read-only'], [])
  testComp(['npm', 'access', 'public'], [])
  testComp(['npm', 'access', 'restricted'], [])
  testComp(['npm', 'access', 'revoke'], [])
  testComp(['npm', 'access', 'ls-packages'], [])
  testComp(['npm', 'access', 'ls-collaborators'], [])
  testComp(['npm', 'access', 'edit'], [])
  testComp(['npm', 'access', '2fa-required'], [])
  testComp(['npm', 'access', '2fa-not-required'], [])
  testComp(['npm', 'access', 'revoke'], [])

  t.rejects(
    access.completion({conf: {argv: {remain: ['npm', 'access', 'foobar']}}}),
    { message: 'foobar not recognized' }
  )

  t.end()
})

t.test('subcommand required', t => {
  const access = new Access({ flatOptions: {} })
  access.exec([], (err) => {
    t.match(err, access.usageError('Subcommand is required.'))
    t.end()
  })
})

t.test('unrecognized subcommand', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec(['blerg'], (err) => {
    t.match(
      err,
      /Usage: blerg is not a recognized subcommand/,
      'should throw EUSAGE on missing subcommand'
    )
    t.end()
  })
})

t.test('edit', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'edit',
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /edit subcommand is not implemented yet/,
      'should throw not implemented yet error'
    )
    t.end()
  })
})

t.test('access public on unscoped package', (t) => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'npm-access-public-pkg',
    }),
  })
  const access = new Access({ prefix })
  access.exec([
    'public',
  ], (err) => {
    t.match(
      err,
      /Usage: This command is only available for scoped packages/,
      'should throw scoped-restricted error'
    )
    t.end()
  })
})

t.test('access public on scoped package', (t) => {
  t.plan(4)
  const name = '@scoped/npm-access-public-pkg'
  const prefix = t.testdir({
    'package.json': JSON.stringify({ name }),
  })
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      public: (pkg, { registry }) => {
        t.equal(pkg, name, 'should use pkg name ref')
        t.equal(
          registry,
          'https://registry.npmjs.org',
          'should forward correct options'
        )
        return true
      },
    },
  })
  const access = new Access({
    flatOptions: { registry: 'https://registry.npmjs.org' },
    prefix,
  })
  access.exec([
    'public',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access public on scoped package')
  })
})

t.test('access public on missing package.json', (t) => {
  const prefix = t.testdir({
    node_modules: {},
  })
  const access = new Access({ prefix })
  access.exec([
    'public',
  ], (err) => {
    t.match(
      err,
      /no package name passed to command and no package.json found/,
      'should throw no package.json found error'
    )
    t.end()
  })
})

t.test('access public on invalid package.json', (t) => {
  const prefix = t.testdir({
    'package.json': '{\n',
    node_modules: {},
  })
  const access = new Access({ prefix })
  access.exec([
    'public',
  ], (err) => {
    t.match(
      err,
      /JSONParseError/,
      'should throw failed to parse package.json'
    )
    t.end()
  })
})

t.test('access restricted on unscoped package', (t) => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'npm-access-restricted-pkg',
    }),
  })
  const access = new Access({ prefix })
  access.exec([
    'restricted',
  ], (err) => {
    t.match(
      err,
      /Usage: This command is only available for scoped packages/,
      'should throw scoped-restricted error'
    )
    t.end()
  })
})

t.test('access restricted on scoped package', (t) => {
  t.plan(4)
  const name = '@scoped/npm-access-restricted-pkg'
  const prefix = t.testdir({
    'package.json': JSON.stringify({ name }),
  })
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      restricted: (pkg, { registry }) => {
        t.equal(pkg, name, 'should use pkg name ref')
        t.equal(
          registry,
          'https://registry.npmjs.org',
          'should forward correct options'
        )
        return true
      },
    },
  })
  const access = new Access({
    flatOptions: { registry: 'https://registry.npmjs.org' },
    prefix,
  })
  access.exec([
    'restricted',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access restricted on scoped package')
  })
})

t.test('access restricted on missing package.json', (t) => {
  const prefix = t.testdir({
    node_modules: {},
  })
  const access = new Access({ prefix })
  access.exec([
    'restricted',
  ], (err) => {
    t.match(
      err,
      /no package name passed to command and no package.json found/,
      'should throw no package.json found error'
    )
    t.end()
  })
})

t.test('access restricted on invalid package.json', (t) => {
  const prefix = t.testdir({
    'package.json': '{\n',
    node_modules: {},
  })
  const access = new Access({ prefix })
  access.exec([
    'restricted',
  ], (err) => {
    t.match(
      err,
      /JSONParseError/,
      'should throw failed to parse package.json'
    )
    t.end()
  })
})

t.test('access grant read-only', (t) => {
  t.plan(5)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      grant: (spec, team, permissions) => {
        t.equal(spec, '@scoped/another', 'should use expected spec')
        t.equal(team, 'myorg:myteam', 'should use expected team')
        t.equal(permissions, 'read-only', 'should forward permissions')
        return true
      },
    },
  })
  const access = new Access({})
  access.exec([
    'grant',
    'read-only',
    'myorg:myteam',
    '@scoped/another',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access grant read-only')
  })
})

t.test('access grant read-write', (t) => {
  t.plan(5)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      grant: (spec, team, permissions) => {
        t.equal(spec, '@scoped/another', 'should use expected spec')
        t.equal(team, 'myorg:myteam', 'should use expected team')
        t.equal(permissions, 'read-write', 'should forward permissions')
        return true
      },
    },
  })
  const access = new Access({})
  access.exec([
    'grant',
    'read-write',
    'myorg:myteam',
    '@scoped/another',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access grant read-write')
  })
})

t.test('access grant current cwd', (t) => {
  t.plan(5)
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'yargs',
    }),
  })
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      grant: (spec, team, permissions) => {
        t.equal(spec, 'yargs', 'should use expected spec')
        t.equal(team, 'myorg:myteam', 'should use expected team')
        t.equal(permissions, 'read-write', 'should forward permissions')
        return true
      },
    },
  })
  const access = new Access({ prefix })
  access.exec([
    'grant',
    'read-write',
    'myorg:myteam',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access grant current cwd')
  })
})

t.test('access grant others', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'grant',
    'rerere',
    'myorg:myteam',
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /Usage: First argument must be either `read-only` or `read-write`./,
      'should throw unrecognized argument error'
    )
    t.end()
  })
})

t.test('access grant missing team args', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'grant',
    'read-only',
    undefined,
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /Usage: `<scope:team>` argument is required./,
      'should throw missing argument error'
    )
    t.end()
  })
})

t.test('access grant malformed team arg', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'grant',
    'read-only',
    'foo',
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /Usage: Second argument used incorrect format.\n/,
      'should throw malformed arg error'
    )
    t.end()
  })
})

t.test('access 2fa-required/2fa-not-required', t => {
  t.plan(2)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      tfaRequired: (spec) => {
        t.equal(spec, '@scope/pkg', 'should use expected spec')
        return true
      },
      tfaNotRequired: (spec) => {
        t.equal(spec, 'unscoped-pkg', 'should use expected spec')
        return true
      },
    },
  })
  const access = new Access({})

  access.exec(['2fa-required', '@scope/pkg'], er => {
    if (er)
      throw er
  })

  access.exec(['2fa-not-required', 'unscoped-pkg'], er => {
    if (er)
      throw er
  })
})

t.test('access revoke', (t) => {
  t.plan(4)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      revoke: (spec, team) => {
        t.equal(spec, '@scoped/another', 'should use expected spec')
        t.equal(team, 'myorg:myteam', 'should use expected team')
        return true
      },
    },
  })
  const access = new Access({})
  access.exec([
    'revoke',
    'myorg:myteam',
    '@scoped/another',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access revoke')
  })
})

t.test('access revoke missing team args', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'revoke',
    undefined,
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /Usage: `<scope:team>` argument is required./,
      'should throw missing argument error'
    )
    t.end()
  })
})

t.test('access revoke malformed team arg', (t) => {
  const access = new Access({ flatOptions: {} })
  access.exec([
    'revoke',
    'foo',
    '@scoped/another',
  ], (err) => {
    t.match(
      err,
      /Usage: First argument used incorrect format.\n/,
      'should throw malformed arg error'
    )
    t.end()
  })
})

t.test('npm access ls-packages with no team', (t) => {
  t.plan(3)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      lsPackages: (entity) => {
        t.equal(entity, 'foo', 'should use expected entity')
        return {}
      },
    },
    '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
  })
  const access = new Access(npm)
  access.exec([
    'ls-packages',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access ls-packages with no team')
  })
})

t.test('access ls-packages on team', (t) => {
  t.plan(3)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      lsPackages: (entity) => {
        t.equal(entity, 'myorg:myteam', 'should use expected entity')
        return {}
      },
    },
  })
  const access = new Access(npm)
  access.exec([
    'ls-packages',
    'myorg:myteam',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access ls-packages on team')
  })
})

t.test('access ls-collaborators on current', (t) => {
  t.plan(3)
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'yargs',
    }),
  })
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      lsCollaborators: (spec) => {
        t.equal(spec, 'yargs', 'should use expected spec')
        return {}
      },
    },
  })
  const access = new Access({ prefix, ...npm })
  access.exec([
    'ls-collaborators',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access ls-collaborators on current')
  })
})

t.test('access ls-collaborators on spec', (t) => {
  t.plan(3)
  const Access = t.mock('../../lib/access.js', {
    libnpmaccess: {
      lsCollaborators: (spec) => {
        t.equal(spec, 'yargs', 'should use expected spec')
        return {}
      },
    },
  })
  const access = new Access(npm)
  access.exec([
    'ls-collaborators',
    'yargs',
  ], (err) => {
    t.error(err, 'npm access')
    t.ok('should successfully access ls-packages with no team')
  })
})
