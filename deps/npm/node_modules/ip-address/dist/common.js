"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.isInSubnet = isInSubnet;
exports.isCorrect = isCorrect;
exports.prefixLengthFromMask = prefixLengthFromMask;
exports.numberToPaddedHex = numberToPaddedHex;
exports.stringToPaddedHex = stringToPaddedHex;
exports.testBit = testBit;
const address_error_1 = require("./address-error");
function isInSubnet(address) {
    if (this.subnetMask < address.subnetMask) {
        return false;
    }
    if (this.mask(address.subnetMask) === address.mask()) {
        return true;
    }
    return false;
}
function isCorrect(defaultBits) {
    return function () {
        if (this.addressMinusSuffix !== this.correctForm()) {
            return false;
        }
        if (this.subnetMask === defaultBits && !this.parsedSubnet) {
            return true;
        }
        return this.parsedSubnet === String(this.subnetMask);
    };
}
/**
 * Returns the prefix length (number of leading 1 bits) of a contiguous
 * subnet mask. Throws `AddressError` if the mask is non-contiguous (e.g.
 * `255.0.255.0`).
 */
function prefixLengthFromMask(value, totalBits) {
    const binary = value.toString(2).padStart(totalBits, '0');
    if (binary.length > totalBits) {
        throw new address_error_1.AddressError('Invalid subnet mask.');
    }
    const firstZero = binary.indexOf('0');
    if (firstZero === -1) {
        return totalBits;
    }
    if (binary.slice(firstZero).includes('1')) {
        throw new address_error_1.AddressError('Invalid subnet mask.');
    }
    return firstZero;
}
function numberToPaddedHex(number) {
    return number.toString(16).padStart(2, '0');
}
function stringToPaddedHex(numberString) {
    return numberToPaddedHex(parseInt(numberString, 10));
}
/**
 * @param binaryValue Binary representation of a value (e.g. `10`)
 * @param position Byte position, where 0 is the least significant bit
 */
function testBit(binaryValue, position) {
    const { length } = binaryValue;
    if (position > length) {
        return false;
    }
    const positionInString = length - position;
    return binaryValue.substring(positionInString, positionInString + 1) === '1';
}
//# sourceMappingURL=common.js.map