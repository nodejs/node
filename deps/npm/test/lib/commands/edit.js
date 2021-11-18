const t = require('tap')
const { resolve } = require('path')
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
let rebuildFail = null
let EDITOR = 'vim'
const npm = {
  config: {
    get: () => EDITOR,
  },
  dir: resolve(__dirname, '../../../node_modules'),
  exec: async (cmd, args) => {
    rebuildArgs = args
    if (rebuildFail) {
      throw rebuildFail
    }
  },
}

const gracefulFs = require('graceful-fs')
const Edit = t.mock('../../../lib/commands/edit.js', {
  child_process: childProcess,
  'graceful-fs': gracefulFs,
})
const edit = new Edit(npm)

t.test('npm edit', async t => {
  t.teardown(() => {
    rebuildArgs = null
    editorBin = null
    editorArgs = null
    editorOpts = null
  })

  await edit.exec(['semver'])
  const path = resolve(__dirname, '../../../node_modules/semver')
  t.strictSame(editorBin, EDITOR, 'used the correct editor')
  t.strictSame(editorArgs, [path], 'edited the correct directory')
  t.strictSame(editorOpts, { stdio: 'inherit' }, 'passed the correct opts')
  t.strictSame(rebuildArgs, [path], 'passed the correct path to rebuild')
})

t.test('rebuild fails', async t => {
  t.teardown(() => {
    rebuildFail = null
    rebuildArgs = null
    editorBin = null
    editorArgs = null
    editorOpts = null
  })

  rebuildFail = new Error('test error')
  await t.rejects(
    edit.exec(['semver']),
    { message: 'test error' }
  )
  const path = resolve(__dirname, '../../../node_modules/semver')
  t.strictSame(editorBin, EDITOR, 'used the correct editor')
  t.strictSame(editorArgs, [path], 'edited the correct directory')
  t.strictSame(editorOpts, { stdio: 'inherit' }, 'passed the correct opts')
  t.strictSame(rebuildArgs, [path], 'passed the correct path to rebuild')
})

t.test('npm edit editor has flags', async t => {
  EDITOR = 'code -w'
  t.teardown(() => {
    rebuildArgs = null
    editorBin = null
    editorArgs = null
    editorOpts = null
    EDITOR = 'vim'
  })

  await edit.exec(['semver'])

  const path = resolve(__dirname, '../../../node_modules/semver')
  t.strictSame(editorBin, 'code', 'used the correct editor')
  t.strictSame(editorArgs, ['-w', path], 'edited the correct directory, keeping flags')
  t.strictSame(editorOpts, { stdio: 'inherit' }, 'passed the correct opts')
  t.strictSame(rebuildArgs, [path], 'passed the correct path to rebuild')
})

t.test('npm edit no args', async t => {
  await t.rejects(
    edit.exec([]),
    /npm edit/,
    'throws usage error'
  )
})

t.test('npm edit lstat error propagates', async t => {
  const _lstat = gracefulFs.lstat
  gracefulFs.lstat = (dir, cb) => {
    return cb(new Error('lstat failed'))
  }
  t.teardown(() => {
    gracefulFs.lstat = _lstat
  })

  await t.rejects(
    edit.exec(['semver']),
    /lstat failed/,
    'user received correct error'
  )
})

t.test('npm edit editor exit code error propagates', async t => {
  EDITOR_CODE = 137
  t.teardown(() => {
    EDITOR_CODE = 0
  })

  await t.rejects(
    edit.exec(['semver']),
    /exited with code: 137/,
    'user received correct error'
  )
})
