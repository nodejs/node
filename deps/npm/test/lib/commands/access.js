const t = require('tap')

const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')

t.test('completion', async t => {
  const { npm } = await loadMockNpm(t)
  const access = await npm.cmd('access')
  const testComp = (argv, expect) => {
    const res = access.completion({ conf: { argv: { remain: argv } } })
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

  await t.rejects(
    access.completion({ conf: { argv: { remain: ['npm', 'access', 'foobar'] } } }),
    { message: 'foobar not recognized' }
  )
})

t.test('subcommand required', async t => {
  const { npm } = await loadMockNpm(t)
  const access = await npm.cmd('access')
  await t.rejects(
    npm.exec('access', []),
    access.usageError('Subcommand is required.')
  )
})

t.test('unrecognized subcommand', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['blerg']),
    /Usage: blerg is not a recognized subcommand/,
    'should throw EUSAGE on missing subcommand'
  )
})

t.test('edit', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['edit', '@scoped/another']),
    /edit subcommand is not implemented yet/,
    'should throw not implemented yet error'
  )
})

t.test('access public on unscoped package', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'npm-access-public-pkg',
      }),
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    /Usage: This command is only available for scoped packages/,
    'should throw scoped-restricted error'
  )
})

t.test('access public on scoped package', async t => {
  t.plan(2)
  const name = '@scoped/npm-access-public-pkg'
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        public: (pkg, { registry }) => {
          t.equal(pkg, name, 'should use pkg name ref')
          t.equal(
            registry,
            'https://registry.npmjs.org/',
            'should forward correct options'
          )
          return true
        },
      },
    },
    prefixDir: {
      'package.json': JSON.stringify({ name }),
    },
  })
  await npm.exec('access', ['public'])
})

t.test('access public on missing package.json', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['public']),
    /no package name passed to command and no package.json found/,
    'should throw no package.json found error'
  )
})

t.test('access public on invalid package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{\n',
      node_modules: {},
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    { code: 'EJSONPARSE' },
    'should throw failed to parse package.json'
  )
})

t.test('access restricted on unscoped package', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'npm-access-restricted-pkg',
      }),
    },
  })
  await t.rejects(
    npm.exec('access', ['public']),
    /Usage: This command is only available for scoped packages/,
    'should throw scoped-restricted error'
  )
})

t.test('access restricted on scoped package', async t => {
  t.plan(2)
  const name = '@scoped/npm-access-restricted-pkg'
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        restricted: (pkg, { registry }) => {
          t.equal(pkg, name, 'should use pkg name ref')
          t.equal(
            registry,
            'https://registry.npmjs.org/',
            'should forward correct options'
          )
          return true
        },
      },
    },
    prefixDir: {
      'package.json': JSON.stringify({ name }),
    },
  })
  await npm.exec('access', ['restricted'])
})

t.test('access restricted on missing package.json', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', ['restricted']),
    /no package name passed to command and no package.json found/,
    'should throw no package.json found error'
  )
})

t.test('access restricted on invalid package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{\n',
      node_modules: {},
    },
  })
  await t.rejects(
    npm.exec('access', ['restricted']),
    { code: 'EJSONPARSE' },
    'should throw failed to parse package.json'
  )
})

t.test('access grant read-only', async t => {
  t.plan(3)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        grant: (spec, team, permissions) => {
          t.equal(spec, '@scoped/another', 'should use expected spec')
          t.equal(team, 'myorg:myteam', 'should use expected team')
          t.equal(permissions, 'read-only', 'should forward permissions')
          return true
        },
      },
    },
  })
  await npm.exec('access', [
    'grant',
    'read-only',
    'myorg:myteam',
    '@scoped/another',
  ])
})

t.test('access grant read-write', async t => {
  t.plan(3)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        grant: (spec, team, permissions) => {
          t.equal(spec, '@scoped/another', 'should use expected spec')
          t.equal(team, 'myorg:myteam', 'should use expected team')
          t.equal(permissions, 'read-write', 'should forward permissions')
          return true
        },
      },
    },
  })
  await npm.exec('access', [
    'grant',
    'read-write',
    'myorg:myteam',
    '@scoped/another',
  ])
})

t.test('access grant current cwd', async t => {
  t.plan(3)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        grant: (spec, team, permissions) => {
          t.equal(spec, 'yargs', 'should use expected spec')
          t.equal(team, 'myorg:myteam', 'should use expected team')
          t.equal(permissions, 'read-write', 'should forward permissions')
          return true
        },
      },
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'yargs',
      }),
    },
  })
  await npm.exec('access', [
    'grant',
    'read-write',
    'myorg:myteam',
  ])
})

t.test('access grant others', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'rerere',
      'myorg:myteam',
      '@scoped/another',
    ]),
    /Usage: First argument must be either `read-only` or `read-write`./,
    'should throw unrecognized argument error'
  )
})

t.test('access grant missing team args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'read-only',
      undefined,
      '@scoped/another',
    ]),
    /Usage: `<scope:team>` argument is required./,
    'should throw missing argument error'
  )
})

t.test('access grant malformed team arg', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'grant',
      'read-only',
      'foo',
      '@scoped/another',
    ]),
    /Usage: Second argument used incorrect format.\n/,
    'should throw malformed arg error'
  )
})

t.test('access 2fa-required/2fa-not-required', async t => {
  t.plan(2)
  const { npm } = await loadMockNpm(t, {
    mocks: {
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
    },
  })
  await npm.exec('access', ['2fa-required', '@scope/pkg'])
  await npm.exec('access', ['2fa-not-required', 'unscoped-pkg'])
})

t.test('access revoke', async t => {
  t.plan(2)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        revoke: (spec, team) => {
          t.equal(spec, '@scoped/another', 'should use expected spec')
          t.equal(team, 'myorg:myteam', 'should use expected team')
          return true
        },
      },
    },
  })
  await npm.exec('access', [
    'revoke',
    'myorg:myteam',
    '@scoped/another',
  ])
})

t.test('access revoke missing team args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'revoke',
      undefined,
      '@scoped/another',
    ]),
    /Usage: `<scope:team>` argument is required./,
    'should throw missing argument error'
  )
})

t.test('access revoke malformed team arg', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('access', [
      'revoke',
      'foo',
      '@scoped/another',
    ]),
    /Usage: First argument used incorrect format.\n/,
    'should throw malformed arg error'
  )
})

t.test('npm access ls-packages with no team', async t => {
  t.plan(1)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        lsPackages: (entity) => {
          t.equal(entity, 'foo', 'should use expected entity')
          return {}
        },
      },
      '../../lib/utils/get-identity.js': () => Promise.resolve('foo'),
    },
  })
  await npm.exec('access', ['ls-packages'])
})

t.test('access ls-packages on team', async t => {
  t.plan(1)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        lsPackages: (entity) => {
          t.equal(entity, 'myorg:myteam', 'should use expected entity')
          return {}
        },
      },
    },
  })
  await npm.exec('access', [
    'ls-packages',
    'myorg:myteam',
  ])
})

t.test('access ls-collaborators on current', async t => {
  t.plan(1)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        lsCollaborators: (spec) => {
          t.equal(spec, 'yargs', 'should use expected spec')
          return {}
        },
      },
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'yargs',
      }),
    },
  })
  await npm.exec('access', ['ls-collaborators'])
})

t.test('access ls-collaborators on spec', async t => {
  t.plan(1)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmaccess: {
        lsCollaborators: (spec) => {
          t.equal(spec, 'yargs', 'should use expected spec')
          return {}
        },
      },
    },
  })
  await npm.exec('access', [
    'ls-collaborators',
    'yargs',
  ])
})
