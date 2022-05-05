const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const MockRegistry = require('../../fixtures/mock-registry.js')
const pacote = require('pacote')
const path = require('path')
const fs = require('@npmcli/fs')
const npa = require('npm-package-arg')

const pkg = 'test-package'
const token = 'test-auth-token'
const auth = { '//registry.npmjs.org/:_authToken': token }
const alternateRegistry = 'https://other.registry.npmjs.org'
const basic = Buffer.from('test-user:test-password').toString('base64')

const pkgJson = {
  name: pkg,
  description: 'npm test package',
  version: '1.0.0',
}

t.cleanSnapshot = data => {
  return data.replace(/shasum:.*/g, 'shasum:{sha}')
    .replace(/integrity:.*/g, 'integrity:{sha}')
    .replace(/"shasum": ".*",/g, '"shasum": "{sha}",')
    .replace(/"integrity": ".*",/g, '"integrity": "{sha}",')
}

t.test('respects publishConfig.registry, runs appropriate scripts', async t => {
  const { npm, joinedOutput, prefix } = await loadMockNpm(t, {
    config: {
      loglevel: 'silent', // prevent scripts from leaking to stdout during the test
      [`${alternateRegistry.slice(6)}/:_authToken`]: 'test-other-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        scripts: {
          prepublishOnly: 'touch scripts-prepublishonly',
          prepublish: 'touch scripts-prepublish', // should NOT run this one
          publish: 'touch scripts-publish',
          postpublish: 'touch scripts-postpublish',
        },
        publishConfig: { registry: alternateRegistry },
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    authorization: 'test-other-token',
  })
  registry.nock.put(`/${pkg}`, body => {
    return t.match(body, {
      _id: pkg,
      name: pkg,
      'dist-tags': { latest: '1.0.0' },
      access: null,
      versions: {
        '1.0.0': {
          name: pkg,
          version: '1.0.0',
          _id: `${pkg}@1.0.0`,
          dist: {
            shasum: /\.*/,
            tarball: `http:${alternateRegistry.slice(6)}/test-package/-/test-package-1.0.0.tgz`,
          },
          publishConfig: {
            registry: alternateRegistry,
          },
        },
      },
      _attachments: {
        [`${pkg}-1.0.0.tgz`]: {},
      },
    })
  }).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
  t.resolveMatch(fs.exists(path.join(prefix, 'scripts-prepublishonly')), true, 'ran prepublishOnly')
  t.resolveMatch(
    fs.exists(path.join(prefix, 'scripts-prepublish')),
    false,
    'did not run prepublish'
  )
  t.resolveMatch(fs.exists(path.join(prefix, 'scripts-publish')), true, 'ran publish')
  t.resolveMatch(fs.exists(path.join(prefix, 'scripts-postpublish')), true, 'ran postpublish')
})

t.test('re-loads publishConfig.registry if added during script process', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      [`${alternateRegistry.slice(6)}/:_authToken`]: 'test-other-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        scripts: {
          prepare: 'cp new.json package.json',
        },
      }, null, 2),
      'new.json': JSON.stringify({
        ...pkgJson,
        publishConfig: { registry: alternateRegistry },
      }),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    authorization: 'test-other-token',
  })
  registry.nock.put(`/${pkg}`, body => {
    return t.match(body, {
      _id: pkg,
      name: pkg,
      'dist-tags': { latest: '1.0.0' },
      access: null,
      versions: {
        '1.0.0': {
          name: pkg,
          version: '1.0.0',
          _id: `${pkg}@1.0.0`,
          dist: {
            shasum: /\.*/,
            tarball: `http:${alternateRegistry.slice(6)}/test-package/-/test-package-1.0.0.tgz`,
          },
          publishConfig: {
            registry: alternateRegistry,
          },
        },
      },
      _attachments: {
        [`${pkg}-1.0.0.tgz`]: {},
      },
    })
  }).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
})

