const { test } = require('tap')
const requireInject = require('require-inject')
const { EventEmitter } = require('events')

let npmUsageArg = null
const npmUsage = (arg) => {
  npmUsageArg = arg
}

const npmConfig = {
  usage: false,
  viewer: undefined,
  loglevel: undefined,
}

let helpSearchArgs = null
const npm = {
  config: {
    get: (key) => npmConfig[key],
    set: (key, value) => {
      npmConfig[key] = value
    },
    parsedArgv: {
      cooked: [],
    },
  },
  commands: {
    'help-search': (args, cb) => {
      helpSearchArgs = args
      return cb()
    },
    help: {
      usage: 'npm help <term>',
    },
  },
  deref: (cmd) => {},
}

const OUTPUT = []
const output = (msg) => {
  OUTPUT.push(msg)
}

const globDefaults = [
  '/root/man/man1/npm-whoami.1',
  '/root/man/man5/npmrc.5',
  '/root/man/man7/disputes.7',
]

let globErr = null
let globResult = globDefaults
const glob = (p, cb) => {
  return cb(globErr, globResult)
}

let spawnBin = null
let spawnArgs = null
const spawn = (bin, args) => {
  spawnBin = bin
  spawnArgs = args
  const spawnEmitter = new EventEmitter()
  process.nextTick(() => {
    spawnEmitter.emit('close', 0)
  })
  return spawnEmitter
}

let openUrlArg = null
const openUrl = (url, msg, cb) => {
  openUrlArg = url
  return cb()
}

const help = requireInject('../../lib/help.js', {
  '../../lib/npm.js': npm,
  '../../lib/utils/npm-usage.js': npmUsage,
  '../../lib/utils/open-url.js': openUrl,
  '../../lib/utils/output.js': output,
  '../../lib/utils/spawn.js': spawn,
  glob,
})

test('npm help', t => {
  t.teardown(() => {
    npmUsageArg = null
  })

  return help([], (err) => {
    if (err)
      throw err

    t.equal(npmUsageArg, false, 'called npmUsage')
    t.end()
  })
})

test('npm help completion', async t => {
  t.teardown(() => {
    globErr = null
  })
  const completion = (opts) => new Promise((resolve, reject) => {
    help.completion(opts, (err, res) => {
      if (err)
        return reject(err)
      return resolve(res)
    })
  })

  const noArgs = await completion({ conf: { argv: { remain: [] } } })
  t.strictSame(noArgs, ['help', 'whoami', 'npmrc', 'disputes'], 'outputs available help pages')
  const threeArgs = await completion({ conf: { argv: { remain: ['one', 'two', 'three'] } } })
  t.strictSame(threeArgs, [], 'outputs no results when more than 2 args are provided')
  globErr = new Error('glob failed')
  t.rejects(completion({ conf: { argv: { remain: [] } } }), /glob failed/, 'glob errors propagate')
})

test('npm help -h', t => {
  npmConfig.usage = true
  t.teardown(() => {
    npmConfig.usage = false
    OUTPUT.length = 0
  })

  return help(['help'], (err) => {
    if (err)
      throw err

    t.strictSame(OUTPUT, ['npm help <term>'], 'outputs usage information for command')
    t.end()
  })
})

test('npm help multiple args calls search', t => {
  t.teardown(() => {
    helpSearchArgs = null
  })

  return help(['run', 'script'], (err) => {
    if (err)
      throw err

    t.strictSame(helpSearchArgs, ['run', 'script'], 'passed the args to help-search')
    t.end()
  })
})

test('npm help no matches calls search', t => {
  globResult = []
  t.teardown(() => {
    helpSearchArgs = null
    globResult = globDefaults
  })

  return help(['asdfasdf'], (err) => {
    if (err)
      throw err

    t.strictSame(helpSearchArgs, ['asdfasdf'], 'passed the args to help-search')
    t.end()
  })
})

