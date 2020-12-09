const { test } = require('tap')
const requireInject = require('require-inject')

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
    },
  }
}

const deprecate = requireInject('../../lib/deprecate.js', {
  '../../lib/npm.js': {
    flatOptions: { registry: 'https://registry.npmjs.org' },
  },
  '../../lib/utils/get-identity.js': async () => getIdentityImpl(),
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  libnpmaccess: {
    lsPackages: async () => ({ foo: 'write', bar: 'write', baz: 'write', buzz: 'read' }),
  },
  'npm-registry-fetch': npmFetch,
})

test('completion', async t => {
  const defaultIdentityImpl = getIdentityImpl
  t.teardown(() => {
    getIdentityImpl = defaultIdentityImpl
  })

  const { completion } = deprecate

  const testComp = (argv, expect) => {
    return new Promise((resolve, reject) => {
      completion({ conf: { argv: { remain: argv } } }, (err, res) => {
        if (err)
          return reject(err)

        t.strictSame(res, expect, `completion: ${argv}`)
        resolve()
      })
    })
  }

  await testComp([], ['foo', 'bar', 'baz'])
  await testComp(['b'], ['bar', 'baz'])
  await testComp(['fo'], ['foo'])
  await testComp(['g'], [])
  await testComp(['foo', 'something'], [])

  getIdentityImpl = () => {
    throw new Error('unknown failure')
  }

  t.rejects(testComp([], []), /unknown failure/)
})

test('no args', t => {
  deprecate([], (err) => {
    t.match(err, /Usage: npm deprecate/, 'logs usage')
    t.end()
  })
})

test('only one arg', t => {
  deprecate(['foo'], (err) => {
    t.match(err, /Usage: npm deprecate/, 'logs usage')
    t.end()
  })
})

test('invalid semver range', t => {
  deprecate(['foo@notaversion', 'this will fail'], (err) => {
    t.match(err, /invalid version range/, 'logs semver error')
    t.end()
  })
})

test('deprecates given range', t => {
  t.teardown(() => {
    npmFetchBody = null
  })

  deprecate(['foo@1.0.0', 'this version is deprecated'], (err) => {
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

test('deprecates all versions when no range is specified', t => {
  t.teardown(() => {
    npmFetchBody = null
  })

  deprecate(['foo', 'this version is deprecated'], (err) => {
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
      },
    })

    t.end()
  })
})
