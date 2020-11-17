const { test } = require('tap')
const requireInject = require('require-inject')

test('throws ENOREGISTRY when no registry option is provided', async (t) => {
  t.plan(2)
  const getIdentity = requireInject('../../../lib/utils/get-identity.js', {
    '../../../lib/npm.js': {},
  })

  try {
    await getIdentity()
  } catch (err) {
    t.equal(err.code, 'ENOREGISTRY', 'assigns the appropriate error code')
    t.equal(err.message, 'No registry specified.', 'returns the correct error message')
  }
})

test('returns username from uri when provided', async (t) => {
  t.plan(1)

  const getIdentity = requireInject('../../../lib/utils/get-identity.js', {
    '../../../lib/npm.js': {
      config: {
        getCredentialsByURI: () => {
          return { username: 'foo' }
        },
      },
    },
  })

  const identity = await getIdentity({ registry: 'https://registry.npmjs.org' })
  t.equal(identity, 'foo', 'returns username from uri')
})

test('calls registry whoami when token is provided', async (t) => {
  t.plan(3)

  const options = {
    registry: 'https://registry.npmjs.org',
    token: 'thisisnotreallyatoken',
  }

  const getIdentity = requireInject('../../../lib/utils/get-identity.js', {
    '../../../lib/npm.js': {
      config: {
        getCredentialsByURI: () => options,
      },
    },
    'npm-registry-fetch': {
      json: (path, opts) => {
        t.equal(path, '/-/whoami', 'calls whoami')
        t.same(opts, options, 'passes through provided options')
        return { username: 'foo' }
      },
    },
  })

  const identity = await getIdentity(options)
  t.equal(identity, 'foo', 'fetched username from registry')
})

test('throws ENEEDAUTH when response does not include a username', async (t) => {
  t.plan(3)

  const options = {
    registry: 'https://registry.npmjs.org',
    token: 'thisisnotreallyatoken',
  }

  const getIdentity = requireInject('../../../lib/utils/get-identity.js', {
    '../../../lib/npm.js': {
      config: {
        getCredentialsByURI: () => options,
      },
    },
    'npm-registry-fetch': {
      json: (path, opts) => {
        t.equal(path, '/-/whoami', 'calls whoami')
        t.same(opts, options, 'passes through provided options')
        return {}
      },
    },
  })

  try {
    await getIdentity(options)
  } catch (err) {
    t.equal(err.code, 'ENEEDAUTH', 'throws correct error code')
  }
})

test('throws ENEEDAUTH when neither username nor token is configured', async (t) => {
  t.plan(1)
  const getIdentity = requireInject('../../../lib/utils/get-identity.js', {
    '../../../lib/npm.js': {
      config: {
        getCredentialsByURI: () => ({}),
      },
    },
  })

  try {
    await getIdentity({ registry: 'https://registry.npmjs.org' })
  } catch (err) {
    t.equal(err.code, 'ENEEDAUTH', 'throws correct error code')
  }
})
