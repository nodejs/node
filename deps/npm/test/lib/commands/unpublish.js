const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const tnock = require('../../fixtures/tnock.js')

const pkgManifest = (name, version = '1.0.0') => {
  return {
    _id: `${name}@${version}`,
    _rev: '00-testdeadbeef',
    name,
    versions: {
      '1.0.0': {
        name,
        version: '1.0.0',
        dist: {
          tarball: `https://registry.npmjs.org/${name}/-/${name}-1.0.0.tgz`,
        },
      },
      [version]: {
        name,
        version: version,
        dist: {
          tarball: `https://registry.npmjs.org/${name}/-/${name}-${version}.tgz`,
        },
      },
    },
    time: {
      '1.0.0': new Date(),
      [version]: new Date(),
    },
    'dist-tags': { latest: version },
  }
}

const user = 'test-user'
const pkg = 'test-package'
const auth = { '//registry.npmjs.org/:_authToken': 'test-auth-token' }

t.test('no args --force success', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
        version: '1.0.0',
      }, null, 2),
    },
  })

  const manifest = pkgManifest(pkg)
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)
    .delete(`/${pkg}/-rev/${manifest._rev}`).reply(201)

  await npm.exec('unpublish', [])
  t.equal(joinedOutput(), '- test-package@1.0.0')
})

t.test('no args --force missing package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      force: true,
    },
  })

  await t.rejects(
    npm.exec('unpublish', []),
    { code: 'EUSAGE' },
    'should throw usage instructions on missing package.json'
  )
})

t.test('no args --force error reading package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      force: true,
    },
    prefixDir: {
      'package.json': '{ not valid json ]',
    },
  })

  await t.rejects(
    npm.exec('unpublish', []),
    /Failed to parse json/,
    'should throw error from reading package.json'
  )
})

t.test('no args entire project', async t => {
  const { npm } = await loadMockNpm(t)

  await t.rejects(
    npm.exec('unpublish', []),
    /Refusing to delete entire project/
  )
})

t.test('too many args', async t => {
  const { npm } = await loadMockNpm(t)

  await t.rejects(
    npm.exec('unpublish', ['a', 'b']),
    { code: 'EUSAGE' },
    'should throw usage instructions if too many args'
  )
})

t.test('unpublish <pkg>@version not the last version', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      ...auth,
    },
  })
  const manifest = pkgManifest(pkg, '1.0.1')
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).times(3).reply(200, manifest)
    .put(`/${pkg}/-rev/${manifest._rev}`, body => {
      // sets latest and deletes version 1.0.1
      return body['dist-tags'].latest === '1.0.0' && body.versions['1.0.1'] === undefined
    }).reply(201)
    .intercept(`/${pkg}/-/${pkg}-1.0.1.tgz/-rev/${manifest._rev}`, 'DELETE').reply(201)

  await npm.exec('unpublish', ['test-package@1.0.1'])
  t.equal(joinedOutput(), '- test-package@1.0.1')
})

t.test('unpublish <pkg>@version last version', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })
  const manifest = pkgManifest(pkg)
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)

  await t.rejects(
    npm.exec('unpublish', ['test-package@1.0.0']),
    /Refusing to delete the last version of the package/
  )
})

t.test('no version found in package.json', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
      }, null, 2),
    },
  })

  const manifest = pkgManifest(pkg)

  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)
    .delete(`/${pkg}/-rev/${manifest._rev}`).reply(201)

  await npm.exec('unpublish', [])
  t.equal(joinedOutput(), '- test-package')
})

t.test('unpublish <pkg> --force no version set', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      ...auth,
    },
  })
  const manifest = pkgManifest(pkg)
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).times(2).reply(200, manifest)
    .delete(`/${pkg}/-rev/${manifest._rev}`).reply(201)

  await npm.exec('unpublish', ['test-package'])
  t.equal(joinedOutput(), '- test-package')
})

t.test('silent', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      loglevel: 'silent',
      ...auth,
    },
  })
  const manifest = pkgManifest(pkg, '1.0.1')
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).times(3).reply(200, manifest)
    .put(`/${pkg}/-rev/${manifest._rev}`, body => {
      // sets latest and deletes version 1.0.1
      return body['dist-tags'].latest === '1.0.0' && body.versions['1.0.1'] === undefined
    }).reply(201)
    .delete(`/${pkg}/-/${pkg}-1.0.1.tgz/-rev/${manifest._rev}`).reply(201)

  await npm.exec('unpublish', ['test-package@1.0.1'])
  t.equal(joinedOutput(), '')
})

t.test('workspaces', async t => {
  const prefixDir = {
    'package.json': JSON.stringify({
      name: pkg,
      version: '1.0.0',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
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
  }

  t.test('no force', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        workspaces: true,
      },
      prefixDir,
    })
    await t.rejects(
      npm.exec('unpublish', []),
      /Refusing to delete entire project/
    )
  })

  t.test('all workspaces --force', async t => {
    const { joinedOutput, npm } = await loadMockNpm(t, {
      config: {
        workspaces: true,
        force: true,
        ...auth,
      },
      prefixDir,
    })
    const manifestA = pkgManifest('workspace-a')
    const manifestB = pkgManifest('workspace-b')
    const manifestN = pkgManifest('workspace-n')
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/workspace-a?write=true').times(2).reply(200, manifestA)
      .delete(`/workspace-a/-rev/${manifestA._rev}`).reply(201)
      .get('/workspace-b?write=true').times(2).reply(200, manifestB)
      .delete(`/workspace-b/-rev/${manifestB._rev}`).reply(201)
      .get('/workspace-n?write=true').times(2).reply(200, manifestN)
      .delete(`/workspace-n/-rev/${manifestN._rev}`).reply(201)

    await npm.exec('unpublish', [])
    t.equal(joinedOutput(), '- workspace-a\n- workspace-b\n- workspace-n')
  })

  t.test('one workspace --force', async t => {
    const { joinedOutput, npm } = await loadMockNpm(t, {
      config: {
        workspace: ['workspace-a'],
        force: true,
        ...auth,
      },
      prefixDir,
    })
    const manifestA = pkgManifest('workspace-a')
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/workspace-a?write=true').times(2).reply(200, manifestA)
      .delete(`/workspace-a/-rev/${manifestA._rev}`).reply(201)

    await npm.exec('unpublish', [])
    t.equal(joinedOutput(), '- workspace-a')
  })
})

