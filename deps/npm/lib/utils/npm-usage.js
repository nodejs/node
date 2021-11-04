const { dirname } = require('path')
const { cmdList } = require('./cmd-list')
const localeCompare = require('@isaacs/string-locale-compare')('en')

module.exports = async (npm) => {
  const usesBrowser = npm.config.get('viewer') === 'browser'
    ? ' (in a browser)' : ''
  return `npm <command>

Usage:

npm install        install all the dependencies in your project
npm install <foo>  add the <foo> dependency to your project
npm test           run this project's tests
npm run <foo>      run the script named <foo>
npm <command> -h   quick help on <command>
npm -l             display usage info for all commands
npm help <term>    search for help on <term>${usesBrowser}
npm help npm       more involved overview${usesBrowser}

All commands:
${await allCommands(npm)}

Specify configs in the ini-formatted file:
    ${npm.config.get('userconfig')}
or on the command line via: npm <command> --key=value

More configuration info: npm help config
Configuration fields: npm help 7 config

npm@${npm.version} ${dirname(dirname(__dirname))}`
}

const allCommands = async (npm) => {
  if (npm.config.get('long'))
    return usages(npm)
  return ('\n    ' + wrap(cmdList))
}

const wrap = (arr) => {
  const out = ['']

  const line = !process.stdout.columns ? 60
    : Math.min(60, Math.max(process.stdout.columns - 16, 24))

  let l = 0
  for (const c of arr.sort((a, b) => a < b ? -1 : 1)) {
    if (out[l].length + c.length + 2 < line)
      out[l] += ', ' + c
    else {
      out[l++] += ','
      out[l] = c
    }
  }
  return out.join('\n    ').substr(2)
}

const usages = async (npm) => {
  // return a string of <command>: <usage>
  let maxLen = 0
  const set = []
  for (const c of cmdList) {
    const cmd = await npm.cmd(c)
    set.push([c, cmd.usage])
    maxLen = Math.max(maxLen, c.length)
  }
  return set.sort(([a], [b]) => localeCompare(a, b))
    .map(([c, usage]) => `\n    ${c}${' '.repeat(maxLen - c.length + 1)}${
      (usage.split('\n').join('\n' + ' '.repeat(maxLen + 5)))}`)
    .join('\n')
}
