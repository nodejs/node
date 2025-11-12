const platform = process.env.__TESTING_BIN_LINKS_PLATFORM__ || process.platform
module.exports = platform === 'win32'
