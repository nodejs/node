const h = require('crypto').createHash('sha384');
const fakeDigest = h.digest();

const kHandle = Object.getOwnPropertySymbols(h)
                      .find((s) => s.description === 'kHandle');
h[kHandle].constructor.prototype.digest = () => fakeDigest;

require('./protected.js');
