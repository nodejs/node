const t = require('tap')
const { EventEmitter } = require('events')

const npmConfig = {
  usage: false,
  viewer: undefined,
  loglevel: undefined,
}

let helpSearchArgs = null
const OUTPUT = []
const npm = {
  usage: 'test npm usage',
  config: {
    get: key => npmConfig[key],
    set: (key, value) => {
      npmConfig[key] = value
    },
    parsedArgv: {
      cooked: [],
    },
  },
  exec: async (cmd, args) => {
    if (cmd === 'help-search') {
      helpSearchArgs = args
    } else if (cmd === 'help') {
      return { usage: 'npm help <term>' }
    }
  },
  deref: cmd => {},
  output: msg => {
    OUTPUT.push(msg)
  },
}

const globDefaults = [
  '/root/man/man1/npm-whoami.1',
  '/root/man/man5/npmrc.5',
  '/root/man/man7/disputes.7',
]

let globErr = null
let globResult = globDefaults
let globParam
const glob = (p, cb) => {
  globParam = p
  return cb(globErr, globResult)
}

let spawnBin = null
let spawnArgs = null
let spawnCode = 0
const spawn = (bin, args) => {
  spawnBin = bin
  spawnArgs = args
  const spawnEmitter = new EventEmitter()
  process.nextTick(() => {
    spawnEmitter.emit('exit', spawnCode)
  })
  return spawnEmitter
}

let openUrlArg = null
const openUrl = async (npm, url, msg) => {
  openUrlArg = url
}

const Help = t.mock('../../../lib/commands/help.js', {
  '../../../lib/utils/open-url.js': openUrl,
  child_process: {
    spawn,
  },
  glob,
})
const help = new Help(npm)

t.test('npm help', async t => {
  await help.exec([])

  t.match(OUTPUT, ['test npm usage'], 'showed npm usage')
})

t.test('npm help completion', async t => {
  t.teardown(() => {
    globErr = null
  })

  const noArgs = await help.completion({ conf: { argv: { remain: [] } } })
  t.strictSame(noArgs, ['help', 'whoami', 'npmrc', 'disputes'], 'outputs available help pages')
  const threeArgs = await help.completion({ conf: { argv: { remain: ['one', 'two', 'three'] } } })
  t.strictSame(threeArgs, [], 'outputs no results when more than 2 args are provided')
  globErr = new Error('glob failed')
  t.rejects(
    help.completion({ conf: { argv: { remain: [] } } }),
    /glob failed/,
    'glob errors propagate'
  )
})

t.test('npm help multiple args calls search', async t => {
  t.teardown(() => {
    helpSearchArgs = null
  })

  await help.exec(['run', 'script'])

  t.strictSame(helpSearchArgs, ['run', 'script'], 'passed the args to help-search')
})

t.test('npm help no matches calls search', async t => {
  globResult = []
  t.teardown(() => {
    helpSearchArgs = null
    globResult = globDefaults
  })

  await help.exec(['asdfasdf'])
  t.strictSame(helpSearchArgs, ['asdfasdf'], 'passed the args to help-search')
})

t.test('npm help glob errors propagate', async t => {
  globErr = new Error('glob failed')
  t.teardown(() => {
    globErr = null
    spawnBin = null
    spawnArgs = null
  })

  await t.rejects(help.exec(['whoami']), /glob failed/, 'glob error propagates')
})

t.test('npm help whoami', async t => {
  globResult = ['/root/man/man1/npm-whoami.1.xz']
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['whoami'])

  t.equal(spawnBin, 'man', 'calls man by default')
  t.strictSame(spawnArgs, [globResult[0]], 'passes the correct arguments')
})

t.test('npm help 1 install', async t => {
  npmConfig.viewer = 'browser'
  globResult = ['/root/man/man5/install.5', '/root/man/man1/npm-install.1']

  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['1', 'install'])

  t.match(openUrlArg, /commands(\/|\\)npm-install.html$/, 'attempts to open the correct url')
  t.ok(openUrlArg.startsWith('file:///'), 'opens with the correct uri schema')
})

t.test('npm help 5 install', async t => {
  npmConfig.viewer = 'browser'
  globResult = ['/root/man/man5/install.5']

  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    globParam = null
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['5', 'install'])

  t.match(globParam, /man5/, 'searches only in man5 folder')
  t.match(openUrlArg, /configuring-npm(\/|\\)install.html$/, 'attempts to open the correct url')
})

