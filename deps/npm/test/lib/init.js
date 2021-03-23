const t = require('tap')
const requireInject = require('require-inject')
const mockNpm = require('../fixtures/mock-npm')

let result = ''
const npmLog = {
  disableProgress: () => null,
  enableProgress: () => null,
  info: () => null,
  pause: () => null,
  resume: () => null,
  silly: () => null,
}
const config = {
  'init-module': '~/.npm-init.js',
}
const npm = mockNpm({
  config,
  log: npmLog,
  commands: {},
  output: (...msg) => {
    result += msg.join('\n')
  },
})
const mocks = {
  'init-package-json': (dir, initFile, config, cb) => cb(null, 'data'),
  '../../lib/utils/usage.js': () => 'usage instructions',
}
const Init = requireInject('../../lib/init.js', mocks)
const init = new Init(npm)

t.afterEach(cb => {
  result = ''
  config.package = undefined
  npm.commands = {}
  npm.log = npmLog
  cb()
})

t.test('classic npm init no args', t => {
  init.exec([], err => {
    t.ifError(err, 'npm init no args')
    t.matchSnapshot(result, 'should print helper info')
    t.end()
  })
})

t.test('classic npm init -y', t => {
  t.plan(7)
  config.yes = true
  Object.defineProperty(npm, 'flatOptions', { value: { yes: true} })
  npm.log = { ...npm.log }
  npm.log.silly = (title, msg) => {
    t.equal(title, 'package data', 'should print title')
    t.equal(msg, 'data', 'should print pkg data info')
  }
  npm.log.resume = () => {
    t.ok('should resume logs')
  }
  npm.log.info = (title, msg) => {
    t.equal(title, 'init', 'should print title')
    t.equal(msg, 'written successfully', 'should print done info')
  }
  init.exec([], err => {
    t.ifError(err, 'npm init -y')
    t.equal(result, '')
  })
})

t.test('npm init <arg>', t => {
  t.plan(3)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(config.package, [], 'should set empty array value')
    t.deepEqual(
      arr,
      ['create-react-app'],
      'should npx with listed packages'
    )
    cb()
  }
  init.exec(['react-app'], err => {
    t.ifError(err, 'npm init react-app')
  })
})

t.test('npm init @scope/name', t => {
  t.plan(2)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(
      arr,
      ['@npmcli/create-something'],
      'should npx with scoped packages'
    )
    cb()
  }
  init.exec(['@npmcli/something'], err => {
    t.ifError(err, 'npm init init @scope/name')
  })
})

t.test('npm init git spec', t => {
  t.plan(2)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(
      arr,
      ['npm/create-something'],
      'should npx with git-spec packages'
    )
    cb()
  }
  init.exec(['npm/something'], err => {
    t.ifError(err, 'npm init init @scope/name')
  })
})

t.test('npm init @scope', t => {
  t.plan(2)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(
      arr,
      ['@npmcli/create'],
      'should npx with @scope/create pkgs'
    )
    cb()
  }
  init.exec(['@npmcli'], err => {
    t.ifError(err, 'npm init init @scope/create')
  })
})

t.test('npm init tgz', t => {
  init.exec(['something.tgz'], err => {
    t.match(
      err,
      /Error: Unrecognized initializer: something.tgz/,
      'should throw error when using an unsupported spec'
    )
    t.end()
  })
})

t.test('npm init <arg>@next', t => {
  t.plan(2)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(
      arr,
      ['create-something@next'],
      'should npx with something@next'
    )
    cb()
  }
  init.exec(['something@next'], err => {
    t.ifError(err, 'npm init init something@next')
  })
})

t.test('npm init exec error', t => {
  npm.commands.exec = (arr, cb) => {
    cb(new Error('ERROR'))
  }
  init.exec(['something@next'], err => {
    t.match(
      err,
      /ERROR/,
      'should exit with exec error'
    )
    t.end()
  })
})

t.test('should not rewrite flatOptions', t => {
  t.plan(3)
  npm.commands.exec = (arr, cb) => {
    t.deepEqual(config.package, [], 'should set empty array value')
    t.deepEqual(
      arr,
      ['create-react-app', 'my-app'],
      'should npx with extra args'
    )
    cb()
  }
  init.exec(['react-app', 'my-app'], err => {
    t.ifError(err, 'npm init react-app')
  })
})

t.test('npm init cancel', t => {
  t.plan(3)
  const Init = requireInject('../../lib/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('canceled')
    ),
  })
  const init = new Init(npm)
  npm.log = { ...npm.log }
  npm.log.warn = (title, msg) => {
    t.equal(title, 'init', 'should have init title')
    t.equal(msg, 'canceled', 'should log canceled')
  }
  init.exec([], err => {
    t.ifError(err, 'npm init cancel')
  })
})

t.test('npm init error', t => {
  const Init = requireInject('../../lib/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('Unknown Error')
    ),
  })
  const init = new Init(npm)
  init.exec([], err => {
    t.match(err, /Unknown Error/, 'should throw error')
    t.end()
  })
})
