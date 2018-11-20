/**
 * @fileoverview Tracks performance of individual rules.
 * @author Brandon Mills
 * @copyright 2014 Brandon Mills. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/* istanbul ignore next */
function alignLeft(str, len, ch) {
    return str + new Array(len - str.length + 1).join(ch || " ");
}

/* istanbul ignore next */
function alignRight(str, len, ch) {
    return new Array(len - str.length + 1).join(ch || " ") + str;
}

//------------------------------------------------------------------------------
// Module definition
//------------------------------------------------------------------------------

var enabled = !!process.env.TIMING;

var HEADERS = ["Rule", "Time (ms)", "Relative"];
var ALIGN = [alignLeft, alignRight, alignRight];

/* istanbul ignore next */
function display(data) {
    var total = 0;
    var rows = Object.keys(data)
        .map(function(key) {
            var time = data[key];
            total += time;
            return [key, time];
        })
        .sort(function(a, b) {
            return b[1] - a[1];
        })
        .slice(0, 10);

    rows.forEach(function(row) {
        row.push((row[1] * 100 / total).toFixed(1) + "%");
        row[1] = row[1].toFixed(3);
    });

    rows.unshift(HEADERS);

    var widths = [];
    rows.forEach(function(row) {
        var len = row.length, i, n;
        for (i = 0; i < len; i++) {
            n = row[i].length;
            if (!widths[i] || n > widths[i]) {
                widths[i] = n;
            }
        }
    });

    var table = rows.map(function(row) {
        return row.map(function(cell, index) {
            return ALIGN[index](cell, widths[index]);
        }).join(" | ");
    });
    table.splice(1, 0, widths.map(function(w, index) {
        if (index !== 0 && index !== widths.length - 1) {
            w++;
        }

        return ALIGN[index](":", w + 1, "-");
    }).join("|"));

    console.log(table.join("\n"));
}

/* istanbul ignore next */
module.exports = (function() {

    var data = Object.create(null);

    function time(key, fn) {
        if (typeof data[key] === "undefined") {
            data[key] = 0;
        }

        return function() {
            var t = process.hrtime();
            fn.apply(null, Array.prototype.slice.call(arguments));
            t = process.hrtime(t);
            data[key] += t[0] * 1e3 + t[1] / 1e6;
        };
    }

    if (enabled) {
        process.on("exit", function() {
            display(data);
        });
    }

    return {
        time: time,
        enabled: enabled
    };

}());