t.test('json', async t => {
  const { joinedOutput, npm, logs } = await loadMockNpm(t, {
    config: {
      json: true,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.put(`/${pkg}`).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(logs.notice)
  t.matchSnapshot(joinedOutput(), 'new package json')
})

t.test('dry-run', async t => {
  const { joinedOutput, npm, logs } = await loadMockNpm(t, {
    config: {
      'dry-run': true,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await npm.exec('publish', [])
  t.equal(joinedOutput(), `+ ${pkg}@1.0.0`)
  t.matchSnapshot(logs.notice)
})

t.test('shows usage with wrong set of arguments', async t => {
  t.plan(1)
  const Publish = t.mock('../../../lib/commands/publish.js')
  const publish = new Publish({})

  await t.rejects(publish.exec(['a', 'b', 'c']), publish.usage)
})

t.test('throws when invalid tag', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      tag: '0.0.13',
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await t.rejects(
    npm.exec('publish', []),
    { message: 'Tag name must not be a valid SemVer range: 0.0.13' }
  )
})

t.test('tarball', async t => {
  const { npm, joinedOutput, logs, home } = await loadMockNpm(t, {
    config: {
      'fetch-retries': 0,
      ...auth,
    },
    homeDir: {
      'package.json': JSON.stringify({
        name: 'test-tar-package',
        description: 'this was from a tarball',
        version: '1.0.0',
      }, null, 2),
      'index.js': 'console.log("hello world"}',
    },
  })
  const tarball = await pacote.tarball(home)
  const tarFilename = path.join(home, 'tarball.tgz')
  await fs.writeFile(tarFilename, tarball)
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.put('/test-tar-package', body => {
    return t.match(body, {
      name: 'test-tar-package',
    })
  }).reply(200, {})
  await npm.exec('publish', [tarFilename])
  t.matchSnapshot(logs.notice)
  t.matchSnapshot(joinedOutput(), 'new package json')
})

t.test('no auth default registry', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await t.rejects(
    npm.exec('publish', []),
    {
      message: 'This command requires you to be logged in to https://registry.npmjs.org/',
      code: 'ENEEDAUTH',
    }
  )
})

t.test('no auth dry-run', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: {
      'dry-run': true,
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput())
  t.matchSnapshot(logs.warn, 'warns about auth being needed')
})

t.test('no auth for configured registry', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      registry: alternateRegistry,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await t.rejects(
    npm.exec('publish', []),
    {
      message: `This command requires you to be logged in to ${alternateRegistry}`,
      code: 'ENEEDAUTH',
    }
  )
})

t.test('no auth for scope configured registry', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      '@npm:registry': alternateRegistry,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await t.rejects(
    npm.exec('publish', []),
    {
      message: `This command requires you to be logged in to ${alternateRegistry}`,
      code: 'ENEEDAUTH',
    }
  )
})

t.test('has auth for scope configured registry', async t => {
  const spec = npa('@npm/test-package')
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '@npm:registry': alternateRegistry,
      [`${alternateRegistry.slice(6)}/:_authToken`]: 'test-scope-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    authorization: 'test-scope-token',
  })
  registry.nock.put(`/${spec.escapedName}`, body => {
    return t.match(body, { name: '@npm/test-package' })
  }).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
})

