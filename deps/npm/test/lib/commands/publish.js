const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')
const fs = require('fs')

t.cleanSnapshot = data => {
  return data.replace(/^ *"gitHead": .*$\n/gm, '')
}

const { definitions } = require('../../../lib/utils/config')
const defaults = Object.entries(definitions).reduce((defaults, [key, def]) => {
  defaults[key] = def.default
  return defaults
}, {})

t.test(
  /* eslint-disable-next-line max-len */
  'should publish with libnpmpublish, passing through flatOptions and respecting publishConfig.registry',
  async t => {
    t.plan(6)

    const registry = 'https://some.registry'
    const publishConfig = { registry }
    const testDir = t.testdir({
      'package.json': JSON.stringify(
        {
          name: 'my-cool-pkg',
          version: '1.0.0',
          publishConfig,
        },
        null,
        2
      ),
    })

    const Publish = t.mock('../../../lib/commands/publish.js', {
      // verify that we do NOT remove publishConfig if it was there originally
      // and then removed during the script/pack process
      libnpmpack: async () => {
        fs.writeFileSync(
          `${testDir}/package.json`,
          JSON.stringify({
            name: 'my-cool-pkg',
            version: '1.0.0',
          })
        )
        return Buffer.from('')
      },
      libnpmpublish: {
        publish: (manifest, tarData, opts) => {
          t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
          t.type(tarData, Buffer, 'tarData is a buffer')
          t.ok(opts, 'gets opts object')
          t.same(opts.customValue, true, 'flatOptions values are passed through')
          t.same(opts.registry, registry, 'publishConfig.registry is passed through')
        },
      },
    })
    const npm = mockNpm({
      flatOptions: {
        customValue: true,
        workspacesEnabled: true,
      },
    })
    npm.config.getCredentialsByURI = uri => {
      t.same(uri, registry, 'gets credentials for expected registry')
      return { token: 'some.registry.token' }
    }
    const publish = new Publish(npm)

    await publish.exec([testDir])
  }
)

t.test('re-loads publishConfig.registry if added during script process', async t => {
  t.plan(5)
  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js', {
    libnpmpack: async () => {
      fs.writeFileSync(
        `${testDir}/package.json`,
        JSON.stringify({
          name: 'my-cool-pkg',
          version: '1.0.0',
          publishConfig,
        })
      )
      return Buffer.from('')
    },
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.type(tarData, Buffer, 'tarData is a buffer')
        t.ok(opts, 'gets opts object')
        t.same(opts.registry, registry, 'publishConfig.registry is passed through')
      },
    },
  })
  const npm = mockNpm()
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  await publish.exec([testDir])
})

t.test('if loglevel=info and json, should not output package contents', async t => {
  t.plan(3)

  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js', {
    '../../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.fail('logTar is not called in json mode')
      },
    },
    libnpmpublish: {
      publish: () => {
        t.pass('publish called')
      },
    },
  })
  const npm = mockNpm({
    config: { json: true, loglevel: 'info' },
    output: () => {
      t.pass('output is called')
    },
  }, t)
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  await publish.exec([testDir])
})

t.test(
  /* eslint-disable-next-line max-len */
  'if loglevel=silent and dry-run, should not output package contents or publish or validate credentials, should log tarball contents',
  async t => {
    t.plan(1)

    const testDir = t.testdir({
      'package.json': JSON.stringify(
        {
          name: 'my-cool-pkg',
          version: '1.0.0',
        },
        null,
        2
      ),
    })

    const Publish = t.mock('../../../lib/commands/publish.js', {
      '../../../lib/utils/tar.js': {
        getContents: () => ({
          id: 'someid',
        }),
        logTar: () => {
          t.pass('logTar is called')
        },
      },
      libnpmpublish: {
        publish: () => {
          throw new Error('should not call libnpmpublish in dry run')
        },
      },
    })
    const npm = mockNpm({
      config: { 'dry-run': true, loglevel: 'silent' },
      output: () => {
        throw new Error('should not output in dry run mode')
      },
    }, t)
    npm.config.getCredentialsByURI = () => {
      throw new Error('should not call getCredentialsByURI in dry run')
    }

    const publish = new Publish(npm)

    await publish.exec([testDir])
  }
)