t.test('npm help 7 config', async t => {
  npmConfig.viewer = 'browser'
  globResult = ['/root/man/man7/config.7']
  t.teardown(() => {
    npmConfig.viewer = undefined
    globParam = null
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['7', 'config'])

  t.match(globParam, /man7/, 'searches only in man5 folder')
  t.match(openUrlArg, /using-npm(\/|\\)config.html$/, 'attempts to open the correct url')
})

t.test('npm help package.json redirects to package-json', async t => {
  globResult = ['/root/man/man5/package-json.5']
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['package.json'])

  t.equal(spawnBin, 'man', 'calls man by default')
  t.match(globParam, /package-json/, 'glob was asked to find package-json')
  t.strictSame(spawnArgs, [globResult[0]], 'passes the correct arguments')
})

t.test('npm help ?(un)star', async t => {
  npmConfig.viewer = 'woman'
  globResult = ['/root/man/man1/npm-star.1', '/root/man/man1/npm-unstar.1']
  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['?(un)star'])

  t.equal(spawnBin, 'emacsclient', 'maps woman to emacs correctly')
  t.strictSame(
    spawnArgs,
    ['-e', `(woman-find-file '/root/man/man1/npm-star.1')`],
    'passes the correct arguments'
  )
})

t.test('npm help - woman viewer propagates errors', async t => {
  npmConfig.viewer = 'woman'
  spawnCode = 1
  globResult = ['/root/man/man1/npm-star.1', '/root/man/man1/npm-unstar.1']
  t.teardown(() => {
    npmConfig.viewer = undefined
    spawnCode = 0
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await t.rejects(
    help.exec(['?(un)star']),
    /help process exited with code: 1/,
    'received the correct error'
  )
  t.equal(spawnBin, 'emacsclient', 'maps woman to emacs correctly')
  t.strictSame(
    spawnArgs,
    ['-e', `(woman-find-file '/root/man/man1/npm-star.1')`],
    'passes the correct arguments'
  )
})

t.test('npm help un*', async t => {
  globResult = [
    '/root/man/man1/npm-unstar.1',
    '/root/man/man1/npm-uninstall.1',
    '/root/man/man1/npm-unpublish.1',
  ]
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['un*'])

  t.equal(spawnBin, 'man', 'calls man by default')
  t.strictSame(spawnArgs, ['/root/man/man1/npm-uninstall.1'], 'passes the correct arguments')
})

t.test('npm help - man viewer propagates errors', async t => {
  spawnCode = 1
  globResult = [
    '/root/man/man1/npm-unstar.1',
    '/root/man/man1/npm-uninstall.1',
    '/root/man/man1/npm-unpublish.1',
  ]
  t.teardown(() => {
    spawnCode = 0
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await t.rejects(help.exec(['un*']), /help process exited with code: 1/, 'received correct error')
  t.equal(spawnBin, 'man', 'calls man by default')
  t.strictSame(spawnArgs, ['/root/man/man1/npm-uninstall.1'], 'passes the correct arguments')
})

t.test('npm help with complex installation path finds proper help file', async t => {
  npmConfig.viewer = 'browser'
  globResult = [
    'C:/Program Files/node-v14.15.5-win-x64/node_modules/npm/man/man1/npm-install.1',
    // glob always returns forward slashes, even on Windows
  ]

  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  await help.exec(['1', 'install'])

  t.match(openUrlArg, /commands(\/|\\)npm-install.html$/, 'attempts to open the correct url')
})

t.test('npm help - prefers npm help pages', async t => {
  // Unusual ordering is to get full test coverage of all branches inside the
  // sort function.
  globResult = [
    '/root/man/man6/npm-install.6',
    '/root/man/man1/install.1',
    '/root/man/man5/npm-install.5',
  ]
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })
  await help.exec(['install'])

  t.equal(spawnBin, 'man', 'calls man by default')
  t.strictSame(spawnArgs, ['/root/man/man5/npm-install.5'], 'passes the correct arguments')
})

t.test('npm help - works in the presence of strange man pages', async t => {
  // Unusual ordering is to get full test coverage of all branches inside the
  // sort function.
  globResult = [
    '/root/man/man6/config.6strange',
    '/root/man/man1/config.1',
    '/root/man/man5/config.5ssl',
  ]
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })
  await help.exec(['config'])

  t.equal(spawnBin, 'man', 'calls man by default')
  t.strictSame(spawnArgs, ['/root/man/man1/config.1'], 'passes the correct arguments')
})
