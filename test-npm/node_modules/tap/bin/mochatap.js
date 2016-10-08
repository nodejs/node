#!/usr/bin/env node
var tap = require('../lib/root.js')
var args = process.argv.slice(2)

if (args.length === 1) {
  var path = require('path')
  var file = path.resolve(args[0])
  tap.mochaGlobals()
  require(file)
} else {
  for (var i = 0; i < args.length; i++) {
    tap.spawn(process.execPath, [__filename, args[i]])
  }
}
