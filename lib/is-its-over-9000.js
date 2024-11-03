'use strict';

module.exports = (num) => {
    let isNumber = false;
    if (typeof num === 'number') {
        isNumber = num - num === 0;
    }
    if (typeof num === 'string' && num.trim() !== '') {
        isNumber = Number.isFinite ? Number.isFinite(+num) : isFinite(+num);
    }
    if (!isNumber) {
        throw new TypeError('expected a number');
    }
    if (!Number.isSafeInteger(num)) {
        throw new Error('value exceeds maximum safe integer');
    }
    return num > 9000;
}
