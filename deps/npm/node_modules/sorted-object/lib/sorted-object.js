"use strict";

module.exports = function (input) {
    var output = {};

    Object.keys(input).sort().forEach(function (key) {
        output[key] = input[key];
    });

    return output;
};