test('npm help glob errors propagate', t => {
  globErr = new Error('glob failed')
  t.teardown(() => {
    globErr = null
    spawnBin = null
    spawnArgs = null
  })

  return help(['whoami'], (err) => {
    t.match(err, /glob failed/, 'glob error propagates')
    t.end()
  })
})

test('npm help whoami', t => {
  globResult = ['/root/man/man1/npm-whoami.1.xz']
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['whoami'], (err) => {
    if (err)
      throw err

    t.equal(spawnBin, 'man', 'calls man by default')
    t.strictSame(spawnArgs, ['1', 'npm-whoami'], 'passes the correct arguments')
    t.end()
  })
})

test('npm help 1 install', t => {
  npmConfig.viewer = 'browser'
  globResult = [
    '/root/man/man5/install.5',
    '/root/man/man1/npm-install.1',
  ]

  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['1', 'install'], (err) => {
    if (err)
      throw err

    t.match(openUrlArg, /commands(\/|\\)npm-install.html$/, 'attempts to open the correct url')
    t.end()
  })
})

test('npm help 5 install', t => {
  npmConfig.viewer = 'browser'
  globResult = [
    '/root/man/man5/install.5',
    '/root/man/man1/npm-install.1',
  ]

  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['5', 'install'], (err) => {
    if (err)
      throw err

    t.match(openUrlArg, /configuring-npm(\/|\\)install.html$/, 'attempts to open the correct url')
    t.end()
  })
})

test('npm help 7 config', t => {
  npmConfig.viewer = 'browser'
  globResult = [
    '/root/man/man1/npm-config.1',
    '/root/man/man7/config.7',
  ]
  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['7', 'config'], (err) => {
    if (err)
      throw err

    t.match(openUrlArg, /using-npm(\/|\\)config.html$/, 'attempts to open the correct url')
    t.end()
  })
})

test('npm help with browser viewer and invalid section throws', t => {
  npmConfig.viewer = 'browser'
  globResult = [
    '/root/man/man1/npm-config.1',
    '/root/man/man7/config.7',
    '/root/man/man9/config.9',
  ]
  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['9', 'config'], (err) => {
    t.match(err, /invalid man section: 9/, 'throws appropriate error')
    t.end()
  })
})

test('npm help global redirects to folders', t => {
  globResult = ['/root/man/man5/folders.5']
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['global'], (err) => {
    if (err)
      throw err

    t.equal(spawnBin, 'man', 'calls man by default')
    t.strictSame(spawnArgs, ['5', 'folders'], 'passes the correct arguments')
    t.end()
  })
})

test('npm help package.json redirects to package-json', t => {
  globResult = ['/root/man/man5/package-json.5']
  t.teardown(() => {
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['package.json'], (err) => {
    if (err)
      throw err

    t.equal(spawnBin, 'man', 'calls man by default')
    t.strictSame(spawnArgs, ['5', 'package-json'], 'passes the correct arguments')
    t.end()
  })
})

test('npm help ?(un)star', t => {
  npmConfig.viewer = 'woman'
  globResult = [
    '/root/man/man1/npm-star.1',
    '/root/man/man1/npm-unstar.1',
  ]
  t.teardown(() => {
    npmConfig.viewer = undefined
    globResult = globDefaults
    spawnBin = null
    spawnArgs = null
  })

  return help(['?(un)star'], (err) => {
    if (err)
      throw err

    t.equal(spawnBin, 'emacsclient', 'maps woman to emacs correctly')
    t.strictSame(spawnArgs, ['-e', `(woman-find-file '/root/man/man1/npm-unstar.1')`], 'passes the correct arguments')
    t.end()
  })
})

test('npm help un*', t => {
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

  return help(['un*'], (err) => {
    if (err)
      throw err

    t.equal(spawnBin, 'man', 'calls man by default')
    t.strictSame(spawnArgs, ['1', 'npm-unstar'], 'passes the correct arguments')
    t.end()
  })
})
