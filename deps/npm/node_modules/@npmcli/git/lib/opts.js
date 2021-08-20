// Values we want to set if they're not already defined by the end user
// This defaults to accepting new ssh host key fingerprints
const gitEnv = {
  GIT_ASKPASS: 'echo',
  GIT_SSH_COMMAND: 'ssh -oStrictHostKeyChecking=accept-new'
}
module.exports = (opts = {}) => ({
  stdioString: true,
  ...opts,
  shell: false,
  env: opts.env || { ...gitEnv, ...process.env }
})
