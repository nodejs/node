'use strict';
const net = require('net');

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  var out = Object.create(null);
  var parts = s.split('\n');
  for (var i = 0, len = parts.length; i < len; i++) {
    var sepIndex = parts[i].indexOf('=');
    if (sepIndex > 0) {
      var key = parts[i].slice(0, sepIndex);
      var value = parts[i].slice(sepIndex + 1);
      if (key in out) {
        if (!Array.isArray(out[key])) {
          out[key] = [out[key]];
        }
        out[key].push(value);
      } else {
        out[key] = value;
      }
    }
  }
  return out;
}

function canonicalIp(address) {
  // Convert the ip address into the same format
  // stored in certificates
  if (net.isIPv6(address)) {
    const b = ['0', '0', '0', '0', '0', '0', '0', '0'];

    const s = address.split('::');
    if (s.length === 2) {
      const s1 = s[0].split(':');
      for (var n = 0; n < s1.length; n++) {
        if (s1[n]) {
          b[n] = s1[n].replace(/^0+(\d+)$/, '$1');
        }
      }
      const s2 = s[1].split(':');
      for (n = 0; n < s2.length; n++) {
        if (s2[n]) {
          b[8 - s2.length + n] = s2[n].replace(/^0+(\d+)$/, '$1');
        }
      }
    }

    return b.join(':');
  } else
    return address.replace(/\b0+(\d)/g, '$1'); // Delete leading zeroes
}

module.exports = {
  parseCertString,
  canonicalIp
};
