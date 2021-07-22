'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  ArrayPrototypePush,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  ObjectCreate,
} = primordials;

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  const out = ObjectCreate(null);
  ArrayPrototypeForEach(StringPrototypeSplit(s, '\n'), (part) => {
    const sepIndex = StringPrototypeIndexOf(part, '=');
    if (sepIndex > 0) {
      const key = StringPrototypeSlice(part, 0, sepIndex);
      const value = StringPrototypeSlice(part, sepIndex + 1);
      if (key in out) {
        if (!ArrayIsArray(out[key])) {
          out[key] = [out[key]];
        }
        ArrayPrototypePush(out[key], value);
      } else {
        out[key] = value;
      }
    }
  });
  return out;
}

exports.parseCertString = parseCertString;
