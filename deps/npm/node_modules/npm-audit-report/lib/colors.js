const chalk = require('chalk')
module.exports = color => {
  const identity = x => x
  const green = color ? s => chalk.green.bold(s) : identity
  const red = color ? s => chalk.red.bold(s) : identity
  const magenta = color ? s => chalk.magenta.bold(s) : identity
  const yellow = color ? s => chalk.yellow.bold(s) : identity
  const white = color ? s => chalk.bold(s) : identity
  const severity = (sev, s) => sev.toLowerCase() === 'moderate' ? yellow(s || sev)
    : sev.toLowerCase() === 'high' ? red(s || sev)
    : sev.toLowerCase() === 'critical' ? magenta(s || sev)
    : white(s || sev)
  const dim = color ? s => chalk.dim(s) : identity

  return {
    dim,
    green,
    red,
    magenta,
    yellow,
    white,
    severity
  }
}
