// happy xmas
var log = require("npmlog")

module.exports = function (args, cb) {
var s = process.platform === "win32" ? " *" : " \u2605"
  , f = "\uFF0F"
  , b = "\uFF3C"
  , x = process.platform === "win32" ? " " : ""
  , o = [ "\u0069" , "\u0020", "\u0020", "\u0020", "\u0020", "\u0020"
        , "\u0020", "\u0020", "\u0020", "\u0020", "\u0020", "\u0020"
        , "\u0020", "\u2E1B","\u2042","\u2E2E","&","@","\uFF61" ]
  , oc = [21,33,34,35,36,37]
  , l = "\u005e"

function w (s) { process.stderr.write(s) }

w("\n")
;(function T (H) {
  for (var i = 0; i < H; i ++) w(" ")
  w(x+"\033[33m"+s+"\n")
  var M = H * 2 - 1
  for (var L = 1; L <= H; L ++) {
    var O = L * 2 - 2
    var S = (M - O) / 2
    for (i = 0; i < S; i ++) w(" ")
    w(x+"\033[32m"+f)
    for (i = 0; i < O; i ++) w(
      "\033["+oc[Math.floor(Math.random()*oc.length)]+"m"+
      o[Math.floor(Math.random() * o.length)]
    )
    w(x+"\033[32m"+b+"\n")
  }
  w(" ")
  for (i = 1; i < H; i ++) w("\033[32m"+l)
  w("| "+x+" |")
  for (i = 1; i < H; i ++) w("\033[32m"+l)
  if (H > 10) {
    w("\n ")
    for (i = 1; i < H; i ++) w(" ")
    w("| "+x+" |")
    for (i = 1; i < H; i ++) w(" ")
  }
})(20)
w("\n\n")
log.heading = ''
log.addLevel('npm', 100000, log.headingStyle)
log.npm("loves you", "Happy Xmas, Noders!")
cb()
}
var dg=false
Object.defineProperty(module.exports, "usage", {get:function () {
  if (dg) module.exports([], function () {})
  dg = true
  return " "
}})
