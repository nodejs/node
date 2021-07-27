// npm edit <pkg>
// open the package folder in the $EDITOR

module.exports = edit
edit.usage = 'npm edit <pkg>[/<subpkg>...]'

edit.completion = require('./utils/completion/installed-shallow.js')

var npm = require('./npm.js')
var path = require('path')
var fs = require('graceful-fs')
var editor = require('editor')
var noProgressTillDone = require('./utils/no-progress-while-running').tillDone

function edit (args, cb) {
  var p = args[0]
  if (args.length !== 1 || !p) return cb(edit.usage)
  var e = npm.config.get('editor')
  if (!e) {
    return cb(new Error(
      "No editor set.  Set the 'editor' config, or $EDITOR environ."
    ))
  }
  p = p.split('/')
    // combine scoped parts
    .reduce(function (parts, part) {
      if (parts.length === 0) {
        return [part]
      }
      var lastPart = parts[parts.length - 1]
      // check if previous part is the first part of a scoped package
      if (lastPart[0] === '@' && !lastPart.includes('/')) {
        parts[parts.length - 1] += '/' + part
      } else {
        parts.push(part)
      }
      return parts
    }, [])
    .join('/node_modules/')
    .replace(/(\/node_modules)+/, '/node_modules')
  var f = path.resolve(npm.dir, p)
  fs.lstat(f, function (er) {
    if (er) return cb(er)
    editor(f, { editor: e }, noProgressTillDone(function (er) {
      if (er) return cb(er)
      npm.commands.rebuild(args, cb)
    }))
  })
}
