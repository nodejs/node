"use strict";
var __assign = (this && this.__assign) || function () {
    __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
                t[p] = s[p];
        }
        return t;
    };
    return __assign.apply(this, arguments);
};
Object.defineProperty(exports, "__esModule", { value: true });
var defaultOptions = {
    delayFirstAttempt: false,
    jitter: "none",
    maxDelay: Infinity,
    numOfAttempts: 10,
    retry: function () { return true; },
    startingDelay: 100,
    timeMultiple: 2
};
function getSanitizedOptions(options) {
    var sanitized = __assign(__assign({}, defaultOptions), options);
    if (sanitized.numOfAttempts < 1) {
        sanitized.numOfAttempts = 1;
    }
    return sanitized;
}
exports.getSanitizedOptions = getSanitizedOptions;
//# sourceMappingURL=options.js.map