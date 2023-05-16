'use strict'

class ErrInvalidAuth extends Error {
  constructor (problems) {
    let message = 'Invalid auth configuration found: '
    message += problems.map((problem) => {
      if (problem.action === 'delete') {
        return `\`${problem.key}\` is not allowed in ${problem.where} config`
      } else if (problem.action === 'rename') {
        return `\`${problem.from}\` must be renamed to \`${problem.to}\` in ${problem.where} config`
      }
    }).join(', ')
    message += '\nPlease run `npm config fix` to repair your configuration.`'
    super(message)
    this.code = 'ERR_INVALID_AUTH'
    this.problems = problems
  }
}

module.exports = {
  ErrInvalidAuth,
}