t.test('dryRun with spec', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      'dry-run': true,
      ...auth,
    },
  })

  const manifest = pkgManifest(pkg, '1.0.1')
  tnock(t, npm.config.get('registry'),
    { reqheaders: { authorization: 'Bearer test-auth-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)

  await npm.exec('unpublish', ['test-package@1.0.1'])
  t.equal(joinedOutput(), '- test-package@1.0.1')
})

t.test('dryRun with no args', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      'dry-run': true,
      ...auth,
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
        version: '1.0.0',
      }, null, 2),
    },
  })

  await npm.exec('unpublish', [])
  t.equal(joinedOutput(), '- test-package@1.0.0')
})

t.test('publishConfig no spec', async t => {
  const alternateRegistry = 'https://other.registry.npmjs.org'
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      'fetch-retries': 0,
      '//other.registry.npmjs.org/:_authToken': 'test-other-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
        version: '1.0.0',
        publishConfig: {
          registry: alternateRegistry,
        },
      }, null, 2),
    },
  })

  const manifest = pkgManifest(pkg)
  tnock(t, alternateRegistry,
    { reqheaders: { authorization: 'Bearer test-other-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)
    .delete(`/${pkg}/-rev/${manifest._rev}`).reply(201)
  await npm.exec('unpublish', [])
  t.equal(joinedOutput(), '- test-package@1.0.0')
})

t.test('publishConfig with spec', async t => {
  const alternateRegistry = 'https://other.registry.npmjs.org'
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: {
      force: true,
      'fetch-retries': 0,
      '//other.registry.npmjs.org/:_authToken': 'test-other-token',
    },
    prefixDir: {
      'package.json': JSON.stringify({
        name: pkg,
        version: '1.0.0',
        publishConfig: {
          registry: alternateRegistry,
        },
      }, null, 2),
    },
  })

  const manifest = pkgManifest(pkg)
  tnock(t, alternateRegistry,
    { reqheaders: { authorization: 'Bearer test-other-token' } })
    .get(`/${pkg}?write=true`).reply(200, manifest)
    .delete(`/${pkg}/-rev/${manifest._rev}`).reply(201)
  await npm.exec('unpublish', ['test-package'])
  t.equal(joinedOutput(), '- test-package')
})

t.test('completion', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      ...auth,
    },
  })

  const unpublish = await npm.cmd('unpublish')
  const testComp =
    async (t, { argv, partialWord, expect, title }) => {
      const res = await unpublish.completion(
        { conf: { argv: { remain: argv } }, partialWord }
      )
      t.strictSame(res, expect, title || argv.join(' '))
    }

  t.test('completing with multiple versions from the registry', async t => {
    const manifest = pkgManifest(pkg, '1.0.1')
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, { [pkg]: 'write' })
      .get(`/${pkg}?write=true`).reply(200, manifest)

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: 'test-package',
      expect: [
        'test-package@1.0.0',
        'test-package@1.0.1',
      ],
    })
  })

  t.test('no versions retrieved', async t => {
    const manifest = pkgManifest(pkg)
    manifest.versions = {}
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, { [pkg]: 'write' })
      .get(`/${pkg}?write=true`).reply(200, manifest)

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: pkg,
      expect: [
        pkg,
      ],
      title: 'should autocomplete package name only',
    })
  })

  t.test('packages starting with same letters', async t => {
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, {
        [pkg]: 'write',
        [`${pkg}a`]: 'write',
        [`${pkg}b`]: 'write',
      })

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: pkg,
      expect: [
        pkg,
        `${pkg}a`,
        `${pkg}b`,
      ],
    })
  })

  t.test('no packages retrieved', async t => {
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, {})

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [],
      title: 'should have no autocompletion',
    })
  })

  t.test('no pkg name to complete', async t => {
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, {
        [pkg]: 'write',
        [`${pkg}a`]: 'write',
      })

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: undefined,
      expect: [pkg, `${pkg}a`],
      title: 'should autocomplete with available package names from user',
    })
  })

  t.test('no pkg names retrieved from user account', async t => {
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(200, { username: user })
      .get('/-/org/test-user/package?format=cli').reply(200, null)

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: pkg,
      expect: [],
      title: 'should have no autocomplete',
    })
  })

  t.test('logged out user', async t => {
    tnock(t, npm.config.get('registry'),
      { reqheaders: { authorization: 'Bearer test-auth-token' } })
      .get('/-/whoami').reply(404)

    await testComp(t, {
      argv: ['npm', 'unpublish'],
      partialWord: pkg,
      expect: [],
    })
  })

  t.test('too many args', async t => {
    await testComp(t, {
      argv: ['npm', 'unpublish', 'foo'],
      partialWord: undefined,
      expect: [],
    })
  })
})
