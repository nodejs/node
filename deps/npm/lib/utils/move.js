'use strict'
module.exports = wrappedMove

const fs = require('graceful-fs')
const move = require('move-concurrently')
const Bluebird = require('bluebird')

const options = {fs: fs, Promise: Bluebird, maxConcurrency: 4}

function wrappedMove (from, to) {
  return move(from, to, options)
}
