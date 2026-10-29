const isWindows = opts => (opts.fakePlatform || process.platform) === 'win32'

exports.isWindows = isWindows
