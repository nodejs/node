const log = require('npmlog')
const usageUtil = require('./utils/usage.js')
const { localPrefix } = require('./npm.js')
const fs = require('fs')
const usage = usageUtil('set-script', 'npm set-script [<script>] [<command>]')
const completion = require('./utils/completion/none.js')
const parseJSON = require('json-parse-even-better-errors')
const rpj = require('read-package-json-fast')

const cmd = (args, cb) => set(args).then(() => cb()).catch(cb)

const set = async function (args) {
  if (process.env.npm_lifecycle_event === 'postinstall')
    throw new Error('Scripts can’t set from the postinstall script')

  // Parse arguments
  if (args.length !== 2)
    throw new Error(`Expected 2 arguments: got ${args.length}`)

  // Set the script
  let manifest
  let warn = false
  try {
    manifest = fs.readFileSync(localPrefix + '/package.json', 'utf-8')
  } catch (error) {
    throw new Error('package.json not found')
  }
  try {
    manifest = parseJSON(manifest)
  } catch (error) {
    throw new Error(`Invalid package.json: ${error}`)
  }
  if (!manifest.scripts)
    manifest.scripts = {}
  if (manifest.scripts[args[0]] && manifest.scripts[args[0]] !== args[1])
    warn = true
  manifest.scripts[args[0]] = args[1]
  // format content
  const packageJsonInfo = await rpj(localPrefix + '/package.json')
  const {
    [Symbol.for('indent')]: indent,
    [Symbol.for('newline')]: newline,
  } = packageJsonInfo
  const format = indent === undefined ? '  ' : indent
  const eol = newline === undefined ? '\n' : newline
  const content = (JSON.stringify(manifest, null, format) + '\n')
    .replace(/\n/g, eol)
  fs.writeFileSync(localPrefix + '/package.json', content)
  if (warn)
    log.warn('set-script', `Script "${args[0]}" was overwritten`)
}

module.exports = Object.assign(cmd, { usage, completion })
