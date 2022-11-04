module.exports = () => ({
  'git+ssh:': { name: 'sshurl' },
  'ssh:': { name: 'sshurl' },
  'git+https:': { name: 'https', auth: true },
  'git:': { auth: true },
  'http:': { auth: true },
  'https:': { auth: true },
  'git+http:': { auth: true },
})