t.test('workspaces', t => {
  const dir = {
    'package.json': JSON.stringify(
      {
        ...pkgJson,
        workspaces: ['workspace-a', 'workspace-b', 'workspace-c', 'workspace-p'],
      }, null, 2),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.2.3-a',
        repository: 'http://repo.workspace-a/',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.2.3-n',
        repository: 'https://github.com/npm/workspace-b',
      }),
    },
    'workspace-c': {
      'package.json': JSON.stringify({
        name: 'workspace-n',
        version: '1.2.3-n',
      }),
    },
    'workspace-p': {
      'package.json': JSON.stringify({
        name: 'workspace-p',
        version: '1.2.3-p',
        private: true,
      }),
    },
  }

  t.test('all workspaces - no color', async t => {
    const { npm, joinedOutput, logs } = await loadMockNpm(t, {
      config: {
        color: false,
        ...auth,
        workspaces: true,
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
      authorization: token,
    })
    registry.nock
      .put('/workspace-a', body => {
        return t.match(body, { name: 'workspace-a' })
      }).reply(200, {})
      .put('/workspace-b', body => {
        return t.match(body, { name: 'workspace-b' })
      }).reply(200, {})
      .put('/workspace-n', body => {
        return t.match(body, { name: 'workspace-n' })
      }).reply(200, {})
    await npm.exec('publish', [])
    t.matchSnapshot(joinedOutput(), 'all public workspaces')
    t.matchSnapshot(logs.warn, 'warns about skipped private workspace')
  })

  t.test('all workspaces - color', async t => {
    const { npm, joinedOutput, logs } = await loadMockNpm(t, {
      config: {
        ...auth,
        color: 'always',
        workspaces: true,
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
      authorization: token,
    })
    registry.nock
      .put('/workspace-a', body => {
        return t.match(body, { name: 'workspace-a' })
      }).reply(200, {})
      .put('/workspace-b', body => {
        return t.match(body, { name: 'workspace-b' })
      }).reply(200, {})
      .put('/workspace-n', body => {
        return t.match(body, { name: 'workspace-n' })
      }).reply(200, {})
    await npm.exec('publish', [])
    t.matchSnapshot(joinedOutput(), 'all public workspaces')
    t.matchSnapshot(logs.warn, 'warns about skipped private workspace in color')
  })

  t.test('one workspace - success', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      config: {
        ...auth,
        workspace: ['workspace-a'],
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
      authorization: token,
    })
    registry.nock
      .put('/workspace-a', body => {
        return t.match(body, { name: 'workspace-a' })
      }).reply(200, {})
    await npm.exec('publish', [])
    t.matchSnapshot(joinedOutput(), 'single workspace')
  })

  t.test('one workspace - failure', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        ...auth,
        workspace: ['workspace-a'],
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
      authorization: token,
    })
    registry.nock
      .put('/workspace-a', body => {
        return t.match(body, { name: 'workspace-a' })
      }).reply(404, {})
    await t.rejects(npm.exec('publish', []), { code: 'E404' })
  })

  t.test('invalid workspace', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        ...auth,
        workspace: ['workspace-x'],
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    await t.rejects(
      npm.exec('publish', []),
      { message: 'No workspaces found:\n  --workspace=workspace-x' }
    )
  })

  t.test('json', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
      config: {
        ...auth,
        workspaces: true,
        json: true,
      },
      globals: ({ prefix }) => ({
        'process.cwd': () => prefix,
      }),
      prefixDir: dir,
    })
    const registry = new MockRegistry({
      tap: t,
      registry: npm.config.get('registry'),
      authorization: token,
    })
    registry.nock
      .put('/workspace-a', body => {
        return t.match(body, { name: 'workspace-a' })
      }).reply(200, {})
      .put('/workspace-b', body => {
        return t.match(body, { name: 'workspace-b' })
      }).reply(200, {})
      .put('/workspace-n', body => {
        return t.match(body, { name: 'workspace-n' })
      }).reply(200, {})
    await npm.exec('publish', [])
    t.matchSnapshot(joinedOutput(), 'all workspaces in json')
  })
  t.end()
})

t.test('ignore-scripts', async t => {
  const { npm, joinedOutput, prefix } = await loadMockNpm(t, {
    config: {
      ...auth,
      'ignore-scripts': true,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        ...pkgJson,
        scripts: {
          prepublishOnly: 'touch scripts-prepublishonly',
          prepublish: 'touch scripts-prepublish', // should NOT run this one
          publish: 'touch scripts-publish',
          postpublish: 'touch scripts-postpublish',
        },
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.put(`/${pkg}`).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
  t.resolveMatch(
    fs.exists(path.join(prefix, 'scripts-prepublishonly')),
    false,
    'did not run prepublishOnly'
  )
  t.resolveMatch(
    fs.exists(path.join(prefix, 'scripts-prepublish')),
    false,
    'did not run prepublish'
  )
  t.resolveMatch(
    fs.exists(path.join(prefix, 'scripts-publish')),
    false,
    'did not run publish'
  )
  t.resolveMatch(
    fs.exists(path.join(prefix, 'scripts-postpublish')),
    false,
    'did not run postpublish'
  )
})

t.test('_auth config default registry', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      _auth: basic,
    },
    prefixDir: {
      'package.json': JSON.stringify(pkgJson),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    basic,
  })
  registry.nock.put(`/${pkg}`).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
})

t.test('bare _auth and registry config', async t => {
  const spec = npa('@npm/test-package')
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      registry: alternateRegistry,
      _auth: basic,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    basic,
  })
  registry.nock.put(`/${spec.escapedName}`).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
})

t.test('bare _auth config scoped registry', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      '@npm:registry': alternateRegistry,
      _auth: basic,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  await t.rejects(
    npm.exec('publish', []),
    { message: `This command requires you to be logged in to ${alternateRegistry}` }
  )
})

t.test('scoped _auth config scoped registry', async t => {
  const spec = npa('@npm/test-package')
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '@npm:registry': alternateRegistry,
      [`${alternateRegistry.slice(6)}/:_auth`]: basic,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npm/test-package',
        version: '1.0.0',
      }, null, 2),
    },
    globals: ({ prefix }) => ({
      'process.cwd': () => prefix,
    }),
  })
  const registry = new MockRegistry({
    tap: t,
    registry: alternateRegistry,
    basic,
  })
  registry.nock.put(`/${spec.escapedName}`).reply(200, {})
  await npm.exec('publish', [])
  t.matchSnapshot(joinedOutput(), 'new package version')
})
