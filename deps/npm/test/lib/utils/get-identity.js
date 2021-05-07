const t = require('tap')

t.test('throws ENOREGISTRY when no registry option is provided', async (t) => {
  t.plan(2)
  const getIdentity = t.mock('../../../lib/utils/get-identity.js')

  try {
    await getIdentity({})
  } catch (err) {
    t.equal(err.code, 'ENOREGISTRY', 'assigns the appropriate error code')
    t.equal(err.message, 'No registry specified.', 'returns the correct error message')
  }
})

t.test('returns username from uri when provided', async (t) => {
  t.plan(1)

  const getIdentity = t.mock('../../../lib/utils/get-identity.js')
  const npm = {
    config: {
      getCredentialsByURI: () => {
        return { username: 'foo' }
      },
    },
  }

  const identity = await getIdentity(npm, { registry: 'https://registry.npmjs.org' })
  t.equal(identity, 'foo', 'returns username from uri')
})

t.test('calls registry whoami when token is provided', async (t) => {
  t.plan(3)

  const options = {
    registry: 'https://registry.npmjs.org',
    token: 'thisisnotreallyatoken',
  }

  const getIdentity = t.mock('../../../lib/utils/get-identity.js', {
    'npm-registry-fetch': {
      json: (path, opts) => {
        t.equal(path, '/-/whoami', 'calls whoami')
        t.same(opts, options, 'passes through provided options')
        return { username: 'foo' }
      },
    },
  })
  const npm = {
    config: {
      getCredentialsByURI: () => options,
    },
  }

  const identity = await getIdentity(npm, options)
  t.equal(identity, 'foo', 'fetched username from registry')
})

t.test('throws ENEEDAUTH when response does not include a username', async (t) => {
  t.plan(3)

  const options = {
    registry: 'https://registry.npmjs.org',
    token: 'thisisnotreallyatoken',
  }

  const getIdentity = t.mock('../../../lib/utils/get-identity.js', {
    'npm-registry-fetch': {
      json: (path, opts) => {
        t.equal(path, '/-/whoami', 'calls whoami')
        t.same(opts, options, 'passes through provided options')
        return {}
      },
    },
  })
  const npm = {
    config: {
      getCredentialsByURI: () => options,
    },
  }

  try {
    await getIdentity(npm, options)
  } catch (err) {
    t.equal(err.code, 'ENEEDAUTH', 'throws correct error code')
  }
})

t.test('throws ENEEDAUTH when neither username nor token is configured', async (t) => {
  t.plan(1)
  const getIdentity = t.mock('../../../lib/utils/get-identity.js', {
  })
  const npm = {
    config: {
      getCredentialsByURI: () => ({}),
    },
  }

  try {
    await getIdentity(npm, { registry: 'https://registry.npmjs.org' })
  } catch (err) {
    t.equal(err.code, 'ENEEDAUTH', 'throws correct error code')
  }
})
