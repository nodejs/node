"use strict";
/**
 * @fileOverview
 * Core operations on curve 25519 required for the higher level modules.
 */

/*
 * Copyright (c) 2007, 2013, 2014 Michele Bini
 * Copyright (c) 2014 Mega Limited
 * under the MIT License.
 *
 * Authors: Guy K. Kloss, Michele Bini
 *
 * You should have received a copy of the license along with this program.
 */

var core = require('./core');
var utils = require('./utils');

    /**
     * @exports jodid25519/curve255
     * Legacy compatibility module for Michele Bini's previous curve255.js.
     *
     * @description
     * Legacy compatibility module for Michele Bini's previous curve255.js.
     *
     * <p>
     * This code presents an API with all key formats as previously available
     * from Michele Bini's curve255.js implementation.
     * </p>
     */
    var ns = {};

    function curve25519_raw(f, c) {
        var a, x_1, q;

        x_1 = c;
        a = core.dbl(x_1, core.ONE());
        q = [x_1, core.ONE()];

        var n = 255;

        while (core.getbit(f, n) == 0) {
            n--;
            // For correct constant-time operation, bit 255 should always be
            // set to 1 so the following 'while' loop is never entered.
            if (n < 0) {
                return core.ZERO();
            }
        }
        n--;

        var aq = [a, q];

        while (n >= 0) {
            var r, s;
            var b = core.getbit(f, n);
            r = core.sum(aq[0][0], aq[0][1], aq[1][0], aq[1][1], x_1);
            s = core.dbl(aq[1 - b][0], aq[1 - b][1]);
            aq[1 - b] = s;
            aq[b] = r;
            n--;
        }
        q = aq[1];

        q[1] = core.invmodp(q[1]);
        q[0] = core.mulmodp(q[0], q[1]);
        core.reduce(q[0]);
        return q[0];
    }

    function curve25519b32(a, b) {
        return _base32encode(curve25519(_base32decode(a),
                                        _base32decode(b)));
    }

    function curve25519(f, c) {
        if (!c) {
            c = core.BASE();
        }
        f[0] &= 0xFFF8;
        f[15] = (f[15] & 0x7FFF) | 0x4000;
        return curve25519_raw(f, c);
    }

    function _hexEncodeVector(k) {
        var hexKey = utils.hexEncode(k);
        // Pad with '0' at the front.
        hexKey = new Array(64 + 1 - hexKey.length).join('0') + hexKey;
        // Invert bytes.
        return hexKey.split(/(..)/).reverse().join('');
    }

    function _hexDecodeVector(v) {
        // assert(length(x) == 64);
        // Invert bytes.
        var hexKey = v.split(/(..)/).reverse().join('');
        return utils.hexDecode(hexKey);
    }


    // Expose some functions to the outside through this name space.

    /**
     * Computes the scalar product of a point on the curve 25519.
     *
     * This function is used for the DH key-exchange protocol.
     *
     * Before multiplication, some bit operations are applied to the
     * private key to ensure it is a valid Curve25519 secret key.
     * It is the user's responsibility to make sure that the private
     * key is a uniformly random, secret value.
     *
     * @function
     * @param f {array}
     *     Private key.
     * @param c {array}
     *     Public point on the curve. If not given, the curve's base point is used.
     * @returns {array}
     *     Key point resulting from scalar product.
     */
    ns.curve25519 = curve25519;

    /**
     * Computes the scalar product of a point on the curve 25519.
     *
     * This variant does not make sure that the private key is valid.
     * The user has the responsibility to ensure the private key is
     * valid or that this results in a safe protocol.  Unless you know
     * exactly what you are doing, you should not use this variant,
     * please use 'curve25519' instead.
     *
     * @function
     * @param f {array}
     *     Private key.
     * @param c {array}
     *     Public point on the curve. If not given, the curve's base point is used.
     * @returns {array}
     *     Key point resulting from scalar product.
     */
    ns.curve25519_raw = curve25519_raw;

    /**
     * Encodes the internal representation of a key to a canonical hex
     * representation.
     *
     * This is the format commonly used in other libraries and for
     * test vectors, and is equivalent to the hex dump of the key in
     * little-endian binary format.
     *
     * @function
     * @param n {array}
     *     Array representation of key.
     * @returns {string}
     *     Hexadecimal string representation of key.
     */
    ns.hexEncodeVector = _hexEncodeVector;

    /**
     * Decodes a canonical hex representation of a key
     * to an internally compatible array representation.
     *
     * @function
     * @param n {string}
     *     Hexadecimal string representation of key.
     * @returns {array}
     *     Array representation of key.
     */
    ns.hexDecodeVector = _hexDecodeVector;

    /**
     * Encodes the internal representation of a key into a
     * hexadecimal representation.
     *
     * This is a strict positional notation, most significant digit first.
     *
     * @function
     * @param n {array}
     *     Array representation of key.
     * @returns {string}
     *     Hexadecimal string representation of key.
     */
    ns.hexencode = utils.hexEncode;

    /**
     * Decodes a hex representation of a key to an internally
     * compatible array representation.
     *
     * @function
     * @param n {string}
     *     Hexadecimal string representation of key.
     * @returns {array}
     *     Array representation of key.
     */
    ns.hexdecode = utils.hexDecode;

    /**
     * Encodes the internal representation of a key to a base32
     * representation.
     *
     * @function
     * @param n {array}
     *     Array representation of key.
     * @returns {string}
     *     Base32 string representation of key.
     */
    ns.base32encode = utils.base32encode;

    /**
     * Decodes a base32 representation of a key to an internally
     * compatible array representation.
     *
     * @function
     * @param n {string}
     *     Base32 string representation of key.
     * @returns {array}
     *     Array representation of key.
     */
    ns.base32decode = utils.base32decode;

module.exports = ns;
