// run an npm command
const spawn = require('@npmcli/promise-spawn')

module.exports = (npmBin, npmCommand, cwd, env, extra) => {
  const isJS = npmBin.endsWith('.js')
  const cmd = isJS ? process.execPath : npmBin
  const args = (isJS ? [npmBin] : []).concat(npmCommand)
  // when installing to run the `prepare` script for a git dep, we need
  // to ensure that we don't run into a cycle of checking out packages
  // in temp directories.  this lets us link previously-seen repos that
  // are also being prepared.

  return spawn(cmd, args, { cwd, stdioString: true, env }, extra)
}
