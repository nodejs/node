#!/usr/bin/env node

var assert = require('assert')
  , ansi = require('../../')

function Progress (stream, width) {
  this.cursor = ansi(stream)
  this.delta = this.cursor.newlines
  this.width = width | 0 || 10
  this.open = '['
  this.close = ']'
  this.complete = '█'
  this.incomplete = '_'

  // initial render
  this.progress = 0
}

Object.defineProperty(Progress.prototype, 'progress', {
    get: get
  , set: set
  , configurable: true
  , enumerable: true
})

function get () {
  return this._progress
}

function set (v) {
  this._progress = Math.max(0, Math.min(v, 100))

  var w = this.width - this.complete.length - this.incomplete.length
    , n = w * (this._progress / 100) | 0
    , i = w - n
    , com = c(this.complete, n)
    , inc = c(this.incomplete, i)
    , delta = this.cursor.newlines - this.delta

  assert.equal(com.length + inc.length, w)

  if (delta > 0) {
    this.cursor.up(delta)
    this.delta = this.cursor.newlines
  }

  this.cursor
    .horizontalAbsolute(0)
    .eraseLine(2)
    .fg.white()
    .write(this.open)
    .fg.grey()
    .bold()
    .write(com)
    .resetBold()
    .write(inc)
    .fg.white()
    .write(this.close)
    .fg.reset()
    .write('\n')
}

function c (char, length) {
  return Array.apply(null, Array(length)).map(function () {
    return char
  }).join('')
}




// Usage
var width = parseInt(process.argv[2], 10) || process.stdout.getWindowSize()[0] / 2
  , p = new Progress(process.stdout, width)

;(function tick () {
  p.progress += Math.random() * 5
  p.cursor
    .eraseLine(2)
    .write('Progress: ')
    .bold().write(p.progress.toFixed(2))
    .write('%')
    .resetBold()
    .write('\n')
  if (p.progress < 100)
    setTimeout(tick, 100)
})()