t.test(
  /* eslint-disable-next-line max-len */
  'if loglevel=info and dry-run, should not publish, should log package contents and log tarball contents',
  async t => {
    t.plan(2)

    const testDir = t.testdir({
      'package.json': JSON.stringify(
        {
          name: 'my-cool-pkg',
          version: '1.0.0',
        },
        null,
        2
      ),
    })

    const Publish = t.mock('../../../lib/commands/publish.js', {
      '../../../lib/utils/tar.js': {
        getContents: () => ({
          id: 'someid',
        }),
        logTar: () => {
          t.pass('logTar is called')
        },
      },
      libnpmpublish: {
        publish: () => {
          throw new Error('should not call libnpmpublish in dry run')
        },
      },
    })
    const npm = mockNpm({
      config: { 'dry-run': true, loglevel: 'info' },
      output: () => {
        t.pass('output fn is called')
      },
    }, t)
    npm.config.getCredentialsByURI = () => {
      throw new Error('should not call getCredentialsByURI in dry run')
    }
    const publish = new Publish(npm)

    await publish.exec([testDir])
  }
)

t.test('shows usage with wrong set of arguments', async t => {
  t.plan(1)
  const Publish = t.mock('../../../lib/commands/publish.js')
  const publish = new Publish({})

  await t.rejects(publish.exec(['a', 'b', 'c']), publish.usage)
})

t.test('throws when invalid tag', async t => {
  t.plan(1)

  const Publish = t.mock('../../../lib/commands/publish.js')
  const npm = mockNpm({
    config: { tag: '0.0.13' },
  })
  const publish = new Publish(npm)

  await t.rejects(
    publish.exec([]),
    /Tag name must not be a valid SemVer range: /,
    'throws when tag name is a valid SemVer range'
  )
})

t.test('can publish a tarball', async t => {
  t.plan(3)

  const testDir = t.testdir({
    tarball: {},
    package: {
      'package.json': JSON.stringify({
        name: 'my-cool-tarball',
        version: '1.2.3',
      }),
    },
  })
  const tar = require('tar')
  tar.c(
    {
      cwd: testDir,
      file: `${testDir}/tarball/package.tgz`,
      sync: true,
    },
    ['package']
  )

  const tarFile = fs.readFileSync(`${testDir}/tarball/package.tgz`)
  const Publish = t.mock('../../../lib/commands/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(
          manifest,
          {
            name: 'my-cool-tarball',
            version: '1.2.3',
          },
          'sent manifest to lib pub'
        )
        t.strictSame(tarData, tarFile, 'sent the tarball data to lib pub')
      },
    },
  })
  const npm = mockNpm()
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  await publish.exec([`${testDir}/tarball/package.tgz`])
})

t.test('should check auth for default registry', async t => {
  t.plan(2)
  const npm = mockNpm()
  const registry = npm.config.get('registry')
  const errorMessage = `This command requires you to be logged in to ${registry}`
  const Publish = t.mock('../../../lib/commands/publish.js')
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

  await t.rejects(
    publish.exec([]),
    { message: errorMessage, code: 'ENEEDAUTH' },
    'throws when not logged in'
  )
})

t.test('should check auth for configured registry', async t => {
  t.plan(2)
  const registry = 'https://some.registry'
  const errorMessage = 'This command requires you to be logged in to https://some.registry'
  const Publish = t.mock('../../../lib/commands/publish.js')
  const npm = mockNpm({
    flatOptions: { registry },
  })
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

  await t.rejects(
    publish.exec([]),
    { message: errorMessage, code: 'ENEEDAUTH' },
    'throws when not logged in'
  )
})

t.test('should check auth for scope specific registry', async t => {
  t.plan(2)
  const registry = 'https://some.registry'
  const errorMessage = 'This command requires you to be logged in to https://some.registry'
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: '@npm/my-cool-pkg',
        version: '1.0.0',
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js')
  const npm = mockNpm({
    flatOptions: { '@npm:registry': registry },
  })
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

  await t.rejects(
    publish.exec([testDir]),
    { message: errorMessage, code: 'ENEEDAUTH' },
    'throws when not logged in'
  )
})

