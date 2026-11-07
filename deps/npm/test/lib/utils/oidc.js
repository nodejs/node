const t = require('tap')
const { readFile } = require('node:fs/promises')
const { join } = require('node:path')
const mockGlobals = require('@npmcli/mock-globals')
const mockNpm = require('../../fixtures/mock-npm')
const tmock = require('../../fixtures/tmock')

const registry = 'https://registry.npmjs.org/'
const authTokenKey = '//registry.npmjs.org/:_authToken'
const fallbackToken = 'fallback-token'
const exchangeToken = 'exchange-token'

const setup = async t => {
  const { npm } = await mockNpm(t, {
    config: ({ globalPrefix, home, prefix }) => ({
      globalconfig: join(globalPrefix, 'etc', 'npmrc'),
      prefix,
      userconfig: join(home, '.npmrc'),
    }),
    globals: ({ globalPrefix, home, prefix }) => ({
      'process.env.NPM_CONFIG_GLOBALCONFIG': join(globalPrefix, 'etc', 'npmrc'),
      'process.env.NPM_CONFIG_PREFIX': prefix,
      'process.env.NPM_CONFIG_USERCONFIG': join(home, '.npmrc'),
    }),
    npm: ({ other }) => ({
      npmRoot: join(other, 'node'),
    }),
    otherDirs: {
      node: { npmrc: '' },
    },
  })
  mockGlobals(t, {
    'process.env.NPM_ID_TOKEN': 'identity-token',
    'process.env.ACTIONS_ID_TOKEN_REQUEST_URL': 'https://github.com/actions/id-token',
    'process.env.ACTIONS_ID_TOKEN_REQUEST_TOKEN': 'request-token',
  })
  const ci = { GITLAB: true }
  const responses = [{ token: exchangeToken }]
  const { oidc } = tmock(t, '{LIB}/utils/oidc.js', {
    'ci-info': ci,
    'npm-registry-fetch': {
      json: async (_url, opts) => {
        const response = responses.shift()
        if (response instanceof Error) {
          throw response
        }
        if (typeof response === 'function') {
          return response(opts)
        }
        return response
      },
    },
    'make-fetch-happen': async () => ({
      ok: false,
      status: 500,
      json: async () => ({}),
    }),
  })
  const run = async (opts = { ...npm.flatOptions }, registryUrl = registry) => {
    await oidc({ packageName: 'test-package', registry: registryUrl, opts, config: npm.config })
    return opts
  }
  return { npm, ci, responses, run, oidc }
}

for (const failure of [
  'exchange failure',
  'missing exchange token',
  'missing identity token',
  'unsupported CI',
  'missing GitHub permissions',
  'GitHub identity request failure',
]) {
  t.test(`restores credentials before ${failure}`, async t => {
    const { npm, ci, responses, run } = await setup(t)
    npm.config.set(authTokenKey, fallbackToken, 'user')
    const firstOpts = await run()
    t.equal(firstOpts[authTokenKey], exchangeToken)
    t.equal(npm.config.get(authTokenKey), exchangeToken)
    t.equal(npm.config.get(authTokenKey, 'cli'), exchangeToken)
    t.equal(npm.config.get(authTokenKey, 'user'), fallbackToken, 'user config is never overwritten')

    if (failure === 'exchange failure') {
      responses.push(new Error('exchange failed'))
    } else if (failure === 'missing exchange token') {
      responses.push({})
    } else if (failure === 'unsupported CI') {
      ci.GITLAB = false
    } else {
      delete process.env.NPM_ID_TOKEN
      if (failure !== 'missing identity token') {
        ci.GITLAB = false
        ci.GITHUB_ACTIONS = true
        if (failure === 'missing GitHub permissions') {
          delete process.env.ACTIONS_ID_TOKEN_REQUEST_URL
        }
      }
    }

    const nextOpts = await run()
    t.equal(nextOpts[authTokenKey], fallbackToken, 'next request uses fallback credentials')
    t.equal(firstOpts[authTokenKey], fallbackToken, 'previous options no longer contain the token')
    t.equal(npm.config.get(authTokenKey, 'cli'), undefined, 'clears the runtime override')
    t.equal(npm.config.get(authTokenKey, 'user'), fallbackToken, 'preserves user config')
    t.equal(npm.flatOptions[authTokenKey], fallbackToken, 'restores flattened config')
  })
}

for (const source of ['user', 'global', 'project', 'env', 'cli', 'none']) {
  t.test(`preserves ${source} credentials when reusing flat options`, async t => {
    const { npm, responses, run } = await setup(t)
    if (source !== 'none') {
      npm.config.set(authTokenKey, fallbackToken, source)
    }
    const opts = npm.flatOptions
    await run(opts)
    t.equal(npm.config.getCredentialsByURI(registry).token, exchangeToken)
    // Populate the flat-options cache with the temporary runtime token.
    const nextOpts = npm.flatOptions
    responses.push({})
    await run(opts)

    const expected = source === 'none' ? undefined : fallbackToken
    t.equal(opts[authTokenKey], expected)
    t.equal(npm.config.get(authTokenKey), expected)
    t.equal(npm.flatOptions[authTokenKey], expected)
    t.equal(npm.config.get(authTokenKey, 'user'), source === 'user' ? fallbackToken : undefined)
    t.not(npm.flatOptions, nextOpts, 'invalidates the cached temporary credentials')
    if (source === 'none') {
      t.notOk(Object.hasOwn(opts, authTokenKey), 'removes the temporary option entirely')
    }
  })
}

