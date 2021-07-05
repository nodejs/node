const t = require('tap')

let getIdentityImpl = () => 'someperson'
let npmFetchBody = null

const npmFetch = async (uri, opts) => {
  npmFetchBody = opts.body
}

npmFetch.json = async (uri, opts) => {
  return {
    versions: {
      '1.0.0': {},
      '1.0.1': {},
      '1.0.1-pre': {},
    },
  }
}

const Deprecate = t.mock('../../lib/deprecate.js', {
  '../../lib/utils/get-identity.js': async () => getIdentityImpl(),
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  libnpmaccess: {
    lsPackages: async () => ({ foo: 'write', bar: 'write', baz: 'write', buzz: 'read' }),
  },
  'npm-registry-fetch': npmFetch,
})

const deprecate = new Deprecate({
  flatOptions: { registry: 'https://registry.npmjs.org' },
})

t.test('completion', async t => {
  const defaultIdentityImpl = getIdentityImpl
  t.teardown(() => {
    getIdentityImpl = defaultIdentityImpl
  })

  const testComp = async (argv, expect) => {
    const res =
      await deprecate.completion({ conf: { argv: { remain: argv } } })
    t.strictSame(res, expect, `completion: ${argv}`)
  }

  await Promise.all([
    testComp([], ['foo', 'bar', 'baz']),
    testComp(['b'], ['bar', 'baz']),
    testComp(['fo'], ['foo']),
    testComp(['g'], []),
    testComp(['foo', 'something'], []),
  ])

  getIdentityImpl = () => {
    throw new Error('deprecate test failure')
  }

  t.rejects(testComp([], []), { message: 'deprecate test failure' })
})

t.test('no args', t => {
  deprecate.exec([], (err) => {
    t.match(err, 'Usage:', 'logs usage')
    t.end()
  })
})

t.test('only one arg', t => {
  deprecate.exec(['foo'], (err) => {
    t.match(err, 'Usage:', 'logs usage')
    t.end()
  })
})

t.test('invalid semver range', t => {
  deprecate.exec(['foo@notaversion', 'this will fail'], (err) => {
    t.match(err, /invalid version range/, 'logs semver error')
    t.end()
  })
})

t.test('undeprecate', t => {
  deprecate.exec(['foo', ''], (err) => {
    if (err)
      throw err
    t.match(npmFetchBody, {
      versions: {
        '1.0.0': { deprecated: '' },
        '1.0.1': { deprecated: '' },
        '1.0.1-pre': { deprecated: '' },
      },
    }, 'undeprecates everything')
    t.end()
  })
})

t.test('deprecates given range', t => {
  t.teardown(() => {
    npmFetchBody = null
  })

  deprecate.exec(['foo@1.0.0', 'this version is deprecated'], (err) => {
    if (err)
      throw err

    t.match(npmFetchBody, {
      versions: {
        '1.0.0': {
          deprecated: 'this version is deprecated',
        },
        '1.0.1': {
          // the undefined here is necessary to ensure that we absolutely
          // did not assign this property
          deprecated: undefined,
        },
      },
    })

    t.end()
  })
})

t.test('deprecates all versions when no range is specified', t => {
  t.teardown(() => {
    npmFetchBody = null
  })

  deprecate.exec(['foo', 'this version is deprecated'], (err) => {
    if (err)
      throw err

    t.match(npmFetchBody, {
      versions: {
        '1.0.0': {
          deprecated: 'this version is deprecated',
        },
        '1.0.1': {
          deprecated: 'this version is deprecated',
        },
        '1.0.1-pre': {
          deprecated: 'this version is deprecated',
        },
      },
    })

    t.end()
  })
})