t.test('should use auth for scope specific registry', async t => {
  t.plan(3)
  const registry = 'https://some.registry'
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: '@npm/my-cool-pkg',
        version: '1.0.0',
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.ok(opts, 'gets opts object')
        t.same(opts['@npm:registry'], registry, 'scope specific registry is passed through')
      },
    },
  })
  const npm = mockNpm({
    flatOptions: { '@npm:registry': registry },
  })
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  await publish.exec([testDir])
})

t.test('read registry only from publishConfig', async t => {
  t.plan(3)

  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
        publishConfig,
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.same(opts.registry, registry, 'publishConfig is passed through')
      },
    },
  })
  const npm = mockNpm()
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  await publish.exec([testDir])
})

t.test('able to publish after if encountered multiple configs', async t => {
  t.plan(2)

  const registry = 'https://some.registry'
  const tag = 'better-tag'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
        publishConfig,
      },
      null,
      2
    ),
  })

  const configList = [defaults]
  configList.unshift(
    Object.assign(Object.create(configList[0]), {
      registry: `https://other.registry`,
      tag: 'some-tag',
    })
  )
  configList.unshift(Object.assign(Object.create(configList[0]), { tag }))

  const Publish = t.mock('../../../lib/commands/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.same(opts.defaultTag, tag, 'gets option for expected tag')
      },
    },
  })
  const publish = new Publish({
    // what would be flattened by the configList created above
    flatOptions: {
      defaultTag: 'better-tag',
      registry: 'https://other.registry',
    },
    output () {},
    config: {
      get: key => configList[0][key],
      list: configList,
      getCredentialsByURI: uri => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  await publish.exec([testDir])
})

t.test('workspaces', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
        workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
      },
      null,
      2
    ),
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
  })

  const publishes = []
  const outputs = []
  t.beforeEach(() => {
    npm.config.set('json', false)
    outputs.length = 0
    publishes.length = 0
  })
  const Publish = t.mock('../../../lib/commands/publish.js', {
    '../../../lib/utils/tar.js': {
      getContents: manifest => ({
        id: manifest._id,
      }),
      logTar: () => {},
    },
    libnpmpublish: {
      publish: (manifest, tarballData, opts) => {
        publishes.push(manifest)
      },
    },
  })
  const npm = mockNpm({
    output: o => {
      outputs.push(o)
    },
  })
  npm.localPrefix = testDir
  npm.config.getCredentialsByURI = uri => {
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  t.test('all workspaces', async t => {
    await publish.execWorkspaces([], [])
    t.matchSnapshot(publishes, 'should publish all workspaces')
    t.matchSnapshot(outputs, 'should output all publishes')
  })

  t.test('one workspace', async t => {
    await publish.execWorkspaces([], ['workspace-a'])
    t.matchSnapshot(publishes, 'should publish given workspace')
    t.matchSnapshot(outputs, 'should output one publish')
  })

  t.test('invalid workspace', async t => {
    await t.rejects(publish.execWorkspaces([], ['workspace-x']), /No workspaces found/)
    await t.rejects(publish.execWorkspaces([], ['workspace-x']), /workspace-x/)
  })

  t.test('json', async t => {
    npm.config.set('json', true)
    await publish.execWorkspaces([], [])
    t.matchSnapshot(publishes, 'should publish all workspaces')
    t.matchSnapshot(outputs, 'should output all publishes as json')
  })
  t.end()
})

