const t = require('tap')
const realFetch = require('npm-registry-fetch')
const npa = require('npm-package-arg')
const mockNpm = require('../../fixtures/mock-npm')
const {
  circleciIdToken,
  githubIdToken,
  gitlabIdToken,
  mockOidc,
} = require('../../fixtures/mock-oidc')

const fixtures = {
  workspace: {
    'package.json': JSON.stringify({
      name: 'root',
      version: '1.0.0',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.0.0',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.0.0',
      }),
    },
    'workspace-c': {
      'package.json': JSON.stringify({
        name: 'workspace-c',
        version: '1.0.0',
      }),
    },
  },
}

const tags = {
  '/-/package/@scoped%2fpkg/dist-tags': {
    latest: '1.0.0',
    a: '0.0.1',
    b: '0.5.0',
  },
  '/-/package/@scoped%2fanother/dist-tags': {
    latest: '2.0.0',
    a: '0.0.2',
    b: '0.6.0',
  },
  '/-/package/@scoped%2fanother/dist-tags/c': {
    latest: '7.7.7',
    a: '0.0.2',
    b: '0.6.0',
    c: '7.7.7',
  },
  '/-/package/workspace-a/dist-tags': {
    latest: '1.0.0',
    'latest-a': '1.0.0',
  },
  '/-/package/workspace-b/dist-tags': {
    latest: '2.0.0',
    'latest-b': '2.0.0',
  },
  '/-/package/workspace-c/dist-tags': {
    latest: '3.0.0',
    'latest-c': '3.0.0',
  },
}

const mockTagRequests = ({
  registry,
  packageName,
  token,
  tags: tagData,
  mutation,
}) => {
  const spec = npa(packageName)
  const path = `/-/package/${spec.escapedName}/dist-tags`
  const authorization = `Bearer ${token}`

  registry.nock.get(registry.fullPath(path))
    .matchHeader('authorization', authorization)
    .reply(200, tagData)

  if (mutation) {
    registry.nock[mutation.method.toLowerCase()](
      registry.fullPath(`${path}/${encodeURIComponent(mutation.tag)}`)
    )
      .matchHeader('authorization', authorization)
      .reply(200, {})
  }
}

const mockDist = async (t, { ...npmOpts } = {}) => {
  const getTag = async (url) => ({ ...tags })[url]

  let fetchOpts
  const nrf = async (url, opts) => {
    fetchOpts = opts

    if (url === '/-/package/foo/dist-tags') {
      throw new Error('no package found')
    }

    return getTag(url)
  }

  const mock = await mockNpm(t, {
    ...npmOpts,
    command: 'dist-tag',
    mocks: {
      'npm-registry-fetch': Object.assign(nrf, realFetch, { json: getTag }),
    },
  })

  return {
    ...mock,
    distTag: mock['dist-tag'],
    fetchOpts: () => fetchOpts,
    result: () => mock.joinedOutput(),
    joinedLogs: () => mock.logs.byTitle('dist-tag').join('\n').trim(),
  }
}

t.test('ls in current package', async t => {
  const { distTag, result } = await mockDist(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@scoped/pkg',
      }),
    },
  })
  await distTag.exec(['ls'])
  t.matchSnapshot(
    result(),
    'should list available tags for current package'
  )
})

t.test('ls global', async t => {
  const { distTag } = await mockDist(t, {
    config: {
      global: true,
    },
  })
  await t.rejects(
    distTag.exec(['ls']),
    distTag.usage,
    'should throw basic usage'
  )
})

t.test('no args in current package', async t => {
  const { distTag, result } = await mockDist(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@scoped/pkg',
      }),
    },
  })
  await distTag.exec([])
  t.matchSnapshot(
    result(),
    'should default to listing available tags for current package'
  )
})

t.test('borked cmd usage', async t => {
  const { distTag } = await mockDist(t)
  await t.rejects(
    distTag.exec(['borked', '@scoped/pkg']),
    distTag.usage,
    'should show usage error'
  )
})

t.test('ls on named package', async t => {
  const { distTag, result } = await mockDist(t)
  await distTag.exec(['ls', '@scoped/another'])
  t.matchSnapshot(
    result(),
    'should list tags for the specified package'
  )
})

