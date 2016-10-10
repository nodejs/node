"use strict";
/**
 * @fileOverview
 * EC Diffie-Hellman operations on Curve25519.
 */

/*
 * Copyright (c) 2014 Mega Limited
 * under the MIT License.
 *
 * Authors: Guy K. Kloss
 *
 * You should have received a copy of the license along with this program.
 */

var core = require('./core');
var utils = require('./utils');
var curve255 = require('./curve255');


    /**
     * @exports jodid25519/dh
     * EC Diffie-Hellman operations on Curve25519.
     *
     * @description
     * EC Diffie-Hellman operations on Curve25519.
     */
    var ns = {};


    function _toString(vector) {
        var u = new Uint16Array(vector);
        return (new Buffer(new Uint8Array(u.buffer)));
    }

    function _fromString(vector) {
        if (Buffer.isBuffer(vector)) {
            var u = new Uint8Array(vector);
            return (new Uint16Array(u.buffer));
        }

        var result = new Array(16);
        for (var i = 0, l = 0; i < vector.length; i += 2) {
            result[l] = (vector.charCodeAt(i + 1) << 8) | vector.charCodeAt(i);
            l++;
        }
        return result;
    }


    /**
     * Computes a key through scalar multiplication of a point on the curve 25519.
     *
     * This function is used for the DH key-exchange protocol. It computes a
     * key based on a secret key with a public component (opponent's public key
     * or curve base point if not given) by using scalar multiplication.
     *
     * Before multiplication, some bit operations are applied to the
     * private key to ensure it is a valid Curve25519 secret key.
     * It is the user's responsibility to make sure that the private
     * key is a uniformly random, secret value.
     *
     * @function
     * @param privateComponent {string}
     *     Private point as byte string on the curve.
     * @param publicComponent {string}
     *     Public point as byte string on the curve. If not given, the curve's
     *     base point is used.
     * @returns {string}
     *     Key point as byte string resulting from scalar product.
     */
    ns.computeKey = function(privateComponent, publicComponent) {
        if (publicComponent) {
            return _toString(curve255.curve25519(_fromString(privateComponent),
                                                 _fromString(publicComponent)));
        } else {
            return _toString(curve255.curve25519(_fromString(privateComponent)));
        }
    };

    /**
     * Computes the public key to a private key on the curve 25519.
     *
     * Before multiplication, some bit operations are applied to the
     * private key to ensure it is a valid Curve25519 secret key.
     * It is the user's responsibility to make sure that the private
     * key is a uniformly random, secret value.
     *
     * @function
     * @param privateKey {string}
     *     Private point as byte string on the curve.
     * @returns {string}
     *     Public key point as byte string resulting from scalar product.
     */
    ns.publicKey = function(privateKey) {
        return _toString(curve255.curve25519(_fromString(privateKey)));
    };


    /**
     * Generates a new random private key of 32 bytes length (256 bit).
     *
     * @function
     * @returns {string}
     *     Byte string containing a new random private key seed.
     */
    ns.generateKey = function() {
        return core.generateKey(true);
    };

module.exports = ns;
