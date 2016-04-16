'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var writeFileAtomic = requireInject('../index', {
  'graceful-fs': {
    writeFile: function (tmpfile, data, options, cb) {
      if (/nowrite/.test(tmpfile)) return cb(new Error('ENOWRITE'))
      cb()
    },
    chown: function (tmpfile, uid, gid, cb) {
      if (/nochown/.test(tmpfile)) return cb(new Error('ENOCHOWN'))
      cb()
    },
    rename: function (tmpfile, filename, cb) {
      if (/norename/.test(tmpfile)) return cb(new Error('ENORENAME'))
      cb()
    },
    unlink: function (tmpfile, cb) {
      if (/nounlink/.test(tmpfile)) return cb(new Error('ENOUNLINK'))
      cb()
    },
    writeFileSync: function (tmpfile, data, options) {
      if (/nowrite/.test(tmpfile)) throw new Error('ENOWRITE')
    },
    chownSync: function (tmpfile, uid, gid) {
      if (/nochown/.test(tmpfile)) throw new Error('ENOCHOWN')
    },
    renameSync: function (tmpfile, filename) {
      if (/norename/.test(tmpfile)) throw new Error('ENORENAME')
    },
    unlinkSync: function (tmpfile) {
      if (/nounlink/.test(tmpfile)) throw new Error('ENOUNLINK')
    }
  }
})
var writeFileAtomicSync = writeFileAtomic.sync

test('async tests', function (t) {
  t.plan(7)
  writeFileAtomic('good', 'test', {mode: '0777'}, function (err) {
    t.notOk(err, 'No errors occur when passing in options')
  })
  writeFileAtomic('good', 'test', function (err) {
    t.notOk(err, 'No errors occur when NOT passing in options')
  })
  writeFileAtomic('nowrite', 'test', function (err) {
    t.is(err.message, 'ENOWRITE', 'writeFile failures propagate')
  })
  writeFileAtomic('nochown', 'test', {chown: {uid: 100, gid: 100}}, function (err) {
    t.is(err.message, 'ENOCHOWN', 'Chown failures propagate')
  })
  writeFileAtomic('nochown', 'test', function (err) {
    t.notOk(err, 'No attempt to chown when no uid/gid passed in')
  })
  writeFileAtomic('norename', 'test', function (err) {
    t.is(err.message, 'ENORENAME', 'Rename errors propagate')
  })
  writeFileAtomic('norename nounlink', 'test', function (err) {
    t.is(err.message, 'ENORENAME', 'Failure to unlink the temp file does not clobber the original error')
  })
})

test('sync tests', function (t) {
  t.plan(7)
  var throws = function (shouldthrow, msg, todo) {
    var err
    try { todo() } catch (e) { err = e }
    t.is(shouldthrow, err.message, msg)
  }
  var noexception = function (msg, todo) {
    var err
    try { todo() } catch (e) { err = e }
    t.notOk(err, msg)
  }

  noexception('No errors occur when passing in options', function () {
    writeFileAtomicSync('good', 'test', {mode: '0777'})
  })
  noexception('No errors occur when NOT passing in options', function () {
    writeFileAtomicSync('good', 'test')
  })
  throws('ENOWRITE', 'writeFile failures propagate', function () {
    writeFileAtomicSync('nowrite', 'test')
  })
  throws('ENOCHOWN', 'Chown failures propagate', function () {
    writeFileAtomicSync('nochown', 'test', {chown: {uid: 100, gid: 100}})
  })
  noexception('No attempt to chown when no uid/gid passed in', function () {
    writeFileAtomicSync('nochown', 'test')
  })
  throws('ENORENAME', 'Rename errors propagate', function () {
    writeFileAtomicSync('norename', 'test')
  })
  throws('ENORENAME', 'Failure to unlink the temp file does not clobber the original error', function () {
    writeFileAtomicSync('norename nounlink', 'test')
  })
})
