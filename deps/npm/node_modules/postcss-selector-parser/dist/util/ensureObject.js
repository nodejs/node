"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.default = ensureObject;
function ensureObject(obj) {
    var props = [];
    for (var _i = 1; _i < arguments.length; _i++) {
        props[_i - 1] = arguments[_i];
    }
    while (props.length > 0) {
        var prop = props.shift();
        if (!obj[prop]) {
            obj[prop] = {};
        }
        obj = obj[prop];
    }
}
//# sourceMappingURL=ensureObject.js.map