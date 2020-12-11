const { test } = require('tap')
const requireInject = require('require-inject')

const { join } = require('path')
const fs = require('fs')
const ansiTrim = require('../../lib/utils/ansi-trim.js')
const isWindows = require('../../lib/utils/is-windows.js')

// getuid and getgid do not exist in windows, so we shim them
// to return 0, as that is the value that lstat will assign the
// gid and uid properties for fs.Stats objects
if (isWindows) {
  process.getuid = () => 0
  process.getgid = () => 0
}

const output = []

let pingError
const ping = async () => {
  if (pingError)
    throw pingError
}

const nodeVersions = [
  { version: 'v14.0.0', lts: false },
  { version: 'v13.0.0', lts: false },
  // it's necessary to allow tests in node 10.x to not mark 12.x as lts
  { version: 'v12.0.0', lts: false },
  { version: 'v10.13.0', lts: 'Dubnium' },
]

const fetch = async () => {
  return {
    json: async () => {
      return nodeVersions
    },
  }
}

const logs = {
  info: [],
}

const clearLogs = (obj = logs) => {
  output.length = 0
  for (const key in obj) {
    if (Array.isArray(obj[key]))
      obj[key].length = 0
    else
      delete obj[key]
  }
}

const npm = {
  flatOptions: {
    registry: 'https://registry.npmjs.org/',
  },
  log: {
    info: (msg) => {
      logs.info.push(msg)
    },
    newItem: (name) => {
      logs[name] = {}

      return {
        info: (_, msg) => {
          if (!logs[name].info)
            logs[name].info = []
          logs[name].info.push(msg)
        },
        warn: (_, msg) => {
          if (!logs[name].warn)
            logs[name].warn = []
          logs[name].warn.push(msg)
        },
        error: (_, msg) => {
          if (!logs[name].error)
            logs[name].error = []
          logs[name].error.push(msg)
        },
        silly: (_, msg) => {
          if (!logs[name].silly)
            logs[name].silly = []
          logs[name].silly.push(msg)
        },
        completeWork: () => {},
        finish: () => {
          logs[name].finished = true
        },
      }
    },
    level: 'error',
    levels: {
      info: 1,
      error: 0,
    },
  },
  version: '7.1.0',
}

let latestNpm = npm.version
const pacote = {
  manifest: async () => {
    return { version: latestNpm }
  },
}

let whichError = null
const which = async () => {
  if (whichError)
    throw whichError
  return '/path/to/git'
}

let verifyResponse = { verifiedCount: 1, verifiedContent: 1 }
const cacache = {
  verify: async () => {
    return verifyResponse
  },
}

const doctor = requireInject('../../lib/doctor.js', {
  '../../lib/utils/is-windows.js': false,
  '../../lib/utils/ping.js': ping,
  '../../lib/utils/output.js': (data) => {
    output.push(data)
  },
  '../../lib/npm.js': npm,
  cacache,
  pacote,
  'make-fetch-happen': fetch,
  which,
})

test('npm doctor checks ok', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    clearLogs()
  })

  doctor([], (err) => {
    if (err) {
      t.fail(output)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor supports silent', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  npm.log.level = 'info'

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    npm.log.level = 'error'
    clearLogs()
  })

  doctor([], (err) => {
    if (err) {
      t.fail(err)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.strictSame(output, [], 'did not print output')
    t.end()
  })
})

test('npm doctor supports color', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  npm.color = true
  pingError = { message: 'generic error' }
  const _consoleError = console.error
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    delete npm.color
    pingError = null
    console.error = _consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the ping error')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping.*not ok/, 'ping output is ok')
    t.match(output, /npm -v.*ok/, 'npm -v output is ok')
    t.match(output, /node -v.*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry.*ok.*using default/, 'npm config get registry output is ok')
    t.match(output, /which git.*ok/, 'which git output is ok')
    t.match(output, /cached files.*ok/, 'cached files are ok')
    t.match(output, /local node_modules.*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules.*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder.*ok/, 'local bin is ok')
    t.match(output, /global bin folder.*ok/, 'global bin is ok')
    t.match(output, /cache contents.*ok/, 'cache contents is ok')
    t.notEqual(output[0], ansiTrim(output[0]), 'output should contain color codes')
    t.end()
  })
})

