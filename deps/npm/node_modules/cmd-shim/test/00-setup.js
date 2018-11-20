var test = require('tap').test
var mkdirp = require('mkdirp')
var fs = require('fs')
var path = require('path')
var fixtures = path.resolve(__dirname, 'fixtures')

var froms = {
  'from.exe': 'exe',
  'from.env': '#!/usr/bin/env node\nconsole.log(/hi/)\n',
  'from.env.args': '#!/usr/bin/env node --expose_gc\ngc()\n',
  'from.sh': '#!/usr/bin/sh\necho hi\n',
  'from.sh.args': '#!/usr/bin/sh -x\necho hi\n'
}

var cmdShim = require('../')

test('create fixture', function (t) {
  mkdirp(fixtures, function (er) {
    if (er)
      throw er
    t.pass('made dir')
    Object.keys(froms).forEach(function (f) {
      t.test('write ' + f, function (t) {
        fs.writeFile(path.resolve(fixtures, f), froms[f], function (er) {
          if (er)
            throw er
          t.pass('wrote ' + f)
          t.end()
        })
      })
    })
    t.end()
  })
})
