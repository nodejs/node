const { commands } = require('./cmd-list')

const COL_MAX = 60
const COL_MIN = 24
const COL_GUTTER = 16
const INDENT = 4

const indent = (repeat = INDENT) => ' '.repeat(repeat)
const indentNewline = (repeat) => `\n${indent(repeat)}`

module.exports = (npm) => {
  const browser = npm.config.get('viewer') === 'browser' ? ' (in a browser)' : ''
  const allCommands = npm.config.get('long') ? cmdUsages(npm.constructor) : cmdNames()

  return `npm <command>

Usage:

npm install        install all the dependencies in your project
npm install <foo>  add the <foo> dependency to your project
npm test           run this project's tests
npm run <foo>      run the script named <foo>
npm <command> -h   quick help on <command>
npm -l             display usage info for all commands
npm help <term>    search for help on <term>${browser}
npm help npm       more involved overview${browser}

All commands:
${allCommands}

Specify configs in the ini-formatted file:
${indent() + npm.config.get('userconfig')}
or on the command line via: npm <command> --key=value

More configuration info: npm help config
Configuration fields: npm help 7 config

npm@${npm.version} ${npm.npmRoot}`
}

const cmdNames = () => {
  const out = ['']

  const line = !process.stdout.columns ? COL_MAX
    : Math.min(COL_MAX, Math.max(process.stdout.columns - COL_GUTTER, COL_MIN))

  let l = 0
  for (const c of commands) {
    if (out[l].length + c.length + 2 < line) {
      out[l] += ', ' + c
    } else {
      out[l++] += ','
      out[l] = c
    }
  }

  return indentNewline() + out.join(indentNewline()).slice(2)
}

const cmdUsages = (Npm) => {
  // return a string of <command>: <usage>
  let maxLen = 0
  const set = []
  for (const c of commands) {
    set.push([c, Npm.cmd(c).getUsage(null, false).split('\n')])
    maxLen = Math.max(maxLen, c.length)
  }

  return set.map(([name, usageLines]) => {
    const gutter = indent(maxLen - name.length + 1)
    const usage = usageLines.join(indentNewline(INDENT + maxLen + 1))
    return indentNewline() + name + gutter + usage
  }).join('\n')
}
