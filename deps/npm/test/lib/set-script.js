const test = require('tap')
const requireInject = require('require-inject')
const setScriptDefault = require('../../lib/set-script.js')
const parseJSON = require('json-parse-even-better-errors')

test.type(setScriptDefault, 'function', 'command is function')
test.equal(setScriptDefault.usage, 'npm set-script [<script>] [<command>]', 'usage matches')
test.test('fails on invalid arguments', (t) => {
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {},
    npmlog: {},
  })
  t.plan(3)
  setScript(['arg1'], (fail) => t.match(fail, /Expected 2 arguments: got 1/))
  setScript(['arg1', 'arg2', 'arg3'], (fail) => t.match(fail, /Expected 2 arguments: got 3/))
  setScript(['arg1', 'arg2', 'arg3', 'arg4'], (fail) => t.match(fail, /Expected 2 arguments: got 4/))
})
test.test('fails if run in postinstall script', (t) => {
  var originalVar = process.env.npm_lifecycle_event
  process.env.npm_lifecycle_event = 'postinstall'
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {},
    npmlog: {},
  })
  t.plan(1)
  setScript(['arg1', 'arg2'], (fail) => t.equal(fail.toString(), 'Error: Scripts can’t set from the postinstall script'))
  process.env.npm_lifecycle_event = originalVar
})
test.test('fails when package.json not found', (t) => {
  const setScript = requireInject('../../lib/set-script.js', {
    '../../lib/npm.js': {
      localPrefix: 'IDONTEXIST',
    },
  })
  t.plan(1)
  setScript(['arg1', 'arg2'], (fail) => t.match(fail, /package.json not found/))
})
test.test('fails on invalid JSON', (t) => {
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFileSync: (name, charcode) => {
        return 'iamnotjson'
      },
    },
  })
  t.plan(1)
  setScript(['arg1', 'arg2'], (fail) => t.match(fail, /Invalid package.json: JSONParseError/))
})
test.test('creates scripts object', (t) => {
  var mockFile = ''
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFileSync: (name, charcode) => {
        return '{}'
      },
      writeFileSync: (location, inner) => {
        mockFile = inner
      },
    },
    'read-package-json-fast': async function (filename) {
      return {
        [Symbol.for('indent')]: '  ',
        [Symbol.for('newline')]: '\n',
      }
    },
  })
  t.plan(2)
  setScript(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.assert(parseJSON(mockFile), {scripts: {arg1: 'arg2'}})
  })
})
test.test('warns before overwriting', (t) => {
  var warningListened = ''
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFileSync: (name, charcode) => {
        return JSON.stringify({
          scripts: {
            arg1: 'blah',
          },
        })
      },
      writeFileSync: (name, content) => {},
    },
    'read-package-json-fast': async function (filename) {
      return {
        [Symbol.for('indent')]: '  ',
        [Symbol.for('newline')]: '\n',
      }
    },
    npmlog: {
      warn: (prefix, message) => {
        warningListened = message
      },
    },
  })
  t.plan(2)
  setScript(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined, 'no error')
    t.equal(warningListened, 'Script "arg1" was overwritten')
  })
})
test.test('provided indentation and eol is used', (t) => {
  var mockFile = ''
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFileSync: (name, charcode) => {
        return '{}'
      },
      writeFileSync: (name, content) => {
        mockFile = content
      },
    },
    'read-package-json-fast': async function (filename) {
      return {
        [Symbol.for('indent')]: ' '.repeat(6),
        [Symbol.for('newline')]: '\r\n',
      }
    },
  })
  t.plan(3)
  setScript(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.equal(mockFile.split('\r\n').length > 1, true)
    t.equal(mockFile.split('\r\n').every((value) => !value.startsWith(' ') || value.startsWith(' '.repeat(6))), true)
  })
})
test.test('goes to default when undefined indent and eol provided', (t) => {
  var mockFile = ''
  const setScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFileSync: (name, charcode) => {
        return '{}'
      },
      writeFileSync: (name, content) => {
        mockFile = content
      },
    },
    'read-package-json-fast': async function (filename) {
      return {
        [Symbol.for('indent')]: undefined,
        [Symbol.for('newline')]: undefined,
      }
    },
  })
  t.plan(3)
  setScript(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.equal(mockFile.split('\n').length > 1, true)
    t.equal(mockFile.split('\n').every((value) => !value.startsWith(' ') || value.startsWith('  ')), true)
  })
})
