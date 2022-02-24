const t = require('tap')

const { join } = require('path')
const fs = require('fs')
const ansiTrim = require('../../../lib/utils/ansi-trim.js')
const isWindows = require('../../../lib/utils/is-windows.js')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

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
  if (pingError) {
    throw pingError
  }
}

let whichError = null
const which = async () => {
  if (whichError) {
    throw whichError
  }
  return '/path/to/git'
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

const clearLogs = () => {
  output.length = 0
  for (const key in logs) {
    if (Array.isArray(logs[key])) {
      logs[key].length = 0
    } else {
      delete logs[key]
    }
  }
}

const npm = mockNpm({
  flatOptions: {
    registry: 'https://registry.npmjs.org/',
  },
  config: {
    loglevel: 'info',
  },
  version: '7.1.0',
  output: data => {
    output.push(data)
  },
})

let latestNpm = npm.version
const pacote = {
  manifest: async () => {
    return { version: latestNpm }
  },
}

let verifyResponse = { verifiedCount: 1, verifiedContent: 1 }
const cacache = {
  verify: async () => {
    return verifyResponse
  },
}

const mocks = {
  '../../../lib/utils/is-windows.js': false,
  '../../../lib/utils/ping.js': ping,
  cacache,
  pacote,
  'make-fetch-happen': fetch,
  which,
  'proc-log': {
    info: msg => {
      logs.info.push(msg)
    },
  },
  npmlog: {
    newItem: name => {
      logs[name] = {}
      return {
        info: (_, msg) => {
          if (!logs[name].info) {
            logs[name].info = []
          }
          logs[name].info.push(msg)
        },
        warn: (_, msg) => {
          if (!logs[name].warn) {
            logs[name].warn = []
          }
          logs[name].warn.push(msg)
        },
        error: (_, msg) => {
          if (!logs[name].error) {
            logs[name].error = []
          }
          logs[name].error.push(msg)
        },
        silly: (_, msg) => {
          if (!logs[name].silly) {
            logs[name].silly = []
          }
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

}

const Doctor = t.mock('../../../lib/commands/doctor.js', {
  ...mocks,
})
const doctor = new Doctor(npm)

const origVersion = process.version
t.test('node versions', t => {
  t.plan(nodeVersions.length)

  nodeVersions.forEach(({ version }) => {
    t.test(`${version}:`, vt => {
      Object.defineProperty(process, 'version', { value: version })
      vt.teardown(() => {
        Object.defineProperty(process, 'version', { value: origVersion })
      })

      vt.test(`${version}: npm doctor checks ok`, async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir

        st.teardown(() => {
          delete npm.cache
          delete npm.flatOptions.cache
          delete npm.localDir
          delete npm.globalDir
          delete npm.localBin
          delete npm.globalBin
          clearLogs()
        })

        await doctor.exec([])
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor supports silent', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        npm.config.set('loglevel', 'silent')

        st.teardown(() => {
          delete npm.cache
          delete npm.flatOptions.cache
          delete npm.localDir
          delete npm.globalDir
          delete npm.localBin
          delete npm.globalBin
          npm.config.set('loglevel', 'info')
          clearLogs()
        })

        await doctor.exec([])

        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.strictSame(output, [], 'did not print output')
      })

      vt.test('npm doctor supports color', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        npm.color = true
        pingError = { message: 'generic error' }
        const _consoleError = console.error
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'detected the ping error')
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping.*not ok/, 'ping output is ok')
        st.match(output, /npm -v.*ok/, 'npm -v output is ok')
        st.match(output, /node -v.*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry.*ok.*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git.*ok/, 'which git output is ok')
        st.match(output, /cached files.*ok/, 'cached files are ok')
        st.match(output, /local node_modules.*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules.*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder.*ok/, 'local bin is ok')
        st.match(output, /global bin folder.*ok/, 'global bin is ok')
        st.match(output, /cache contents.*ok/, 'cache contents is ok')
        st.not(output[0], ansiTrim(output[0]), 'output should contain color codes')
      })

      vt.test('npm doctor skips some tests in windows', async st => {
        const WinDoctor = t.mock('../../../lib/commands/doctor.js', {
          ...mocks,
          '../../../lib/utils/is-windows.js': true,
        })
        const winDoctor = new WinDoctor(npm)

        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir

        st.teardown(() => {
          delete npm.cache
          delete npm.flatOptions.cache
          delete npm.localDir
          delete npm.globalDir
          delete npm.localBin
          delete npm.globalBin
          clearLogs()
        })

        await winDoctor.exec([])
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: undefined,
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor ping error E{3}', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        pingError = { code: 'E111', message: 'this error is 111' }
        const consoleError = console.error
        // we just print an empty line here, so swallow it and ignore
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'detected the ping error')
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(
          output,
          /npm ping\s*not ok\s*111 this error is 111/,
          'ping output contains trimmed error'
        )
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor generic ping error', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        pingError = { message: 'generic error' }
        const consoleError = console.error
        // we just print an empty line here, so swallow it and ignore
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'detected the ping error')
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*not ok\s*generic error/, 'ping output contains trimmed error')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor outdated npm version', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        latestNpm = '7.1.1'
        const consoleError = console.error
        // we just print an empty line here, so swallow it and ignore
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'detected the out of date npm')
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*not ok/, 'npm -v output is not ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor file permission checks', async st => {
        const dir = st.testdir({
          cache: {
            one: 'one',
            link: st.fixture('symlink', './baddir'),
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

          if (p === join(dir, 'cache', 'baddir')) {
            err = new Error('broken')
          }

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

        const Doctor = t.mock('../../../lib/commands/doctor.js', {
          ...mocks,
          fs,
        })
        const doctor = new Doctor(npm)
        // it's necessary to allow tests in node 10.x to not mark 12.x as lted

        npm.cache = npm.flatOptions.cache = join(dir, 'cache')
        npm.localDir = join(dir, 'local')
        npm.globalDir = join(dir, 'global')
        npm.localBin = join(dir, 'localBin')
        npm.globalBin = join(dir, 'globalBin')
        const _consoleError = console.error
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'identified problems')
        st.match(
          logs,
          {
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
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*not ok/, 'cached files are not ok')
        st.match(output, /local node_modules\s*not ok/, 'local node_modules are not ok')
        st.match(output, /global node_modules\s*not ok/, 'global node_modules are not ok')
        st.match(output, /local bin folder\s*not ok/, 'local bin is not ok')
        st.match(output, /global bin folder\s*not ok/, 'global bin is not ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor missing git', async st => {
        const dir = st.testdir()
        npm.cache = npm.flatOptions.cache = dir
        npm.localDir = dir
        npm.globalDir = dir
        npm.localBin = dir
        npm.globalBin = dir
        whichError = new Error('boom')
        const consoleError = console.error
        // we just print an empty line here, so swallow it and ignore
        console.error = () => {}

        st.teardown(() => {
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

        await st.rejects(doctor.exec([]), /Some problems found/, 'detected the missing git')
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*not ok/, 'which git output is not ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.test('npm doctor cache verification showed bad content', async st => {
        const dir = st.testdir()
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

        st.teardown(() => {
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

        // cache verification problems get fixed and so do not throw an error
        await doctor.exec([])
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is not ok')
      })

      vt.test('npm doctor cache verification showed reclaimed content', async st => {
        const dir = st.testdir()
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

        st.teardown(() => {
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

        // cache verification problems get fixed and so do not throw an error
        await doctor.exec([])

        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is not ok')
      })

      vt.test('npm doctor cache verification showed missing content', async st => {
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

        st.teardown(() => {
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

        // cache verification problems get fixed and so do not throw an error
        await doctor.exec([])
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*ok\s*using default/,
          'npm config get registry output is ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is not ok')
      })

      vt.test('npm doctor not using default registry', async st => {
        const dir = st.testdir()
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

        st.teardown(() => {
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

        await st.rejects(
          doctor.exec([]),
          /Some problems found/,
          'detected the non-default registry'
        )
        st.match(
          logs,
          {
            checkPing: { finished: true },
            getLatestNpmVersion: { finished: true },
            getLatestNodejsVersion: { finished: true },
            getGitPath: { finished: true },
            [dir]: { finished: true },
            verifyCachedFiles: { finished: true },
          },
          'trackers all finished'
        )
        st.match(output, /npm ping\s*ok/, 'ping output is ok')
        st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
        st.match(output, /node -v\s*ok/, 'node -v output is ok')
        st.match(
          output,
          /npm config get registry\s*not ok/,
          'npm config get registry output is not ok'
        )
        st.match(output, /which git\s*ok/, 'which git output is ok')
        st.match(output, /cached files\s*ok/, 'cached files are ok')
        st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
        st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
        st.match(output, /local bin folder\s*ok/, 'local bin is ok')
        st.match(output, /global bin folder\s*ok/, 'global bin is ok')
        st.match(output, /cache contents\s*ok/, 'cache contents is ok')
      })

      vt.end()
    })
  })
})

t.test('outdated node version', vt => {
  vt.plan(1)
  const version = 'v10.0.0'

  Object.defineProperty(process, 'version', { value: version })
  vt.teardown(() => {
    Object.defineProperty(process, 'version', { value: origVersion })
  })

  vt.test('npm doctor outdated nodejs version', async st => {
    const dir = st.testdir()
    npm.cache = npm.flatOptions.cache = dir
    npm.localDir = dir
    npm.globalDir = dir
    npm.localBin = dir
    npm.globalBin = dir
    nodeVersions.push({ version: process.version.replace(/\d+(-.*)?$/, '999'), lts: false })
    const consoleError = console.error
    // we just print an empty line here, so swallow it and ignore
    console.error = () => {}

    st.teardown(() => {
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

    await st.rejects(doctor.exec([]), /Some problems found/, 'detected the out of date nodejs')
    st.match(
      logs,
      {
        checkPing: { finished: true },
        getLatestNpmVersion: { finished: true },
        getLatestNodejsVersion: { finished: true },
        getGitPath: { finished: true },
        [dir]: { finished: true },
        verifyCachedFiles: { finished: true },
      },
      'trackers all finished'
    )
    st.match(output, /npm ping\s*ok/, 'ping output is ok')
    st.match(output, /npm -v\s*ok/, 'npm -v output is ok')
    st.match(output, /node -v\s*not ok/, 'node -v output is not ok')
    st.match(
      output,
      /npm config get registry\s*ok\s*using default/,
      'npm config get registry output is ok'
    )
    st.match(output, /which git\s*ok/, 'which git output is ok')
    st.match(output, /cached files\s*ok/, 'cached files are ok')
    st.match(output, /local node_modules\s*ok/, 'local node_modules are ok')
    st.match(output, /global node_modules\s*ok/, 'global node_modules are ok')
    st.match(output, /local bin folder\s*ok/, 'local bin is ok')
    st.match(output, /global bin folder\s*ok/, 'global bin is ok')
    st.match(output, /cache contents\s*ok/, 'cache contents is ok')
  })
})
