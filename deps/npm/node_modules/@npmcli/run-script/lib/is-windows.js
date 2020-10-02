const platform = process.env.__FAKE_TESTING_PLATFORM__ || process.platform
module.exports = platform === 'win32'
