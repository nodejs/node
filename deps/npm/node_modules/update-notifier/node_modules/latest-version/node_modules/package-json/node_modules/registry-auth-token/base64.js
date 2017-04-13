function decodeBase64Old(base64) {
  return new Buffer(base64, 'base64').toString('utf8')
}

function decodeBase64New(base64) {
  return Buffer.from(base64, 'base64').toString('utf8')
}

function encodeBase64Old(string) {
  return new Buffer(string, 'utf8').toString('base64')
}

function encodeBase64New(string) {
  return Buffer.from(string, 'utf8').toString('base64')
}

module.exports = {
  decodeBase64: Buffer.prototype.from ? decodeBase64New : decodeBase64Old,
  encodeBase64: Buffer.prototype.from ? encodeBase64New : encodeBase64Old
}