test('npm doctor skips some tests in windows', t => {
  const winDoctor = requireInject('../../lib/doctor.js', {
    '../../lib/utils/is-windows.js': true,
    '../../lib/utils/ping.js': ping,
    '../../lib/utils/output.js': (data) => {
      output.push(data)
    },
    '../../lib/npm.js': npm,
    cacache,
    pacote,
    'make-fetch-happen': fetch,
    which,
  })

  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    clearLogs()
  })

  winDoctor([], (err) => {
    if (err) {
      t.fail(output)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: undefined,
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor ping error E{3}', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  pingError = { code: 'E111', message: 'this error is 111' }
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    pingError = null
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the ping error')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*not ok\s*111 this error is 111/, 'ping output contains trimmed error')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor generic ping error', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  pingError = { message: 'generic error' }
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    pingError = null
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the ping error')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*not ok\s*generic error/, 'ping output contains trimmed error')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor outdated npm version', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  latestNpm = '7.1.1'
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    latestNpm = npm.version
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the out of date npm')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*not ok/, 'npm -v output is not ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor outdated nodejs version', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  nodeVersions.push({ version: process.version.replace(/\d+$/, '999'), lts: false })
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    nodeVersions.pop()
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the out of date nodejs')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*not ok/, 'node -v output is not ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor file permission checks', t => {
  const dir = t.testdir({
    cache: {
      one: 'one',
      link: t.fixture('symlink', './one'),
      unreadable: 'unreadable',
      baddir: {},
    },
    local: {
      two: 'two',
      notmine: 'notmine',
    },
    global: {
      three: 'three',
      broken: 'broken',
    },
    localBin: {
      four: 'four',
      five: 'five',
    },
    globalBin: {
      six: 'six',
      seven: 'seven',
    },
  })

  const _fsLstat = fs.lstat
  fs.lstat = (p, cb) => {
    let err = null
    let stat = null

    try {
      stat = fs.lstatSync(p)
    } catch (err) {
      return cb(err)
    }

    switch (p) {
      case join(dir, 'local', 'notmine'):
        stat.uid += 1
        stat.gid += 1
        break
      case join(dir, 'global', 'broken'):
        err = new Error('broken')
        break
    }

    return cb(err, stat)
  }

  const _fsReaddir = fs.readdir
  fs.readdir = (p, cb) => {
    let err = null
    let result = null

    try {
      result = fs.readdirSync(p)
    } catch (err) {
      return cb(err)
    }

    if (p === join(dir, 'cache', 'baddir'))
      err = new Error('broken')

    return cb(err, result)
  }

  const _fsAccess = fs.access
  fs.access = (p, mask, cb) => {
    const err = new Error('failed')
    switch (p) {
      case join(dir, 'cache', 'unreadable'):
      case join(dir, 'localBin', 'four'):
      case join(dir, 'globalBin', 'six'):
        return cb(err)
      default:
        return cb(null)
    }
  }

  const doctor = requireInject('../../lib/doctor.js', {
    '../../lib/utils/is-windows.js': false,
    '../../lib/utils/ping.js': ping,
    '../../lib/utils/output.js': (data) => {
      output.push(data)
    },
    '../../lib/npm.js': npm,
    cacache,
    pacote,
    'make-fetch-happen': fetch,
    which,
    fs,
  })
  // it's necessary to allow tests in node 10.x to not mark 12.x as lted

  npm.cache = npm.flatOptions.cache = join(dir, 'cache')
  npm.localDir = join(dir, 'local')
  npm.globalDir = join(dir, 'global')
  npm.localBin = join(dir, 'localBin')
  npm.globalBin = join(dir, 'globalBin')
  const _consoleError = console.error
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    console.error = _consoleError
    fs.lstat = _fsLstat
    fs.readdir = _fsReaddir
    fs.access = _fsAccess
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'identified problems')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [join(dir, 'cache')]: { finished: true },
      [join(dir, 'local')]: { finished: true },
      [join(dir, 'global')]: { finished: true },
      [join(dir, 'localBin')]: { finished: true },
      [join(dir, 'globalBin')]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*not ok/, 'cached files are not ok')
    t.match(output, /local node_modules\s*not ok/, 'local node_modules are not ok')
    t.match(output, /global node_modules\s*not ok/, 'global node_modules are not ok')
    t.match(output, /local bin folder\s*not ok/, 'local bin is not ok')
    t.match(output, /global bin folder\s*not ok/, 'global bin is not ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor missing git', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  whichError = new Error('boom')
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    whichError = null
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    t.match(err, /Some problems found/, 'detected the missing git')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*not ok/, 'which git output is not ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})

test('npm doctor cache verification showed bad content', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  const _verifyResponse = verifyResponse
  verifyResponse = {
    ...verifyResponse,
    badContentCount: 1,
  }
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    verifyResponse = _verifyResponse
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    // cache verification problems get fixed and so do not throw an error
    if (err) {
      t.fail(output)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is not ok')
    t.end()
  })
})

