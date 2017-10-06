'use strict';

function getCIDRSuffix(mask, protocol = 'ipv4') {
  const isV6 = protocol === 'ipv6';
  const bitsString = mask
        .split(isV6 ? ':' : '.')
        .filter((v) => !!v)
        .map((v) => pad(parseInt(v, isV6 ? 16 : 10).toString(2), isV6))
        .join('');

  if (isValidMask(bitsString)) {
    return countOnes(bitsString);
  } else {
    return null;
  }
}

function pad(binaryString, isV6) {
  const groupLength = isV6 ? 16 : 8;
  const binLen = binaryString.length;

  return binLen < groupLength ?
    `${'0'.repeat(groupLength - binLen)}${binaryString}` : binaryString;
}

function isValidMask(bitsString) {
  const firstIndexOfZero = bitsString.indexOf(0);
  const lastIndexOfOne = bitsString.lastIndexOf(1);

  return firstIndexOfZero < 0 || firstIndexOfZero > lastIndexOfOne;
}

function countOnes(bitsString) {
  return bitsString
    .split('')
    .reduce((acc, bit) => acc += parseInt(bit, 10), 0);
}

module.exports = {
  getCIDRSuffix
};
