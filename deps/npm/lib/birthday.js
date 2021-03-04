class Birthday {
  constructor (npm) {
    this.npm = npm
    Object.defineProperty(this.npm, 'flatOptions', {
      value: {
        ...npm.flatOptions,
        package: ['@npmcli/npm-birthday'],
        yes: true,
      },
    })
  }

  exec (args, cb) {
    return this.npm.commands.exec(['npm-birthday'], cb)
  }
}

module.exports = Birthday