t.test('ls on missing package', async t => {
  const { distTag, joinedLogs } = await mockDist(t)
  await t.rejects(
    distTag.exec(['ls', 'foo']),
    distTag.usage
  )
  t.matchSnapshot(
    joinedLogs(),
    'should log no dist-tag found msg'
  )
})

t.test('ls on missing name in current package', async t => {
  const { distTag } = await mockDist(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        version: '1.0.0',
      }),
    },
  })
  await t.rejects(
    distTag.exec(['ls']),
    distTag.usage,
    'should throw usage error message'
  )
})

t.test('only named package arg', async t => {
  const { distTag, result } = await mockDist(t)
  await distTag.exec(['@scoped/another'])
  t.matchSnapshot(
    result(),
    'should default to listing tags for the specified package'
  )
})

t.test('workspaces', async t => {
  const mockWorkspaces = async (t, exec = [], workspaces = true, prefixDir = {}) => {
    const mock = await mockDist(t, {
      prefixDir: {
        ...fixtures.workspace,
        ...prefixDir,
      },
      config: workspaces === true ? { workspaces } : { workspace: workspaces },
    })

    await mock.distTag.exec(exec)

    return mock
  }

  t.test('no args', async t => {
    const { result } = await mockWorkspaces(t)
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('no args, one workspace', async t => {
    const { result } = await mockWorkspaces(t, [], 'workspace-a')
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('one arg -- cwd', async t => {
    const { result } = await mockWorkspaces(t, ['.'])
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('one arg -- .@1, ignores version spec', async t => {
    const { result } = await mockWorkspaces(t, ['.@'])
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('one arg -- list', async t => {
    const { result } = await mockWorkspaces(t, ['list'])
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('two args -- list, cwd', async t => {
    const { result } = await mockWorkspaces(t, ['list', '.'])
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('two args -- list, .@1, ignores version spec', async t => {
    const { result } = await mockWorkspaces(t, ['list', '.@'])
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('two args -- list, @scoped/pkg, logs a warning and ignores workspaces', async t => {
    const { result, joinedLogs } = await mockWorkspaces(t, ['list', '@scoped/pkg'])
    t.match(joinedLogs(), 'Ignoring workspaces for specified package', 'logs a warning')
    t.matchSnapshot(result(), 'printed the expected output')
  })

  t.test('no args, one failing workspace sets exitCode to 1', async t => {
    const { result, logs } = await mockWorkspaces(t, [], true, {
      'package.json': JSON.stringify({
        name: 'root',
        version: '1.0.0',
        workspaces: ['workspace-a', 'workspace-b', 'workspace-c', 'workspace-d'],
      }),

      'workspace-d': {
        'package.json': JSON.stringify({
          name: 'workspace-d',
          version: '1.0.0',
        }),
      },
    })

    const error = logs.error.byTitle('dist-tag ls')[0]

    t.match(error, 'Couldn\'t get dist-tag data for Result {')
    t.match(error, `name: 'workspace-d',`)
    t.matchSnapshot(result(), 'printed the expected output')
  })
})

t.test('add new tag', async t => {
  const { distTag, result, fetchOpts } = await mockDist(t)
  await distTag.exec(['add', '@scoped/another@7.7.7', 'c'])
  const opts = fetchOpts()
  t.equal(opts.method, 'PUT', 'should trigger request to add new tag')
  t.equal(opts.body, '"7.7.7"', 'should point to expected version')
  t.matchSnapshot(
    result(),
    'should return success msg'
  )
})

t.test('add using valid semver range as name', async t => {
  const { distTag, joinedLogs } = await mockDist(t)
  await t.rejects(
    distTag.exec(['add', '@scoped/another@7.7.7', '1.0.0']),
    /Tag name must not be a valid SemVer range: 1.0.0/,
    'should exit with semver range error'
  )
  t.matchSnapshot(
    joinedLogs(),
    'should return success msg'
  )
})

t.test('add missing args', async t => {
  const { distTag } = await mockDist(t, {
    config: {
      tag: '',
    },
  })
  await t.rejects(
    distTag.exec(['add', '@scoped/another@7.7.7']),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('add missing pkg name', async t => {
  const { distTag } = await mockDist(t)
  await t.rejects(
    distTag.exec(['add', null]),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('add invalid tag', async t => {
  const { distTag } = await mockDist(t)
  await t.rejects(
    distTag.exec(['add', '@tag']),
    { code: 'EINVALIDTAGNAME' },
    'should exit with invalid tag name error'
  )
})

t.test('set existing version', async t => {
  const { distTag, joinedLogs } = await mockDist(t)
  await distTag.exec(['set', '@scoped/another@0.6.0', 'b'])
  t.matchSnapshot(
    joinedLogs(),
    'should log warn msg'
  )
})

t.test('remove existing tag', async t => {
  const { distTag, result, joinedLogs, fetchOpts } = await mockDist(t)
  await distTag.exec(['rm', '@scoped/another', 'c'])
  const opts = fetchOpts()
  t.equal(opts.method, 'DELETE', 'should trigger request to remove tag')
  t.matchSnapshot(joinedLogs(), 'should log remove info')
  t.matchSnapshot(result(), 'should return success msg')
})

t.test('remove non-existing tag', async t => {
  const { distTag, joinedLogs } = await mockDist(t)
  await t.rejects(
    distTag.exec(['rm', '@scoped/another', 'nonexistent']),
    /nonexistent is not a dist-tag on @scoped\/another/,
    'should exit with error'
  )
  t.matchSnapshot(joinedLogs(), 'should log error msg')
})

t.test('remove missing pkg name', async t => {
  const { distTag } = await mockDist(t)
  await t.rejects(
    distTag.exec(['rm', null]),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('oidc token exchange', async t => {
  const packageName = '@npmcli/test-package'
  const fallbackToken = 'existing-fallback-token'
  const exchangeToken = 'exchange-token'
  const privateGithubToken = githubIdToken({ visibility: 'private' })

  const setup = (t, options = {}) => mockOidc(t, {
    packageName,
    publish: false,
    ...options,
  })

  t.test('list uses the exchanged token', async t => {
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: { token: exchangeToken },
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0' },
    })

    await npm.exec('dist-tag', ['list', packageName])
  })

  t.test('add uses the exchanged token for GET and PUT', async t => {
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: { token: exchangeToken },
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0' },
      mutation: { method: 'PUT', tag: 'next' },
    })

    await npm.exec('dist-tag', ['add', `${packageName}@2.0.0`, 'next'])
  })

  t.test('remove uses the exchanged token for GET and DELETE', async t => {
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: { token: exchangeToken },
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0', old: '0.9.0' },
      mutation: { method: 'DELETE', tag: 'old' },
    })

    await npm.exec('dist-tag', ['remove', packageName, 'old'])
  })

  for (const [provider, oidcOptions, idToken] of [
    ['gitlab', { gitlab: true, NPM_ID_TOKEN: gitlabIdToken({ visibility: 'private' }) }, gitlabIdToken({ visibility: 'private' })],
    ['circleci', { circleci: true, NPM_ID_TOKEN: circleciIdToken() }, circleciIdToken()],
  ]) {
    t.test(`${provider} uses NPM_ID_TOKEN for exchange`, async t => {
      const providerIdToken = oidcOptions.NPM_ID_TOKEN || idToken
      const { npm, registry } = await setup(t, {
        oidcOptions,
        mockOidcTokenExchangeOptions: {
          idToken: providerIdToken,
          body: { token: exchangeToken },
        },
      })
      mockTagRequests({
        registry,
        packageName,
        token: exchangeToken,
        tags: { latest: '1.0.0' },
      })

      await npm.exec('dist-tag', ['list', packageName])
    })
  }

  t.test('custom scoped registry uses its audience and auth key', async t => {
    const registryUrl = 'https://registry.zzz.org'
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      config: {
        '@npmcli:registry': registryUrl,
      },
      mockGithubOidcOptions: {
        audience: 'npm:registry.zzz.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: { token: exchangeToken },
      },
      load: {
        registry: registryUrl,
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0' },
    })

    await npm.exec('dist-tag', ['list', packageName])
  })

  for (const testCase of [
    {
      name: 'no CI',
      oidcOptions: {},
    },
    {
      name: 'missing GitHub OIDC permissions',
      oidcOptions: { github: true, ACTIONS_ID_TOKEN_REQUEST_URL: '' },
    },
    {
      name: 'GitHub identity token request failure',
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        statusCode: 500,
      },
    },
    {
      name: 'token exchange failure',
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        statusCode: 500,
        body: { message: 'exchange failed' },
      },
    },
    {
      name: 'token exchange response missing token',
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: {},
      },
    },
  ]) {
    t.test(`${testCase.name} preserves fallback credentials`, async t => {
      const { npm, registry } = await setup(t, {
        config: {
          '//registry.npmjs.org/:_authToken': fallbackToken,
        },
        ...testCase,
      })
      mockTagRequests({
        registry,
        packageName,
        token: fallbackToken,
        tags: { latest: '1.0.0' },
      })

      await npm.exec('dist-tag', ['list', packageName])
    })
  }

  t.test('current package list exchanges after resolving its name', async t => {
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: privateGithubToken,
        body: { token: exchangeToken },
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0' },
    })

    await npm.exec('dist-tag', ['list'])
  })

  t.test('workspace list exchanges separately for each package', async t => {
    const workspaceToken = githubIdToken({ visibility: 'private' })
    const prefixDir = {
      'package.json': JSON.stringify({
        name: 'workspace-root',
        version: '1.0.0',
        workspaces: ['workspace-a', 'workspace-b'],
      }),
      'workspace-a': {
        'package.json': JSON.stringify({
          name: 'workspace-a',
          version: '1.0.0',
        }),
      },
      'workspace-b': {
        'package.json': JSON.stringify({
          name: 'workspace-b',
          version: '1.0.0',
        }),
      },
    }
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      config: { workspaces: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: workspaceToken,
        times: 2,
      },
      load: { prefixDir },
    })

    for (const [name, token] of [
      ['workspace-a', 'workspace-a-exchange-token'],
      ['workspace-b', 'workspace-b-exchange-token'],
    ]) {
      registry.mockOidcTokenExchange({
        packageName: name,
        idToken: workspaceToken,
        body: { token },
      })
      mockTagRequests({
        registry,
        packageName: name,
        token,
        tags: { latest: '1.0.0' },
      })
    }

    await npm.exec('dist-tag', [])
  })

  t.test('workspace list restores fallback credentials after a failed exchange', async t => {
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      config: { workspaces: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: privateGithubToken,
        times: 3,
      },
      load: {
        prefixDir: fixtures.workspace,
        homeDir: {
          '.npmrc': `//registry.npmjs.org/:_authToken=${fallbackToken}`,
        },
      },
    })
    for (const [name, token] of [
      ['workspace-a', exchangeToken],
      ['workspace-b', fallbackToken],
      ['workspace-c', 'workspace-c-exchange-token'],
    ]) {
      const failed = name === 'workspace-b'
      registry.mockOidcTokenExchange({
        packageName: name,
        idToken: privateGithubToken,
        statusCode: failed ? 404 : 200,
        body: failed ? { message: 'No trusted publisher configured' } : { token },
      })
      mockTagRequests({
        registry,
        packageName: name,
        token,
        tags: { latest: '1.0.0' },
      })
    }

    await npm.exec('dist-tag', [])
  })

  t.test('does not run publish provenance checks', async t => {
    const publicGithubToken = githubIdToken({ visibility: 'public' })
    let visibilityCalls = 0
    const { npm, registry } = await setup(t, {
      oidcOptions: { github: true },
      mockGithubOidcOptions: {
        audience: 'npm:registry.npmjs.org',
        idToken: publicGithubToken,
      },
      mockOidcTokenExchangeOptions: {
        idToken: publicGithubToken,
        body: { token: exchangeToken },
      },
      load: {
        mocks: {
          libnpmaccess: {
            getVisibility: () => {
              visibilityCalls++
              return { public: true }
            },
          },
        },
      },
    })
    mockTagRequests({
      registry,
      packageName,
      token: exchangeToken,
      tags: { latest: '1.0.0' },
    })

    await npm.exec('dist-tag', ['list', packageName])
    t.equal(visibilityCalls, 0, 'must not check package visibility')
  })

  t.end()
})

t.test('completion', async t => {
  const { distTag } = await mockDist(t)

  const match = distTag.completion({ conf: { argv: { remain: ['npm', 'dist-tag'] } } })
  t.resolveMatch(match, ['add', 'rm', 'ls'],
    'should list npm dist-tag commands for completion')

  const noMatch = distTag.completion({ conf: { argv: { remain: ['npm', 'dist-tag', 'foobar'] } } })
  t.resolveMatch(noMatch, [])
})