t.test('private workspaces', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-project',
      version: '1.0.0',
      workspaces: ['packages/*'],
    }),
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: '@npmcli/a',
          version: '1.0.0',
          private: true,
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: '@npmcli/b',
          version: '1.0.0',
        }),
      },
    },
  })

  const publishes = []
  const outputs = []
  t.beforeEach(() => {
    npm.config.set('json', false)
    outputs.length = 0
    publishes.length = 0
  })
  const mocks = {
    '../../../lib/utils/tar.js': {
      getContents: manifest => ({
        id: manifest._id,
      }),
      logTar: () => {},
    },
    libnpmpublish: {
      publish: (manifest, tarballData, opts) => {
        if (manifest.private) {
          throw Object.assign(new Error('private pkg'), { code: 'EPRIVATE' })
        }
        publishes.push(manifest)
      },
    },
  }
  const npm = mockNpm({
    config: { loglevel: 'info' },
    output: o => {
      outputs.push(o)
    },
  }, t)
  npm.localPrefix = testDir
  npm.config.getCredentialsByURI = uri => {
    return { token: 'some.registry.token' }
  }

  t.test('with color', async t => {
    t.plan(4)

    const Publish = t.mock('../../../lib/commands/publish.js', {
      ...mocks,
      'proc-log': {
        notice () {},
        verbose () {},
        warn (title, msg) {
          t.equal(title, 'publish', 'should use publish warn title')
          t.match(
            msg,
            /* eslint-disable-next-line max-len */
            'Skipping workspace \u001b[32m@npmcli/a\u001b[39m, marked as \u001b[1mprivate\u001b[22m',
            'should display skip private workspace warn msg'
          )
        },
      },
    })
    const publish = new Publish(npm)

    npm.color = true
    await publish.execWorkspaces([], [])
    t.matchSnapshot(publishes, 'should publish all non-private workspaces')
    t.matchSnapshot(outputs, 'should output all publishes')
    npm.color = false
  })

  t.test('colorless', async t => {
    t.plan(4)

    const Publish = t.mock('../../../lib/commands/publish.js', {
      ...mocks,
      'proc-log': {
        notice () {},
        verbose () {},
        warn (title, msg) {
          t.equal(title, 'publish', 'should use publish warn title')
          t.equal(
            msg,
            'Skipping workspace @npmcli/a, marked as private',
            'should display skip private workspace warn msg'
          )
        },
      },
    })
    const publish = new Publish(npm)

    await publish.execWorkspaces([], [])
    t.matchSnapshot(publishes, 'should publish all non-private workspaces')
    t.matchSnapshot(outputs, 'should output all publishes')
  })

  t.test('unexpected error', async t => {
    t.plan(2)

    const Publish = t.mock('../../../lib/commands/publish.js', {
      ...mocks,
      libnpmpublish: {
        publish: (manifest, tarballData, opts) => {
          if (manifest.private) {
            throw new Error('ERR')
          }
          publishes.push(manifest)
        },
      },
      'proc-log': {
        notice (__, msg) {
          t.match(msg, 'Publishing to https://registry.npmjs.org/')
        },
        verbose () {},
      },
    })
    const publish = new Publish(npm)

    await t.rejects(publish.execWorkspaces([], []), /ERR/, 'should throw unexpected error')
  })

  t.end()
})

t.test('runs correct lifecycle scripts', async t => {
  t.plan(5)

  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
        scripts: {
          prepublishOnly: 'echo test prepublishOnly',
          prepublish: 'echo test prepublish', // should NOT run this one
          publish: 'echo test publish',
          postpublish: 'echo test postpublish',
        },
      },
      null,
      2
    ),
  })

  const scripts = []
  const Publish = t.mock('../../../lib/commands/publish.js', {
    '@npmcli/run-script': args => {
      scripts.push(args)
    },
    '../../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.pass('logTar is called')
      },
    },
    libnpmpublish: {
      publish: () => {
        t.pass('publish called')
      },
    },
  })
  const npm = mockNpm({
    config: { loglevel: 'info' },
    output: () => {
      t.pass('output is called')
    },
  }, t)
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)
  await publish.exec([testDir])
  t.same(
    scripts.map(s => s.event),
    ['prepublishOnly', 'publish', 'postpublish'],
    'runs only expected scripts, in order'
  )
})

t.test('does not run scripts on --ignore-scripts', async t => {
  t.plan(4)

  const testDir = t.testdir({
    'package.json': JSON.stringify(
      {
        name: 'my-cool-pkg',
        version: '1.0.0',
      },
      null,
      2
    ),
  })

  const Publish = t.mock('../../../lib/commands/publish.js', {
    '@npmcli/run-script': () => {
      t.fail('should not call run-script')
    },
    '../../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.pass('logTar is called')
      },
    },
    libnpmpublish: {
      publish: () => {
        t.pass('publish called')
      },
    },
  })
  const npm = mockNpm({
    config: { 'ignore-scripts': true, loglevel: 'info' },
    output: () => {
      t.pass('output is called')
    },
  }, t)
  npm.config.getCredentialsByURI = uri => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)
  await publish.exec([testDir])
})
