// this file handles outputting usage instructions,
// failures, etc. keeps logging in one place.
const cliui = require('cliui')
const decamelize = require('decamelize')
const stringWidth = require('string-width')
const wsize = require('window-size')
const objFilter = require('./obj-filter')
const setBlocking = require('set-blocking')

module.exports = function (yargs, y18n) {
  const __ = y18n.__
  const self = {}

  // methods for ouputting/building failure message.
  var fails = []
  self.failFn = function (f) {
    fails.push(f)
  }

  var failMessage = null
  var showHelpOnFail = true
  self.showHelpOnFail = function (enabled, message) {
    if (typeof enabled === 'string') {
      message = enabled
      enabled = true
    } else if (typeof enabled === 'undefined') {
      enabled = true
    }
    failMessage = message
    showHelpOnFail = enabled
    return self
  }

  var failureOutput = false
  self.fail = function (msg, err) {
    if (fails.length) {
      fails.forEach(function (f) {
        f(msg, err)
      })
    } else {
      if (yargs.getExitProcess()) setBlocking(true)

      // don't output failure message more than once
      if (!failureOutput) {
        failureOutput = true
        if (showHelpOnFail) yargs.showHelp('error')
        if (msg) console.error(msg)
        if (failMessage) {
          if (msg) console.error('')
          console.error(failMessage)
        }
      }
      if (yargs.getExitProcess()) {
        process.exit(1)
      } else {
        throw err || new Error(msg)
      }
    }
  }

  // methods for ouputting/building help (usage) message.
  var usage
  self.usage = function (msg) {
    usage = msg
  }
  self.getUsage = function () {
    return usage
  }

  var examples = []
  self.example = function (cmd, description) {
    examples.push([cmd, description || ''])
  }

  var commands = []
  self.command = function (cmd, description) {
    commands.push([cmd, description || ''])
  }
  self.getCommands = function () {
    return commands
  }

  var descriptions = {}
  self.describe = function (key, desc) {
    if (typeof key === 'object') {
      Object.keys(key).forEach(function (k) {
        self.describe(k, key[k])
      })
    } else {
      descriptions[key] = desc
    }
  }
  self.getDescriptions = function () {
    return descriptions
  }

  var epilog
  self.epilog = function (msg) {
    epilog = msg
  }

  var wrap = windowWidth()
  self.wrap = function (cols) {
    wrap = cols
  }

  var deferY18nLookupPrefix = '__yargsString__:'
  self.deferY18nLookup = function (str) {
    return deferY18nLookupPrefix + str
  }

  var defaultGroup = 'Options:'
  self.help = function () {
    normalizeAliases()

    var demanded = yargs.getDemanded()
    var groups = yargs.getGroups()
    var options = yargs.getOptions()
    var keys = Object.keys(
      Object.keys(descriptions)
      .concat(Object.keys(demanded))
      .concat(Object.keys(options.default))
      .reduce(function (acc, key) {
        if (key !== '_') acc[key] = true
        return acc
      }, {})
    )
    var ui = cliui({
      width: wrap,
      wrap: !!wrap
    })

    // the usage string.
    if (usage) {
      var u = usage.replace(/\$0/g, yargs.$0)
      ui.div(u + '\n')
    }

    // your application's commands, i.e., non-option
    // arguments populated in '_'.
    if (commands.length) {
      ui.div(__('Commands:'))

      commands.forEach(function (command) {
        ui.div(
          {text: command[0], padding: [0, 2, 0, 2], width: maxWidth(commands) + 4},
          {text: command[1]}
        )
      })

      ui.div()
    }

    // perform some cleanup on the keys array, making it
    // only include top-level keys not their aliases.
    var aliasKeys = (Object.keys(options.alias) || [])
      .concat(Object.keys(yargs.parsed.newAliases) || [])

    keys = keys.filter(function (key) {
      return !yargs.parsed.newAliases[key] && aliasKeys.every(function (alias) {
        return (options.alias[alias] || []).indexOf(key) === -1
      })
    })

    // populate 'Options:' group with any keys that have not
    // explicitly had a group set.
    if (!groups[defaultGroup]) groups[defaultGroup] = []
    addUngroupedKeys(keys, options.alias, groups)

    // display 'Options:' table along with any custom tables:
    Object.keys(groups).forEach(function (groupName) {
      if (!groups[groupName].length) return

      ui.div(__(groupName))

      // if we've grouped the key 'f', but 'f' aliases 'foobar',
      // normalizedKeys should contain only 'foobar'.
      var normalizedKeys = groups[groupName].map(function (key) {
        if (~aliasKeys.indexOf(key)) return key
        for (var i = 0, aliasKey; (aliasKey = aliasKeys[i]) !== undefined; i++) {
          if (~(options.alias[aliasKey] || []).indexOf(key)) return aliasKey
        }
        return key
      })

      // actually generate the switches string --foo, -f, --bar.
      var switches = normalizedKeys.reduce(function (acc, key) {
        acc[key] = [ key ].concat(options.alias[key] || [])
          .map(function (sw) {
            return (sw.length > 1 ? '--' : '-') + sw
          })
          .join(', ')

        return acc
      }, {})

      normalizedKeys.forEach(function (key) {
        var kswitch = switches[key]
        var desc = descriptions[key] || ''
        var type = null

        if (~desc.lastIndexOf(deferY18nLookupPrefix)) desc = __(desc.substring(deferY18nLookupPrefix.length))

        if (~options.boolean.indexOf(key)) type = '[' + __('boolean') + ']'
        if (~options.count.indexOf(key)) type = '[' + __('count') + ']'
        if (~options.string.indexOf(key)) type = '[' + __('string') + ']'
        if (~options.normalize.indexOf(key)) type = '[' + __('string') + ']'
        if (~options.array.indexOf(key)) type = '[' + __('array') + ']'
        if (~options.number.indexOf(key)) type = '[' + __('number') + ']'

        var extra = [
          type,
          demanded[key] ? '[' + __('required') + ']' : null,
          options.choices && options.choices[key] ? '[' + __('choices:') + ' ' +
            self.stringifiedValues(options.choices[key]) + ']' : null,
          defaultString(options.default[key], options.defaultDescription[key])
        ].filter(Boolean).join(' ')

        ui.span(
          {text: kswitch, padding: [0, 2, 0, 2], width: maxWidth(switches) + 4},
          desc
        )

        if (extra) ui.div({text: extra, padding: [0, 0, 0, 2], align: 'right'})
        else ui.div()
      })

      ui.div()
    })

    // describe some common use-cases for your application.
    if (examples.length) {
      ui.div(__('Examples:'))

      examples.forEach(function (example) {
        example[0] = example[0].replace(/\$0/g, yargs.$0)
      })

      examples.forEach(function (example) {
        ui.div(
          {text: example[0], padding: [0, 2, 0, 2], width: maxWidth(examples) + 4},
          example[1]
        )
      })

      ui.div()
    }

    // the usage string.
    if (epilog) {
      var e = epilog.replace(/\$0/g, yargs.$0)
      ui.div(e + '\n')
    }

    return ui.toString()
  }

  // return the maximum width of a string
  // in the left-hand column of a table.
  function maxWidth (table) {
    var width = 0

    // table might be of the form [leftColumn],
    // or {key: leftColumn}}
    if (!Array.isArray(table)) {
      table = Object.keys(table).map(function (key) {
        return [table[key]]
      })
    }

    table.forEach(function (v) {
      width = Math.max(stringWidth(v[0]), width)
    })

    // if we've enabled 'wrap' we should limit
    // the max-width of the left-column.
    if (wrap) width = Math.min(width, parseInt(wrap * 0.5, 10))

    return width
  }

  // make sure any options set for aliases,
  // are copied to the keys being aliased.
  function normalizeAliases () {
    var demanded = yargs.getDemanded()
    var options = yargs.getOptions()

    ;(Object.keys(options.alias) || []).forEach(function (key) {
      options.alias[key].forEach(function (alias) {
        // copy descriptions.
        if (descriptions[alias]) self.describe(key, descriptions[alias])
        // copy demanded.
        if (demanded[alias]) yargs.demand(key, demanded[alias].msg)
        // type messages.
        if (~options.boolean.indexOf(alias)) yargs.boolean(key)
        if (~options.count.indexOf(alias)) yargs.count(key)
        if (~options.string.indexOf(alias)) yargs.string(key)
        if (~options.normalize.indexOf(alias)) yargs.normalize(key)
        if (~options.array.indexOf(alias)) yargs.array(key)
        if (~options.number.indexOf(alias)) yargs.number(key)
      })
    })
  }

  // given a set of keys, place any keys that are
  // ungrouped under the 'Options:' grouping.
  function addUngroupedKeys (keys, aliases, groups) {
    var groupedKeys = []
    var toCheck = null
    Object.keys(groups).forEach(function (group) {
      groupedKeys = groupedKeys.concat(groups[group])
    })

    keys.forEach(function (key) {
      toCheck = [key].concat(aliases[key])
      if (!toCheck.some(function (k) {
        return groupedKeys.indexOf(k) !== -1
      })) {
        groups[defaultGroup].push(key)
      }
    })
    return groupedKeys
  }

  self.showHelp = function (level) {
    if (!level) level = 'error'
    var emit = typeof level === 'function' ? level : console[ level ]
    emit(self.help())
  }

  self.functionDescription = function (fn) {
    var description = fn.name ? decamelize(fn.name, '-') : __('generated-value')
    return ['(', description, ')'].join('')
  }

  self.stringifiedValues = function (values, separator) {
    var string = ''
    var sep = separator || ', '
    var array = [].concat(values)

    if (!values || !array.length) return string

    array.forEach(function (value) {
      if (string.length) string += sep
      string += JSON.stringify(value)
    })

    return string
  }

  // format the default-value-string displayed in
  // the right-hand column.
  function defaultString (value, defaultDescription) {
    var string = '[' + __('default:') + ' '

    if (value === undefined && !defaultDescription) return null

    if (defaultDescription) {
      string += defaultDescription
    } else {
      switch (typeof value) {
        case 'string':
          string += JSON.stringify(value)
          break
        case 'object':
          string += JSON.stringify(value)
          break
        default:
          string += value
      }
    }

    return string + ']'
  }

  // guess the width of the console window, max-width 80.
  function windowWidth () {
    return wsize.width ? Math.min(80, wsize.width) : null
  }

  // logic for displaying application version.
  var version = null
  self.version = function (ver) {
    version = ver
  }

  self.showVersion = function () {
    if (typeof version === 'function') console.log(version())
    else console.log(version)
  }

  self.reset = function (globalLookup) {
    // do not reset wrap here
    fails = []
    failMessage = null
    failureOutput = false
    usage = undefined
    epilog = undefined
    examples = []
    commands = []
    descriptions = objFilter(descriptions, function (k, v) {
      return globalLookup[k]
    })
    return self
  }

  return self
}
