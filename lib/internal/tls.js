'use strict';

const {
  ArrayIsArray,
  ObjectCreate,
} = primordials;

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  const out = ObjectCreate(null);
  for (const part of s.split('\n')) {
    const sepIndex = part.indexOf('=');
    if (sepIndex > 0) {
      const key = part.slice(0, sepIndex);
      const value = part.slice(sepIndex + 1);
      if (key in out) {
        if (!ArrayIsArray(out[key])) {
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
