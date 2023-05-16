"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.ceil = ceil;

exports.floor = void 0;

function ceil(e, t) {
    return Math.floor((e + t - 1) / t);
}

const floor = Math.floor;

exports.floor = floor;
//# sourceMappingURL=math.js.map
