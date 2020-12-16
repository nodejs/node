const { test } = require('tap')
const { resolve } = require('path')
const requireInject = require('require-inject')
const { EventEmitter } = require('events')

let editorBin = null
let editorArgs = null
let editorOpts = null
let EDITOR_CODE = 0
const childProcess = {
  spawn: (bin, args, opts) => {
    // save for assertions
    editorBin = bin
    editorArgs = args
    editorOpts = opts

    const editorEvents = new EventEmitter()
    process.nextTick(() => {
      editorEvents.emit('exit', EDITOR_CODE)
    })
    return editorEvents
  },
}

let rebuildArgs = null
let EDITOR = 'vim'
const npm = {
  config: {
    get: () => EDITOR,
  },
  dir: resolve(__dirname, '../../node_modules'),
  commands: {
    rebuild: (args, cb) => {
      rebuildArgs = args
      return cb()
    },
  },
}

const gracefulFs = require('graceful-fs')
const edit = requireInject('../../lib/edit.js', {
  '../../lib/npm.js': npm,
  child_process: childProcess,
  'graceful-fs': gracefulFs,
})

test('npm edit', t => {
  t.teardown(() => {
    rebuildArgs = null
    editorBin = null
    editorArgs = null
    editorOpts = null
  })

  return edit(['semver'], (err) => {
    if (err)
      throw err

    const path = resolve(__dirname, '../../node_modules/semver')
    t.strictSame(editorBin, EDITOR, 'used the correct editor')
    t.strictSame(editorArgs, [path], 'edited the correct directory')
    t.strictSame(editorOpts, { stdio: 'inherit' }, 'passed the correct opts')
    t.strictSame(rebuildArgs, [path], 'passed the correct path to rebuild')
    t.end()
  })
})

test('npm edit editor has flags', t => {
  EDITOR = 'code -w'
  t.teardown(() => {
    rebuildArgs = null
    editorBin = null
    editorArgs = null
    editorOpts = null
    EDITOR = 'vim'
  })

  return edit(['semver'], (err) => {
    if (err)
      throw err

    const path = resolve(__dirname, '../../node_modules/semver')
    t.strictSame(editorBin, 'code', 'used the correct editor')
    t.strictSame(editorArgs, ['-w', path], 'edited the correct directory, keeping flags')
    t.strictSame(editorOpts, { stdio: 'inherit' }, 'passed the correct opts')
    t.strictSame(rebuildArgs, [path], 'passed the correct path to rebuild')
    t.end()
  })
})

test('npm edit no args', t => {
  return edit([], (err) => {
    t.match(err, /npm edit/, 'throws usage error')
    t.end()
  })
})

test('npm edit lstat error propagates', t => {
  const _lstat = gracefulFs.lstat
  gracefulFs.lstat = (dir, cb) => {
    return cb(new Error('lstat failed'))
  }
  t.teardown(() => {
    gracefulFs.lstat = _lstat
  })

  return edit(['semver'], (err) => {
    t.match(err, /lstat failed/, 'user received correct error')
    t.end()
  })
})

test('npm edit editor exit code error propagates', t => {
  EDITOR_CODE = 137
  t.teardown(() => {
    EDITOR_CODE = 0
  })

  return edit(['semver'], (err) => {
    t.match(err, /exited with code: 137/, 'user received correct error')
    t.end()
  })
})
