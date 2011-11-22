// wrapper around the non-sync fs functions to gracefully handle
// having too many file descriptors open.  Note that this is
// *only* possible because async patterns let one interject timeouts
// and other cleverness anywhere in the process without disrupting
// anything else.
var fs = require("fs")
  , timeout = 0

Object.keys(fs)
  .forEach(function (i) {
    exports[i] = (typeof fs[i] !== "function") ? fs[i]
               : (i.match(/^[A-Z]|^create|Sync$/)) ? function () {
                   return fs[i].apply(fs, arguments)
                 }
               : graceful(fs[i])
  })

if (process.platform === "win32"
    && !process.binding("fs").lstat) {
  exports.lstat = exports.stat
  exports.lstatSync = exports.statSync
}

function graceful (fn) { return function GRACEFUL () {
  var args = Array.prototype.slice.call(arguments)
    , cb_ = args.pop()
  args.push(cb)
  function cb (er) {
    if (er && er.message.match(/^EMFILE, Too many open files/)) {
      setTimeout(function () {
        GRACEFUL.apply(fs, args)
      }, timeout ++)
      return
    }
    timeout = 0
    cb_.apply(null, arguments)
  }
  fn.apply(fs, args)
}}
