exports = module.exports = Dump
var Base = require('./base')
  , cursor = Base.cursor
  , color = Base.color
  , useColors = Base.useColors
  , util = require('util')

function Dump(runner) {
  Base.call(this, runner);

  var events = [
    'start',
    'version',
    'suite',
    'suite end',
    'test',
    'pending',
    'pass',
    'fail',
    'test end',
  ];

  var i = process.argv.indexOf('dump')
  if (i !== -1) {
    var args = process.argv.slice(i + 1)
    if (args.length)
      events = args
  }

  runner.on('line', function (c) {
    if (c.trim())
      process.stderr.write(Base.color('bright yellow', c))
  })

  events.forEach(function (ev) {
    runner.on(ev, function (obj) {
      console.log(ev)
      if (arguments.length) {
        console.log(util.inspect(obj, false, Infinity, useColors))
        console.log()
      }
    })
  })

  runner.on('end', function () {
    console.log('end')
    console.log(runner.stats)
    console.log()
  })
}