t.test('does not restore an earlier exchange token after multiple successes', async t => {
  const { npm, responses, run } = await setup(t)
  npm.config.set(authTokenKey, fallbackToken, 'user')
  await run()
  responses.push({ token: 'second-exchange-token' })
  const secondOpts = await run()
  t.equal(secondOpts[authTokenKey], 'second-exchange-token')
  responses.push({})
  const thirdOpts = await run()
  t.equal(thirdOpts[authTokenKey], fallbackToken)
  t.equal(npm.config.get(authTokenKey), fallbackToken)
})

t.test('clears the previous token before starting another exchange', async t => {
  const { npm, responses, run } = await setup(t)
  npm.config.set(authTokenKey, fallbackToken, 'user')
  const firstOpts = await run()
  responses.push(opts => {
    t.equal(npm.config.get(authTokenKey), fallbackToken, 'old token is already cleared')
    t.equal(firstOpts[authTokenKey], fallbackToken, 'old request options are already cleared')
    t.equal(opts[authTokenKey], 'identity-token', 'exchange uses the identity token')
    return { token: 'new-exchange-token' }
  })
  const nextOpts = await run()
  t.equal(nextOpts[authTokenKey], 'new-exchange-token')
  t.equal(npm.config.get(authTokenKey), 'new-exchange-token')
})

t.test('clears the previous registry token when switching registries', async t => {
  const { npm, responses, run } = await setup(t)
  const otherRegistry = 'https://other.registry.npmjs.org/'
  const otherKey = '//other.registry.npmjs.org/:_authToken'
  npm.config.set(authTokenKey, fallbackToken, 'user')
  npm.config.set(otherKey, 'other-fallback', 'user')
  await run()
  responses.push({ token: 'other-exchange-token' })
  const otherOpts = await run(undefined, otherRegistry)
  t.equal(otherOpts[authTokenKey], fallbackToken)
  t.equal(otherOpts[otherKey], 'other-exchange-token')
  responses.push({})
  const nextOpts = await run()
  t.equal(nextOpts[authTokenKey], fallbackToken)
  t.equal(nextOpts[otherKey], 'other-fallback')
  t.equal(npm.config.get(otherKey), 'other-fallback')
})

t.test('preserves credentials explicitly replaced after an exchange', async t => {
  const { npm, responses, run } = await setup(t)
  const opts = await run()
  npm.config.set(authTokenKey, 'updated-user-token', 'user')
  opts[authTokenKey] = 'updated-request-token'
  responses.push({})
  await run(opts)
  t.equal(npm.config.get(authTokenKey), 'updated-user-token')
  t.equal(opts[authTokenKey], 'updated-request-token')
})

t.test('preserves a runtime credential replaced independently of OIDC', async t => {
  const { npm, responses, run } = await setup(t)
  await run()
  npm.config.set(authTokenKey, 'updated-runtime-token', 'cli')
  responses.push({})
  const opts = await run()
  t.equal(npm.config.get(authTokenKey, 'cli'), 'updated-runtime-token')
  t.equal(opts[authTokenKey], 'updated-runtime-token')
})

t.test('saving config during an exchange never persists the OIDC token', async t => {
  const { npm, run } = await setup(t)
  const sources = ['user', 'project', 'global']
  for (const source of sources) {
    npm.config.set(authTokenKey, `${source}-fallback`, source)
  }
  await run()
  t.equal(npm.config.get(authTokenKey, 'cli'), exchangeToken)
  await t.rejects(npm.config.save('cli'), { message: 'invalid config location param: cli' })
  for (const source of sources) {
    t.equal(npm.config.get(authTokenKey, source), `${source}-fallback`)
    await npm.config.save(source)
    const content = await readFile(npm.config.data.get(source).source, 'utf8')
    t.match(content, `${authTokenKey}=${source}-fallback`)
    t.notMatch(content, exchangeToken)
  }
})

t.test('the next invocation clears the token even with a different config', async t => {
  const { npm, ci, run, oidc } = await setup(t)
  npm.config.set(authTokenKey, fallbackToken, 'user')
  const opts = await run()
  ci.GITLAB = false
  await oidc({ packageName: 'other-package', registry, opts: {}, config: {} })
  t.equal(npm.config.get(authTokenKey, 'cli'), undefined)
  t.equal(npm.config.get(authTokenKey), fallbackToken)
  t.equal(opts[authTokenKey], fallbackToken)
})

t.test('does not copy package-specific fallback options into the next package', async t => {
  const { npm, responses, run } = await setup(t)
  npm.config.set(authTokenKey, fallbackToken, 'user')
  const firstOpts = await run({ ...npm.flatOptions, [authTokenKey]: 'package-specific-token' })
  responses.push({})
  const nextOpts = await run()
  t.equal(firstOpts[authTokenKey], 'package-specific-token')
  t.equal(nextOpts[authTokenKey], fallbackToken)
})
