const npm = require('../npm.js')
const didYouMean = require('./did-you-mean.js')
const { dirname } = require('path')
const output = require('./output.js')
const { cmdList } = require('./cmd-list')

module.exports = (valid = true) => {
  npm.config.set('loglevel', 'silent')
  npm.log.level = 'silent'
  output(`
Usage: npm <command>

npm install        install all the dependencies in your project
npm install <foo>  add the <foo> dependency to your project
npm test           run this project's tests
npm run <foo>      run the script named <foo>
npm <command> -h   quick help on <command>
npm -l             display usage info for all commands
npm help <term>    search for help on <term>
npm help npm       more involved overview

All commands:
${npm.config.get('long') ? usages() : ('\n    ' + wrap(cmdList))}

Specify configs in the ini-formatted file:
    ${npm.config.get('userconfig')}
or on the command line via: npm <command> --key=value

More configuration info: npm help config
Configuration fields: npm help 7 config

npm@${npm.version} ${dirname(dirname(__dirname))}
`)

  if (npm.argv.length >= 1)
    output(didYouMean(npm.argv[0], cmdList))

  if (!valid)
    process.exitCode = 1
}

const wrap = (arr) => {
  var out = ['']
  var l = 0
  var line

  line = process.stdout.columns
  if (!line)
    line = 60
  else
    line = Math.min(60, Math.max(line - 16, 24))

  arr.sort(function (a, b) {
    return a < b ? -1 : 1
  })
    .forEach(function (c) {
      if (out[l].length + c.length + 2 < line)
        out[l] += ', ' + c
      else {
        out[l++] += ','
        out[l] = c
      }
    })
  return out.join('\n    ').substr(2)
}

const usages = () => {
  // return a string of <command>: <usage>
  var maxLen = 0
  return cmdList.reduce(function (set, c) {
    set.push([c, require(`./${npm.deref(c)}.js`).usage || ''])
    maxLen = Math.max(maxLen, c.length)
    return set
  }, []).sort((a, b) => {
    return a[0].localeCompare(b[0])
  }).map(function (item) {
    var c = item[0]
    var usage = item[1]
    return '\n    ' +
      c + (new Array(maxLen - c.length + 2).join(' ')) +
      (usage.split('\n').join('\n' + (new Array(maxLen + 6).join(' '))))
  }).join('\n')
}
