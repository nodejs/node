#!/usr/bin/env node

var fs = require('fs')

var browserslist = require('./')
var updateDb = require('./update-db')
var pkg = require('./package.json')

var args = process.argv.slice(2)

var USAGE =
  'Usage:\n' +
  '  npx browserslist\n' +
  '  npx browserslist "QUERIES"\n' +
  '  npx browserslist --json "QUERIES"\n' +
  '  npx browserslist --config="path/to/browserlist/file"\n' +
  '  npx browserslist --coverage "QUERIES"\n' +
  '  npx browserslist --coverage=US "QUERIES"\n' +
  '  npx browserslist --coverage=US,RU,global "QUERIES"\n' +
  '  npx browserslist --env="environment name defined in config"\n' +
  '  npx browserslist --stats="path/to/browserlist/stats/file"\n' +
  '  npx browserslist --mobile-to-desktop\n' +
  '  npx browserslist --update-db'

function isArg(arg) {
  return args.some(function (str) {
    return str === arg || str.indexOf(arg + '=') === 0
  })
}

function error(msg) {
  process.stderr.write('browserslist: ' + msg + '\n')
  process.exit(1)
}

if (isArg('--help') || isArg('-h')) {
  process.stdout.write(pkg.description + '.\n\n' + USAGE + '\n')
} else if (isArg('--version') || isArg('-v')) {
  process.stdout.write('browserslist ' + pkg.version + '\n')
} else if (isArg('--update-db')) {
  updateDb(function (str) {
    process.stdout.write(str)
  })
} else {
  var mode = 'browsers'
  var opts = {}
  var queries
  var areas

  for (var i = 0; i < args.length; i++) {
    if (args[i][0] !== '-') {
      queries = args[i].replace(/^["']|["']$/g, '')
      continue
    }

    var arg = args[i].split('=')
    var name = arg[0]
    var value = arg[1]

    if (value) value = value.replace(/^["']|["']$/g, '')

    if (name === '--config' || name === '-b') {
      opts.config = value
    } else if (name === '--env' || name === '-e') {
      opts.env = value
    } else if (name === '--stats' || name === '-s') {
      opts.stats = value
    } else if (name === '--coverage' || name === '-c') {
      if (mode !== 'json') mode = 'coverage'
      if (value) {
        areas = value.split(',')
      } else {
        areas = ['global']
      }
    } else if (name === '--json') {
      mode = 'json'
    } else if (name === '--mobile-to-desktop') {
      opts.mobileToDesktop = true
    } else {
      error('Unknown arguments ' + args[i] + '.\n\n' + USAGE)
    }
  }

  var browsers
  try {
    browsers = browserslist(queries, opts)
  } catch (e) {
    if (e.name === 'BrowserslistError') {
      error(e.message)
    } else {
      throw e
    }
  }

  var coverage
  if (mode === 'browsers') {
    browsers.forEach(function (browser) {
      process.stdout.write(browser + '\n')
    })
  } else if (areas) {
    coverage = areas.map(function (area) {
      var stats
      if (area !== 'global') {
        stats = area
      } else if (opts.stats) {
        stats = JSON.parse(fs.readFileSync(opts.stats))
      }
      var result = browserslist.coverage(browsers, stats)
      var round = Math.round(result * 100) / 100.0

      return [area, round]
    })

    if (mode === 'coverage') {
      var prefix = 'These browsers account for '
      process.stdout.write(prefix)
      coverage.forEach(function (data, index) {
        var area = data[0]
        var round = data[1]
        var end = 'globally'
        if (area && area !== 'global') {
          end = 'in the ' + area.toUpperCase()
        } else if (opts.stats) {
          end = 'in custom statistics'
        }

        if (index !== 0) {
          process.stdout.write(prefix.replace(/./g, ' '))
        }

        process.stdout.write(round + '% of all users ' + end + '\n')
      })
    }
  }

  if (mode === 'json') {
    var data = { browsers: browsers }
    if (coverage) {
      data.coverage = coverage.reduce(function (object, j) {
        object[j[0]] = j[1]
        return object
      }, {})
    }
    process.stdout.write(JSON.stringify(data, null, '  ') + '\n')
  }
}
