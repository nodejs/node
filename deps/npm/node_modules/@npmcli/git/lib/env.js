const uniqueFilename = require('unique-filename')
const { join } = require('path')
const {tmpdir} = require('os')

const goodEnvVars = new Set([
  'GIT_ASKPASS',
  'GIT_EXEC_PATH',
  'GIT_PROXY_COMMAND',
  'GIT_SSH',
  'GIT_SSH_COMMAND',
  'GIT_SSL_CAINFO',
  'GIT_SSL_NO_VERIFY'
])

// memoize
let gitEnv

module.exports = () => {
  if (gitEnv)
    return gitEnv

  // we set the template dir to an empty folder to give git less to do
  const tmpDir = join(tmpdir(), 'npmcli-git-template-tmp')
  const tmpName = uniqueFilename(tmpDir, 'git-clone')
  return gitEnv = Object.keys(process.env).reduce((gitEnv, k) => {
    if (goodEnvVars.has(k) || !k.startsWith('GIT_'))
      gitEnv[k] = process.env[k]
    return gitEnv
  }, {
    GIT_ASKPASS: 'echo',
    GIT_TEMPLATE_DIR: tmpName
  })
}
