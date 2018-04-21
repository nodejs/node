'use strict';

// Load modules

const Crypto = require('crypto');
const Boom = require('boom');


// Declare internals

const internals = {};


// Generate a cryptographically strong pseudo-random data

exports.randomString = function (size) {

    const buffer = exports.randomBits((size + 1) * 6);
    if (buffer instanceof Error) {
        return buffer;
    }

    const string = buffer.toString('base64').replace(/\+/g, '-').replace(/\//g, '_').replace(/\=/g, '');
    return string.slice(0, size);
};


// Return a random string of digits

exports.randomDigits = function (size) {

    const buffer = exports.randomBits(size * 8);
    if (buffer instanceof Error) {
        return buffer;
    }

    const digits = [];
    for (let i = 0; i < buffer.length; ++i) {
        digits.push(Math.floor(buffer[i] / 25.6));
    }

    return digits.join('');
};


// Generate a buffer of random bits

exports.randomBits = function (bits) {

    if (!bits ||
        bits < 0) {

        return Boom.internal('Invalid random bits count');
    }

    const bytes = Math.ceil(bits / 8);
    try {
        return Crypto.randomBytes(bytes);
    }
    catch (err) {
        return Boom.internal('Failed generating random bits: ' + err.message);
    }
};


// Compare two strings using fixed time algorithm (to prevent time-based analysis of MAC digest match)

exports.fixedTimeComparison = function (a, b) {

    if (typeof a !== 'string' ||
        typeof b !== 'string') {

        return false;
    }

    let mismatch = (a.length === b.length ? 0 : 1);
    if (mismatch) {
        b = a;
    }

    for (let i = 0; i < a.length; ++i) {
        const ac = a.charCodeAt(i);
        const bc = b.charCodeAt(i);
        mismatch |= (ac ^ bc);
    }

    return (mismatch === 0);
};