test('npm doctor cache verification showed reclaimed content', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  const _verifyResponse = verifyResponse
  verifyResponse = {
    ...verifyResponse,
    reclaimedCount: 1,
    reclaimedSize: 100,
  }
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    verifyResponse = _verifyResponse
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    // cache verification problems get fixed and so do not throw an error
    if (err) {
      t.fail(output)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is not ok')
    t.end()
  })
})

test('npm doctor cache verification showed missing content', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  const _verifyResponse = verifyResponse
  verifyResponse = {
    ...verifyResponse,
    missingContent: 1,
  }
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    verifyResponse = _verifyResponse
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    // cache verification problems get fixed and so do not throw an error
    if (err) {
      t.fail(output)
      return t.end()
    }

    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*ok\s*using default/, 'npm config get registry output is ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is not ok')
    t.end()
  })
})

test('npm doctor not using default registry', t => {
  const dir = t.testdir()
  npm.cache = npm.flatOptions.cache = dir
  npm.localDir = dir
  npm.globalDir = dir
  npm.localBin = dir
  npm.globalBin = dir
  const _currentRegistry = npm.flatOptions.registry
  npm.flatOptions.registry = 'https://google.com'
  const consoleError = console.error
  // we just print an empty line here, so swallow it and ignore
  console.error = () => {}

  t.teardown(() => {
    delete npm.cache
    delete npm.flatOptions.cache
    delete npm.localDir
    delete npm.globalDir
    delete npm.localBin
    delete npm.globalBin
    npm.flatOptions.registry = _currentRegistry
    console.error = consoleError
    clearLogs()
  })

  doctor([], (err) => {
    // cache verification problems get fixed and so do not throw an error
    t.match(err, /Some problems found/, 'detected the non-default registry')
    t.match(logs, {
      checkPing: { finished: true },
      getLatestNpmVersion: { finished: true },
      getLatestNodejsVersion: { finished: true },
      getGitPath: { finished: true },
      [dir]: { finished: true },
      verifyCachedFiles: { finished: true },
    }, 'trackers all finished')
    t.match(output, /npm ping\s*ok/, 'ping output is ok')
    t.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    t.match(output, /node -v\s*ok/, 'node -v output is ok')
    t.match(output, /npm config get registry\s*not ok/, 'npm config get registry output is not ok')
    t.match(output, /which git\s*ok/, 'which git output is ok')
    t.match(output, /cached files\s*ok/, 'cached files are ok')
    t.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    t.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    t.match(output, /local bin folder\s*ok/, 'local bin is ok')
    t.match(output, /global bin folder\s*ok/, 'global bin is ok')
    t.match(output, /cache contents\s*ok/, 'cache contents is ok')
    t.end()
  })
})
