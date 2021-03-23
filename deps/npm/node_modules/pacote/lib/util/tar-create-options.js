const isPackageBin = require('./is-package-bin.js')

const tarCreateOptions = manifest => ({
  cwd: manifest._resolved,
  prefix: 'package/',
  portable: true,
  gzip: {
    // forcing the level to 9 seems to avoid some
    // platform specific optimizations that cause
    // integrity mismatch errors due to differing
    // end results after compression
    level: 9
  },

  // ensure that package bins are always executable
  // Note that npm-packlist is already filtering out
  // anything that is not a regular file, ignored by
  // .npmignore or package.json "files", etc.
  filter: (path, stat) => {
    if (isPackageBin(manifest, path))
      stat.mode |= 0o111
    return true
  },

  // Provide a specific date in the 1980s for the benefit of zip,
  // which is confounded by files dated at the Unix epoch 0.
  mtime: new Date('1985-10-26T08:15:00.000Z'),
})

module.exports = tarCreateOptions
