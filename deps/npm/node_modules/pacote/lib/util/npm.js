// run an npm command
const spawn = require('@npmcli/promise-spawn')

module.exports = (npmBin, npmCommand, cwd, extra) => {
  const isJS = npmBin.endsWith('.js')
  const cmd = isJS ? process.execPath : npmBin
  const args = (isJS ? [npmBin] : []).concat(npmCommand)
  return spawn(cmd, args, { cwd, stdioString: true }, extra)
}
