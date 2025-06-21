module.exports = (chalk) => {
  const green = s => chalk.green.bold(s)
  const red = s => chalk.red.bold(s)
  const magenta = s => chalk.magenta.bold(s)
  const yellow = s => chalk.yellow.bold(s)
  const white = s => chalk.bold(s)
  const severity = (sev, s) => sev.toLowerCase() === 'moderate' ? yellow(s || sev)
    : sev.toLowerCase() === 'high' ? red(s || sev)
    : sev.toLowerCase() === 'critical' ? magenta(s || sev)
    : white(s || sev)
  const dim = s => chalk.dim(s)

  return {
    dim,
    green,
    red,
    magenta,
    yellow,
    white,
    severity,
  }
}
