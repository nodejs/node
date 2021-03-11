// Base class for npm.commands[cmd]
const usageUtil = require('./utils/usage.js')

class BaseCommand {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    let usage = `npm ${this.constructor.name}\n\n`
    if (this.constructor.description)
      usage = `${usage}${this.constructor.description}\n\n`

    usage = `${usage}Usage:\n`
    if (!this.constructor.usage)
      usage = `${usage}npm ${this.constructor.name}`
    else
      usage = `${usage}${this.constructor.usage.map(u => `npm ${this.constructor.name} ${u}`).join('\n')}`

    // Mostly this just appends aliases, this could be more clear
    usage = usageUtil(this.constructor.name, usage)
    usage = `${usage}\n\nRun "npm help ${this.constructor.name}" for more info`
    return usage
  }

  usageError (msg) {
    if (!msg) {
      return Object.assign(new Error(`\nUsage: ${this.usage}`), {
        code: 'EUSAGE',
      })
    }

    return Object.assign(new Error(`\nUsage: ${msg}\n\n${this.usage}`), {
      code: 'EUSAGE',
    })
  }
}
module.exports = BaseCommand
