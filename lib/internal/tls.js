'use strict';

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  const out = Object.create(null);
  const parts = s.split('\n');
  for (var i = 0, len = parts.length; i < len; i++) {
    const sepIndex = parts[i].indexOf('=');
    if (sepIndex > 0) {
      const key = parts[i].slice(0, sepIndex);
      const value = parts[i].slice(sepIndex + 1);
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

module.exports = {
  parseCertString
};
