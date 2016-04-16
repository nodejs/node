'use strict';

var util = require("util");

function toString(val) {
    val = val.toString(8);
    while (val.length < 4) {
        val = "0" + val;
    }
    return val;
}

var defaultUmask = 18; // 0022;
var defaultUmaskString = toString(defaultUmask);

function validate(data, k, val) {
    // must be either an integer or an octal string.
    if (typeof val === "number" && !isNaN(val)) {
        data[k] = val;
        return true;
    }

    if (typeof val === "string") {
        if (val.charAt(0) !== "0") {
            return false;
        }
        data[k] = parseInt(val, 8);
        return true;
    }

    return false;
}

function convert_fromString(val, cb) {
    if (typeof val === "string") {
        // check for octal string first
        if (val.charAt(0) === '0' && /^[0-7]+$/.test(val)) {
            val = parseInt(val, 8);
        } else if (val.charAt(0) !== '0' && /^[0-9]+$/.test(val)) {
            // legacy support for decimal strings
            val = parseInt(val, 10);
        } else {
            return cb(new Error(util.format("Expected octal string, got %j, defaulting to %j",
                                            val, defaultUmaskString)),
                      defaultUmask);
        }
    } else if (typeof val !== "number") {
        return cb(new Error(util.format("Expected number or octal string, got %j, defaulting to %j",
                                        val, defaultUmaskString)),
                  defaultUmask);
    }

    val = Math.floor(val);

    if ((val < 0) || (val > 511)) {
        return cb(new Error(util.format("Must be in range 0..511 (0000..0777), got %j", val)),
                  defaultUmask);
    }

    cb(null, val);
}

function fromString(val, cb) {

    // synchronous callback, no zalgo
    convert_fromString(val, cb || function (err, result) {
        /*jslint unparam:true*/
        val = result;
    });

    return val;
}

exports.toString = toString;
exports.fromString = fromString;
exports.validate = validate;

