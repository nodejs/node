const test = require('tap')
const requireInject = require('require-inject')
const parseJSON = require('json-parse-even-better-errors')

test.test('fails on invalid arguments', (t) => {
  const SetScript = requireInject('../../lib/set-script.js', {
    npmlog: {},
  })
  const setScript = new SetScript({})
  t.plan(3)
  setScript.exec(['arg1'], (fail) => t.match(fail, /Expected 2 arguments: got 1/))
  setScript.exec(['arg1', 'arg2', 'arg3'], (fail) => t.match(fail, /Expected 2 arguments: got 3/))
  setScript.exec(['arg1', 'arg2', 'arg3', 'arg4'], (fail) => t.match(fail, /Expected 2 arguments: got 4/))
})
test.test('fails if run in postinstall script', (t) => {
  const originalVar = process.env.npm_lifecycle_event
  process.env.npm_lifecycle_event = 'postinstall'
  const SetScript = requireInject('../../lib/set-script.js', {
    npmlog: {},
  })
  t.plan(1)
  const setScript = new SetScript({})
  setScript.exec(['arg1', 'arg2'], (fail) => t.equal(fail.toString(), 'Error: Scripts can’t set from the postinstall script'))
  process.env.npm_lifecycle_event = originalVar
})
test.test('fails when package.json not found', (t) => {
  const SetScript = requireInject('../../lib/set-script.js')
  const setScript = new SetScript({})
  t.plan(1)
  setScript.exec(['arg1', 'arg2'], (fail) => t.match(fail, /package.json not found/))
})
test.test('fails on invalid JSON', (t) => {
  const SetScript = requireInject('../../lib/set-script.js', {
    fs: {
      readFile: () => {}, // read-package-json-fast explodes w/o this
      readFileSync: (name, charcode) => {
        return 'iamnotjson'
      },
    },
  })
  const setScript = new SetScript({})
  t.plan(1)
  setScript.exec(['arg1', 'arg2'], (fail) => t.match(fail, /Invalid package.json: JSONParseError/))
})
test.test('creates scripts object', (t) => {
  var mockFile = ''
  const SetScript = requireInject('../../lib/set-script.js', {
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
  const setScript = new SetScript({})
  t.plan(2)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.assert(parseJSON(mockFile), {scripts: {arg1: 'arg2'}})
  })
})
test.test('warns before overwriting', (t) => {
  var warningListened = ''
  const SetScript = requireInject('../../lib/set-script.js', {
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
  const setScript = new SetScript({})
  t.plan(2)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined, 'no error')
    t.equal(warningListened, 'Script "arg1" was overwritten')
  })
})
test.test('provided indentation and eol is used', (t) => {
  var mockFile = ''
  const SetScript = requireInject('../../lib/set-script.js', {
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
  const setScript = new SetScript({})
  t.plan(3)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.equal(mockFile.split('\r\n').length > 1, true)
    t.equal(mockFile.split('\r\n').every((value) => !value.startsWith(' ') || value.startsWith(' '.repeat(6))), true)
  })
})
test.test('goes to default when undefined indent and eol provided', (t) => {
  var mockFile = ''
  const SetScript = requireInject('../../lib/set-script.js', {
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
  const setScript = new SetScript({})
  t.plan(3)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    t.equal(mockFile.split('\n').length > 1, true)
    t.equal(mockFile.split('\n').every((value) => !value.startsWith(' ') || value.startsWith('  ')), true)
  })
})
