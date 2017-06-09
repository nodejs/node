"use strict";
/**
 * @fileOverview
 * Digital signature scheme based on Curve25519 (Ed25519 or EdDSA).
 */

/*
 * Copyright (c) 2011, 2012, 2014 Ron Garret
 * Copyright (c) 2014 Mega Limited
 * under the MIT License.
 *
 * Authors: Guy K. Kloss, Ron Garret
 *
 * You should have received a copy of the license along with this program.
 */

var core = require('./core');
var curve255 = require('./curve255');
var utils = require('./utils');
var BigInteger = require('jsbn').BigInteger;
var crypto = require('crypto');

    /**
     * @exports jodid25519/eddsa
     * Digital signature scheme based on Curve25519 (Ed25519 or EdDSA).
     *
     * @description
     * Digital signature scheme based on Curve25519 (Ed25519 or EdDSA).
     *
     * <p>
     * This code is adapted from fast-djbec.js, a faster but more complicated
     * version of the Ed25519 encryption scheme (as compared to djbec.js).
     * It uses two different representations for big integers: The jsbn
     * BigInteger class, which can represent arbitrary-length numbers, and a
     * special fixed-length representation optimised for 256-bit integers.
     * The reason both are needed is that the Ed25519 algorithm requires some
     * 512-bit numbers.</p>
    */
    var ns = {};

    function _bi255(value) {
        if (!(this instanceof _bi255)) {
            return new _bi255(value);
        }
        if (typeof value === 'undefined') {
            return _ZERO;
        }
        var c = value.constructor;
        if ((c === Array || c === Uint16Array || c === Uint32Array) && (value.length === 16)) {
            this.n = value;
        } else if ((c === Array) && (value.length === 32)) {
            this.n = _bytes2bi255(value).n;
        } else if (c === String) {
            this.n = utils.hexDecode(value);
        } else if (c === Number) {
            this.n = [value & 0xffff,
                      value >> 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        } else if (value instanceof _bi255) {
            this.n = value.n.slice(0); // Copy constructor
        } else {
            throw "Bad argument for bignum: " + value;
        }
    }

   _bi255.prototype = {
        'toString' : function() {
            return utils.hexEncode(this.n);
        },
        'toSource' : function() {
            return '_' + utils.hexEncode(this.n);
        },
        'plus' : function(n1) {
            return _bi255(core.bigintadd(this.n, n1.n));
        },
        'minus' : function(n1) {
            return _bi255(core.bigintsub(this.n, n1.n)).modq();
        },
        'times' : function(n1) {
            return _bi255(core.mulmodp(this.n, n1.n));
        },
        'divide' : function(n1) {
            return this.times(n1.inv());
        },
        'sqr' : function() {
            return _bi255(core.sqrmodp(this.n));
        },
        'cmp' : function(n1) {
            return core.bigintcmp(this.n, n1.n);
        },
        'equals' : function(n1) {
            return this.cmp(n1) === 0;
        },
        'isOdd' : function() {
            return (this.n[0] & 1) === 1;
        },
        'shiftLeft' : function(cnt) {
            _shiftL(this.n, cnt);
            return this;
        },
        'shiftRight' : function(cnt) {
            _shiftR(this.n, cnt);
            return this;
        },
        'inv' : function() {
            return _bi255(core.invmodp(this.n));
        },
        'pow' : function(e) {
            return _bi255(_pow(this.n, e.n));
        },
        'modq' : function() {
            return _modq(this);
        },
        'bytes' : function() {
            return _bi255_bytes(this);
        }
    };

    function _shiftL(n, cnt) {
        var lastcarry = 0;
        for (var i = 0; i < 16; i++) {
            var carry = n[i] >> (16 - cnt);
            n[i] = (n[i] << cnt) & 0xffff | lastcarry;
            lastcarry = carry;
        }
        return n;
    }

    function _shiftR(n, cnt) {
        var lastcarry = 0;
        for (var i = 15; i >= 0; i--) {
            var carry = n[i] << (16 - cnt) & 0xffff;
            n[i] = (n[i] >> cnt) | lastcarry;
            lastcarry = carry;
        }
        return n;
    }

    function _bi255_bytes(n) {
        n = _bi255(n); // Make a copy because shiftRight is destructive
        var a = new Array(32);
        for (var i = 31; i >= 0; i--) {
            a[i] = n.n[0] & 0xff;
            n.shiftRight(8);
        }
        return a;
    }

    function _bytes2bi255(a) {
        var n = _ZERO;
        for (var i = 0; i < 32; i++) {
            n.shiftLeft(8);
            n = n.plus(_bi255(a[i]));
        }
        return n;
    }

    function _pow(n, e) {
        var result = core.ONE();
        for (var i = 0; i < 256; i++) {
            if (core.getbit(e, i) === 1) {
                result = core.mulmodp(result, n);
            }
            n = core.sqrmodp(n);
        }
        return result;
    }

    var _ZERO = _bi255(0);
    var _ONE = _bi255(1);
    var _TWO = _bi255(2);
    // This is the core prime.
    var _Q = _bi255([0xffff - 18, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                     0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
                     0xffff, 0xffff, 0x7fff]);

    function _modq(n) {
        core.reduce(n.n);
        if (n.cmp(_Q) >= 0) {
            return _modq(n.minus(_Q));
        }
        if (n.cmp(_ZERO) === -1) {
            return _modq(n.plus(_Q));
        } else {
            return n;
        }
    }

    // _RECOVERY_EXPONENT = _Q.plus(_bi255(3)).divide(_bi255(8));
    var _RECOVERY_EXPONENT = _bi255('0ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe');
    // _D = _Q.minus(_bi255(121665)).divide(_bi255(121666));
    var _D = _bi255('52036cee2b6ffe738cc740797779e89800700a4d4141d8ab75eb4dca135978a3');
    // _I = _TWO.pow(_Q.minus(_ONE).divide(_bi255(4)));
    var _I = _bi255('2b8324804fc1df0b2b4d00993dfbd7a72f431806ad2fe478c4ee1b274a0ea0b0');
    // _L = _TWO.pow(_bi255(252)).plus(_bi255('14def9dea2f79cd65812631a5cf5d3ed'));
    var _L = _bi255('1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed');
    var _L_BI = _bi('1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed', 16);


    // ////////////////////////////////////////////////////////////

    function _isoncurve(p) {
        var x = p[0];
        var y = p[1];
        var xsqr = x.sqr();
        var ysqr = y.sqr();
        var v = _D.times(xsqr).times(ysqr);
        return ysqr.minus(xsqr).minus(_ONE).minus(v).modq().equals(_ZERO);
    }

    function _xrecover(y) {
        var ysquared = y.sqr();
        var xx = ysquared.minus(_ONE).divide(_ONE.plus(_D.times(ysquared)));
        var x = xx.pow(_RECOVERY_EXPONENT);
        if (!(x.times(x).minus(xx).equals(_ZERO))) {
            x = x.times(_I);
        }
        if (x.isOdd()) {
            x = _Q.minus(x);
        }
        return x;
    }

    function _x_pt_add(pt1, pt2) {
        var x1 = pt1[0];
        var y1 = pt1[1];
        var z1 = pt1[2];
        var t1 = pt1[3];
        var x2 = pt2[0];
        var y2 = pt2[1];
        var z2 = pt2[2];
        var t2 = pt2[3];
        var A = y1.minus(x1).times(y2.plus(x2));
        var B = y1.plus(x1).times(y2.minus(x2));
        var C = z1.times(_TWO).times(t2);
        var D = t1.times(_TWO).times(z2);
        var E = D.plus(C);
        var F = B.minus(A);
        var G = B.plus(A);
        var H = D.minus(C);
        return [E.times(F), G.times(H), F.times(G), E.times(H)];
    }

    function _xpt_double(pt1) {
        var x1 = pt1[0];
        var y1 = pt1[1];
        var z1 = pt1[2];
        var A = x1.times(x1);
        var B = y1.times(y1);
        var C = _TWO.times(z1).times(z1);
        var D = _Q.minus(A);
        var J = x1.plus(y1);
        var E = J.times(J).minus(A).minus(B);
        var G = D.plus(B);
        var F = G.minus(C);
        var H = D.minus(B);
        return [E.times(F), G.times(H), F.times(G), E.times(H)];
    }

    function _xpt_mult(pt, n) {
        if (n.equals(_ZERO)) {
            return [_ZERO, _ONE, _ONE, _ZERO];
        }
        var odd = n.isOdd();
        n.shiftRight(1);
        var value = _xpt_double(_xpt_mult(pt, n));
        return odd ? _x_pt_add(value, pt) : value;
    }

    function _pt_xform(pt) {
        var x = pt[0];
        var y = pt[1];
        return [x, y, _ONE, x.times(y)];
    }

    function _pt_unxform(pt) {
        var x = pt[0];
        var y = pt[1];
        var z = pt[2];
        var invz = z.inv();
        return [x.times(invz), y.times(invz)];
    }

    function _scalarmult(pt, n) {
        return _pt_unxform(_xpt_mult(_pt_xform(pt), n));
    }

    function _bytesgetbit(bytes, n) {
        return (bytes[bytes.length - (n >>> 3) - 1] >> (n & 7)) & 1;
    }

    function _xpt_mult_bytes(pt, bytes) {
        var r = [_ZERO, _ONE, _ONE, _ZERO];
        for (var i = (bytes.length << 3) - 1; i >= 0; i--) {
            r = _xpt_double(r);
            if (_bytesgetbit(bytes, i) === 1) {
                r = _x_pt_add(r, pt);
            }
        }
        return r;
    }

    function _scalarmultBytes(pt, bytes) {
        return _pt_unxform(_xpt_mult_bytes(_pt_xform(pt), bytes));
    }

    var _by = _bi255(4).divide(_bi255(5));
    var _bx = _xrecover(_by);
    var _bp = [_bx, _by];

    function _encodeint(n) {
        return n.bytes(32).reverse();
    }
    function _decodeint(b) {
        return _bi255(b.slice(0).reverse());
    }

    function _encodepoint(p) {
        var v = _encodeint(p[1]);
        if (p[0].isOdd()) {
            v[31] |= 0x80;
        }
        return v;
    }

    function _decodepoint(v) {
        v = v.slice(0);
        var signbit = v[31] >> 7;
        v[31] &= 127;
        var y = _decodeint(v);
        var x = _xrecover(y);
        if ((x.n[0] & 1) !== signbit) {
            x = _Q.minus(x);
        }
        var p = [x, y];
        if (!_isoncurve(p)) {
            throw ('Point is not on curve');
        }
        return p;
    }

    // //////////////////////////////////////////////////

    /**
     * Factory function to create a suitable BigInteger.
     *
     * @param value
     *     The value for the big integer.
     * @param base {integer}
     *     Base of the conversion of elements in ``value``.
     * @returns
     *     A BigInteger object.
     */
    function _bi(value, base) {
        if (base !== undefined) {
            if (base === 256) {
                return _bi(utils.string2bytes(value));
            }
            return new BigInteger(value, base);
        } else if (typeof value === 'string') {
            return new BigInteger(value, 10);
        } else if ((value instanceof Array) || (value instanceof Uint8Array)
          || Buffer.isBuffer(value)) {
            return new BigInteger(value);
        } else if (typeof value === 'number') {
            return new BigInteger(value.toString(), 10);
        } else {
            throw "Can't convert " + value + " to BigInteger";
        }
    }

    function _bi2bytes(n, cnt) {
        if (cnt === undefined) {
            cnt = (n.bitLength() + 7) >>> 3;
        }
        var bytes = new Array(cnt);
        for (var i = cnt - 1; i >= 0; i--) {
            bytes[i] = n[0] & 255; // n.and(0xff);
            n = n.shiftRight(8);
        }
        return bytes;
    }

    BigInteger.prototype.bytes = function(n) {
        return _bi2bytes(this, n);
    };

    // /////////////////////////////////////////////////////////

    function _bytehash(s) {
        var sha = crypto.createHash('sha512').update(s).digest();
        return _bi2bytes(_bi(sha), 64).reverse();
    }

    function _stringhash(s) {
        var sha = crypto.createHash('sha512').update(s).digest();
        return _map(_chr, _bi2bytes(_bi(sha), 64)).join('');
    }

    function _inthash(s) {
        // Need a leading 0 to prevent sign extension
        return _bi([0].concat(_bytehash(s)));
    }

    function _inthash_lo(s) {
        return _bi255(_bytehash(s).slice(32, 64));
    }

    function _inthash_mod_l(s) {
        return _inthash(s).mod(_L_BI);
    }

    function _get_a(sk) {
        var a = _inthash_lo(sk);
        a.n[0] &= 0xfff8;
        a.n[15] &= 0x3fff;
        a.n[15] |= 0x4000;
        return a;
    }

    function _publickey(sk) {
        return _encodepoint(_scalarmult(_bp, _get_a(sk)));
    }

    function _map(f, l) {
        var result = new Array(l.length);
        for (var i = 0; i < l.length; i++) {
            result[i] = f(l[i]);
        }
        return result;
    }

    function _chr(n) {
        return String.fromCharCode(n);
    }

    function _ord(c) {
        return c.charCodeAt(0);
    }

    function _pt_add(p1, p2) {
        return _pt_unxform(_x_pt_add(_pt_xform(p1), _pt_xform(p2)));
    }


    // Exports for the API.

    /**
     * Checks whether a point is on the curve.
     *
     * @function
     * @param point {string}
     *     The point to check for in a byte string representation.
     * @returns {boolean}
     *     true if the point is on the curve, false otherwise.
     */
    ns.isOnCurve = function(point) {
        try {
            _isoncurve(_decodepoint(utils.string2bytes(point)));
        } catch(e) {
            if (e === 'Point is not on curve') {
                return false;
            } else {
                throw e;
            }
        }
        return true;
    };


    /**
     * Computes the EdDSA public key.
     *
     * <p>Note: Seeds should be a byte string, not a unicode string containing
     * multi-byte characters.</p>
     *
     * @function
     * @param keySeed {string}
     *     Private key seed in the form of a byte string.
     * @returns {string}
     *     Public key as byte string computed from the private key seed
     *     (32 bytes).
     */
    ns.publicKey = function(keySeed) {
        return utils.bytes2string(_publickey(keySeed));
    };


    /**
     * Computes an EdDSA signature of a message.
     *
     * <p>Notes:</p>
     *
     * <ul>
     *   <li>Unicode messages need to be converted to a byte representation
     *   (e. g. UTF-8).</li>
     *   <li>If `publicKey` is given, and it is *not* a point of the curve,
     *   the signature will be faulty, but no error will be thrown.</li>
     * </ul>
     *
     * @function
     * @param message {string}
     *     Message in the form of a byte string.
     * @param keySeed {string}
     *     Private key seed in the form of a byte string.
     * @param publicKey {string}
     *     Public key as byte string (if not present, it will be computed from
     *     the private key seed).
     * @returns {string}
     *     Detached message signature in the form of a byte string (64 bytes).
     */
    ns.sign = function(message, keySeed, publicKey) {
        if (publicKey === undefined) {
            publicKey = _publickey(keySeed);
        } else {
            publicKey = utils.string2bytes(publicKey);
        }
        var a = _bi(_get_a(keySeed).toString(), 16);
        var hs = _stringhash(keySeed);
        var r = _bytehash(hs.slice(32, 64) + message);
        var rp = _scalarmultBytes(_bp, r);
        var erp = _encodepoint(rp);
        r = _bi(r).mod(_bi(1, 10).shiftLeft(512));
        var s = _map(_chr, erp).join('') + _map(_chr, publicKey).join('') + message;
        s = _inthash_mod_l(s).multiply(a).add(r).mod(_L_BI);
        return utils.bytes2string(erp.concat(_encodeint(s)));
    };


    /**
     * Verifies an EdDSA signature of a message with the public key.
     *
     * <p>Note: Unicode messages need to be converted to a byte representation
     * (e. g. UTF-8).</p>
     *
     * @function
     * @param signature {string}
     *     Message signature in the form of a byte string. Can be detached
     *     (64 bytes), or attached to be sliced off.
     * @param message {string}
     *     Message in the form of a byte string.
     * @param publicKey {string}
     *     Public key as byte string (if not present, it will be computed from
     *     the private key seed).
     * @returns {boolean}
     *     true, if the signature verifies.
     */
    ns.verify = function(signature, message, publicKey) {
        signature = utils.string2bytes(signature.slice(0, 64));
        publicKey = utils.string2bytes(publicKey);
        var rpe = signature.slice(0, 32);
        var rp = _decodepoint(rpe);
        var a = _decodepoint(publicKey);
        var s = _decodeint(signature.slice(32, 64));
        var h = _inthash(utils.bytes2string(rpe.concat(publicKey)) + message);
        var v1 = _scalarmult(_bp, s);
        var value = _scalarmultBytes(a, _bi2bytes(h));
        var v2 = _pt_add(rp, value);
        return v1[0].equals(v2[0]) && v1[1].equals(v2[1]);
    };


    /**
     * Generates a new random private key seed of 32 bytes length (256 bit).
     *
     * @function
     * @returns {string}
     *     Byte string containing a new random private key seed.
     */
    ns.generateKeySeed = function() {
        return core.generateKey(false);
    };

module.exports = ns;
