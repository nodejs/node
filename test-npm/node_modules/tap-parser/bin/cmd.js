#!/usr/bin/env node

var Parser = require('../')
var etoa = require('events-to-array')
var util = require('util')

var args = process.argv.slice(2)
var json = null

args.forEach(function (arg, i) {
  if (arg === '-j') {
    json = args[i + 1] || 2
  } else {
    var m = arg.match(/^--json(?:=([0-9]+))$/)
    if (m)
      json = +m[1] || args[i + 1] || 2
  }

  if (arg === '-t' || arg === '--tap')
    json = 'tap'

  if (arg === '-h' || arg === '--help')
    usage()
})

function usage () {
  console.log(function () {/*
Usage:
  tap-parser [-j [<indent>] | --json[=indent] | -t | --tap]

Parses TAP data from stdin, and outputs an object representing
the data found in the TAP stream to stdout.

If there are any failures in the TAP stream, then exits with a
non-zero status code.

Data is output by default using node's `util.format()` method, but
JSON can be specified using the `-j` or `--json` flag with a number
of spaces to use as the indent (default=2).

If you pass -t or --tap as an argument, then the output will be a
re-imagined synthesized purified idealized manufactured TAP stream,
rather than JSON or util.format.
*/}.toString().split('\n').slice(1, -1).join('\n'))

  if (!process.stdin.isTTY)
    process.stdin.resume()

  process.exit()
}

var yaml = require('js-yaml')
function tapFormat (msg, indent) {
  return indent + msg.map(function (item) {
    switch (item[0]) {
      case 'child':
        return tapFormat(item[1], '    ')

      case 'version':
        return 'TAP version ' + item[1] + '\n'

      case 'plan':
        var p = item[1].start + '..' + item[1].end
        if (item[1].comment)
          p += ' # ' + item[1].comment
        return p + '\n'

      case 'pragma':
        return 'pragma ' + (item[2] ? '+' : '-') + item[1] + '\n'

      case 'bailout':
        var r = item[1] === true ? '' : (' ' + item[1])
        return 'Bail out!' + r + '\n'

      case 'assert':
        var res = item[1]
        return (res.ok ? '' : 'not ') + 'ok ' + res.id +
          (res.name ? ' - ' + res.name : '') +
          (res.skip ? ' # SKIP' +
            (res.skip === true ? '' : ' ' + res.skip) : '') +
          (res.todo ? ' # TODO' +
            (res.todo === true ? '' : ' ' + res.todo) : '') +
          (res.time ? ' # time=' + res.time + 's' : '') +
          '\n' +
          (res.diags ?
             '  ---\n  ' +
             yaml.safeDump(res.diags).split('\n').join('\n  ').trim() +
             '\n  ...\n'
             : '')

      case 'extra':
      case 'comment':
        return item[1]
    }
  }).join('').split('\n').join('\n' + indent).trim() + '\n'
}

function format (msg) {
  if (json === 'tap')
    return tapFormat(msg, '')
  else if (json !== null)
    return JSON.stringify(msg, null, +json)
  else
    return util.inspect(events, null, Infinity)
}

var parser = new Parser()
var events = etoa(parser, [ 'pipe', 'unpipe', 'prefinish', 'finish', 'line' ])

process.stdin.pipe(parser)
process.on('exit', function () {
  console.log(format(events))
  if (!parser.ok)
    process.exit(1)
})
