// A formatter is a Duplex stream that TAP data is written into,
// and then something else (presumably not-TAP) is read from.
//
// See tap-classic.js for an example of a formatter in use.

var Duplex = require('stream').Duplex
if (!Duplex) {
  try {
    Duplex = require('readable-stream').Duplex
  } catch (er) {
    throw new Error('Please install "readable-stream" to use this module ' +
                    'with Node.js v0.8 and before')
  }
}

var util = require('util')
var Parser = require('tap-parser')
util.inherits(Formatter, Duplex)
module.exports = Formatter

function Formatter(options, parser, parent) {
  if (!(this instanceof Formatter))
    return new Formatter(options, parser, parent)

  if (!parser)
    parser = new Parser()

  Duplex.call(this, options)
  this.child = null
  this.parent = parent || null
  this.level = parser.level
  this.parser = parser

  attachEvents(this, parser, options)

  if (options.init)
    options.init.call(this)
}

function attachEvents (self, parser, options) {
  var events = [
    'version', 'plan', 'assert', 'comment',
    'complete', 'extra', 'bailout'
  ]

  parser.on('child', function (childparser) {
    self.child = new Formatter(options, childparser, self)
    if (options.child)
      options.child.call(self, self.child)
  })

  events.forEach(function (ev) {
    if (typeof options[ev] === 'function')
      parser.on(ev, options[ev].bind(self))
  })

  // proxy all stream events directly
  var streamEvents = [
    'pipe', 'prefinish', 'finish', 'unpipe', 'close'
  ]

  streamEvents.forEach(function (ev) {
    parser.on(ev, function () {
      var args = [ev]
      args.push.apply(args, arguments)
      self.emit.apply(self, args)
    })
  })
}

Formatter.prototype.write = function (c, e, cb) {
  return this.parser.write(c, e, cb)
}

Formatter.prototype.end = function (c, e, cb) {
  return this.parser.end(c, e, cb)
}

Formatter.prototype._read = function () {}

// child formatters always push data to the root obj
Formatter.prototype.push = function (c) {
  if (this.parent)
    return this.parent.push(c)

  Duplex.prototype.push.call(this, c)
}
