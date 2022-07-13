// XXX: this also cleans quoted " in json snapshots
// ideally this could be avoided but its easier to just
// run this command inside cleanSnapshot
const normalizePath = (str) => str
  .replace(/\r\n/g, '\n') // normalize line endings (for ini)
  .replace(/[A-z]:\\/g, '\\') // turn windows roots to posix ones
  .replace(/\\+/g, '/') // replace \ with /

const cleanCwd = (path) => normalizePath(path)
  .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')

const cleanDate = (str) =>
  str.replace(/\d{4}-\d{2}-\d{2}T\d{2}[_:]\d{2}[_:]\d{2}[_:.]\d{3}Z/g, '{DATE}')

module.exports = {
  normalizePath,
  cleanCwd,
  cleanDate,
}
