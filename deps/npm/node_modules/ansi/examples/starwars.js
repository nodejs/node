#!/usr/bin/env node

/**
 * A little script to play the ASCII Star Wars, but with a hidden cursor,
 * since over `telnet(1)` the cursor remains visible which is annoying.
 */

process.title = 'starwars'

var net = require('net')
  , cursor = require('../')(process.stdout)
  , color = process.argv[2]

// enable "raw mode" so that keystrokes aren't visible
process.stdin.resume()
if (process.stdin.setRawMode) {
  process.stdin.setRawMode(true)
} else {
  require('tty').setRawMode(true)
}

// connect to the ASCII Star Wars server
var socket = net.connect(23, 'towel.blinkenlights.nl')

socket.on('connect', function () {
  if (color in cursor.fg) {
    cursor.fg[color]()
  }
  cursor.hide()
  socket.pipe(process.stdout)
})

process.stdin.on('data', function (data) {
  if (data.toString() === '\u0003') {
    // Ctrl+C; a.k.a SIGINT
    socket.destroy()
    process.stdin.pause()
  }
})

process.on('exit', function () {
  cursor
    .show()
    .fg.reset()
    .write('\n')
})
