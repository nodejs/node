// Unique ID creation requires a high quality random # generator.  In node.js
// this is prett straight-forward - we use the crypto API.

var rb = require('crypto').randomBytes;

function rng() {
  return rb(16);
};

module.exports = rng;
