const t = require('tap')
const requireInject = require('require-inject')

// have to fake the node version, or else it'll only pass on this one
Object.defineProperty(process, 'version', {
  value: 'v14.8.0',
})

t.formatSnapshot = obj => {
  if (typeof obj !== 'object' || !obj || !obj.types)
    return obj

  return {
    ...obj,
    defaults: {
      ...obj.defaults,
      cache: '{CACHE DIR} ' + path.basename(obj.defaults.cache),
    },
    types: formatTypes(obj.types),
  }
}

const path = require('path')
const url = require('url')
const semver = require('semver')

const formatTypes = (types) => Object.entries(types).map(([key, value]) => {
  return [key, formatTypeValue(value)]
}).reduce((set, [key, value]) => {
  set[key] = value
  return set
}, {})

const formatTypeValue = (value) => {
  if (Array.isArray(value))
    return value.map(formatTypeValue)
  else if (value === url)
    return '{URL MODULE}'
  else if (value === path)
    return '{PATH MODULE}'
  else if (value === semver)
    return '{SEMVER MODULE}'
  else if (typeof value === 'function')
    return `{${value.name} TYPE}`
  else
    return value
}

process.env.ComSpec = 'cmd.exe'
process.env.SHELL = '/usr/local/bin/bash'
process.env.LANG = 'UTF-8'
delete process.env.LC_ALL
delete process.env.LC_CTYPE
process.env.EDITOR = 'vim'
process.env.VISUAL = 'mate'

const networkInterfacesThrow = () => {
  throw new Error('no network interfaces for some reason')
}
const networkInterfaces = () => ({
  eth420: [{ address: '127.0.0.1' }],
  eth69: [{ address: 'no place like home' }],
})
const tmpdir = () => '/tmp'
const os = { networkInterfaces, tmpdir }
const pkg = { version: '7.0.0' }

t.test('working network interfaces, not windows', t => {
  const config = requireInject('../../../lib/utils/config.js', {
    os,
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': false,
    '../../../package.json': pkg,
  })
  t.matchSnapshot(config)
  t.end()
})

t.test('no working network interfaces, on windows', t => {
  const config = requireInject('../../../lib/utils/config.js', {
    os: { tmpdir, networkInterfaces: networkInterfacesThrow },
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': true,
    '../../../package.json': pkg,
  })
  t.matchSnapshot(config)
  t.end()
})

t.test('no comspec on windows', t => {
  delete process.env.ComSpec
  const config = requireInject('../../../lib/utils/config.js', {
    os: { tmpdir, networkInterfaces: networkInterfacesThrow },
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': true,
  })
  t.equal(config.defaults.shell, 'cmd')
  t.end()
})

t.test('no shell on posix', t => {
  delete process.env.SHELL
  const config = requireInject('../../../lib/utils/config.js', {
    os,
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': false,
  })
  t.equal(config.defaults.shell, 'sh')
  t.end()
})

t.test('no EDITOR env, use VISUAL', t => {
  delete process.env.EDITOR
  const config = requireInject('../../../lib/utils/config.js', {
    os,
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': false,
  })
  t.equal(config.defaults.editor, 'mate')
  t.end()
})

t.test('no VISUAL, use system default, not windows', t => {
  delete process.env.VISUAL
  const config = requireInject('../../../lib/utils/config.js', {
    os,
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': false,
  })
  t.equal(config.defaults.editor, 'vi')
  t.end()
})

t.test('no VISUAL, use system default, not windows', t => {
  delete process.env.VISUAL
  const config = requireInject('../../../lib/utils/config.js', {
    os,
    '@npmcli/ci-detect': () => false,
    '../../../lib/utils/is-windows.js': true,
  })
  t.equal(config.defaults.editor, 'notepad.exe')
  t.end()
})
