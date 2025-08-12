const ciInfo = require('ci-info')
const nock = require('nock')
const mockGlobals = require('@npmcli/mock-globals')
const { loadNpmWithRegistry } = require('./mock-npm')
const { mockProvenance } = require('@npmcli/mock-registry/lib/provenance')

// this is an effort to not add a dependency to the cli just for testing
function makeJwt (payload) {
  const header = { alg: 'none', typ: 'JWT' }
  const headerB64 = Buffer.from(JSON.stringify(header)).toString('base64')
  const payloadB64 = Buffer.from(JSON.stringify(payload)).toString('base64')
  // empty signature section
  return `${headerB64}.${payloadB64}.`
}

function gitlabIdToken ({ visibility = 'public' } = { visibility: 'public' }) {
  const now = Math.floor(Date.now() / 1000)
  const payload = {
    project_visibility: visibility,
    iat: now,
    exp: now + 3600, // 1 hour expiration
  }
  return makeJwt(payload)
}

function githubIdToken ({ visibility = 'public' } = { visibility: 'public' }) {
  const now = Math.floor(Date.now() / 1000)
  const payload = {
    repository_visibility: visibility,
    iat: now,
    exp: now + 3600, // 1 hour expiration
  }
  return makeJwt(payload)
}

const mockOidc = async (t, {
  oidcOptions = {},
  packageName = '@npmcli/test-package',
  config = {},
  packageJson = {},
  load = {},
  mockGithubOidcOptions = null,
  mockOidcTokenExchangeOptions = null,
  publishOptions = {},
  provenance = false,
}) => {
  const github = oidcOptions.github ?? false
  const gitlab = oidcOptions.gitlab ?? false

  const ACTIONS_ID_TOKEN_REQUEST_URL = oidcOptions.ACTIONS_ID_TOKEN_REQUEST_URL ?? 'https://github.com/actions/id-token'
  const ACTIONS_ID_TOKEN_REQUEST_TOKEN = oidcOptions.ACTIONS_ID_TOKEN_REQUEST_TOKEN ?? 'ACTIONS_ID_TOKEN_REQUEST_TOKEN'

  mockGlobals(t, {
    process: {
      env: {
        ACTIONS_ID_TOKEN_REQUEST_TOKEN: ACTIONS_ID_TOKEN_REQUEST_TOKEN,
        ACTIONS_ID_TOKEN_REQUEST_URL: ACTIONS_ID_TOKEN_REQUEST_URL,
        CI: github || gitlab ? 'true' : undefined,
        ...(github ? { GITHUB_ACTIONS: 'true' } : {}),
        ...(gitlab ? { GITLAB_CI: 'true' } : {}),
        ...(oidcOptions.NPM_ID_TOKEN ? { NPM_ID_TOKEN: oidcOptions.NPM_ID_TOKEN } : {}),
        /* eslint-disable-next-line max-len */
        ...(oidcOptions.SIGSTORE_ID_TOKEN ? { SIGSTORE_ID_TOKEN: oidcOptions.SIGSTORE_ID_TOKEN } : {}),
      },
    },
  })

  const GITHUB_ACTIONS = ciInfo.GITHUB_ACTIONS
  const GITLAB = ciInfo.GITLAB
  delete ciInfo.GITHUB_ACTIONS
  delete ciInfo.GITLAB
  if (github) {
    ciInfo.GITHUB_ACTIONS = 'true'
  }
  if (gitlab) {
    ciInfo.GITLAB = 'true'
  }
  t.teardown(() => {
    ciInfo.GITHUB_ACTIONS = GITHUB_ACTIONS
    ciInfo.GITLAB = GITLAB
  })

  const { npm, registry, joinedOutput, logs } = await loadNpmWithRegistry(t, {
    config: {
      loglevel: 'silly',
      ...config,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: packageName,
        version: '1.0.0',
        ...packageJson,
      }, null, 2),
    },
    ...load,
  })

  if (mockGithubOidcOptions) {
    const { idToken, audience, statusCode = 200 } = mockGithubOidcOptions
    const url = new URL(ACTIONS_ID_TOKEN_REQUEST_URL)
    nock(url.origin)
      .get(url.pathname)
      .query({ audience })
      .matchHeader('authorization', `Bearer ${ACTIONS_ID_TOKEN_REQUEST_TOKEN}`)
      .matchHeader('accept', 'application/json')
      .reply(statusCode, statusCode !== 500 ? { value: idToken } : { message: 'Internal Server Error' })
  }

  if (mockOidcTokenExchangeOptions) {
    registry.mockOidcTokenExchange({
      packageName,
      ...mockOidcTokenExchangeOptions,
    })
  }

  registry.publish(packageName, publishOptions)

  if ((github || gitlab) && provenance) {
    registry.getVisibility({ spec: packageName, visibility: { public: true } })
    mockProvenance(t, {
      oidcURL: ACTIONS_ID_TOKEN_REQUEST_URL,
      requestToken: ACTIONS_ID_TOKEN_REQUEST_TOKEN,
      workflowPath: '.github/workflows/publish.yml',
      repository: 'github/foo',
      serverUrl: 'https://github.com',
      ref: 'refs/tags/pkg@1.0.0',
      sha: 'deadbeef',
      runID: '123456',
      runAttempt: '1',
      runnerEnv: 'github-hosted',
    })
  }

  return { npm, joinedOutput, logs, ACTIONS_ID_TOKEN_REQUEST_URL }
}

const oidcPublishTest = (opts) => {
  return async (t) => {
    const { logsContain } = opts
    const { npm, joinedOutput, logs } = await mockOidc(t, opts)
    await npm.exec('publish', [])
    logsContain?.forEach(item => {
      t.ok(logs.includes(item), `Expected log to include: ${item}`)
    })
    t.match(joinedOutput(), '+ @npmcli/test-package@1.0.0')
  }
}

module.exports = {
  gitlabIdToken,
  githubIdToken,
  mockOidc,
  oidcPublishTest,
}
