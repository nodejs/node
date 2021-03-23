// Base class for npm.commands[cmd]
const usageUtil = require('./utils/usage.js')
const ConfigDefinitions = require('./utils/config/definitions.js')

class BaseCommand {
  constructor (npm) {
    this.npm = npm
  }

  get name () {
    return this.constructor.name
  }

  get description () {
    return this.constructor.description
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

    if (this.constructor.params)
      usage = `${usage}\n\nOptions:\n[${this.constructor.params.map(p => ConfigDefinitions[p].usage).join('] [')}]`

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

  execWorkspaces (args, filters, cb) {
    throw Object.assign(
      new Error('This command does not support workspaces.'),
      { code: 'ENOWORKSPACES' }
    )
  }
}
module.exports = BaseCommand
