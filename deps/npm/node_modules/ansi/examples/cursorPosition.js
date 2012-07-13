#!/usr/bin/env node

var tty = require('tty')
var cursor = require('../')(process.stdout)

// listen for the queryPosition report on stdin
process.stdin.resume()
tty.setRawMode(true)

process.stdin.once('data', function (b) {
  var match = /\[(\d+)\;(\d+)R$/.exec(b.toString())
  if (match) {
    var xy = match.slice(1, 3).reverse().map(Number)
    console.error(xy)
  }

  // cleanup and close stdin
  tty.setRawMode(false)
  process.stdin.pause()
})


// send the query position request code to stdout
cursor.queryPosition()
