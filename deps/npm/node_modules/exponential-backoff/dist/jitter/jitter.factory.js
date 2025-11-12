"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var full_jitter_1 = require("./full/full.jitter");
var no_jitter_1 = require("./no/no.jitter");
function JitterFactory(options) {
    switch (options.jitter) {
        case "full":
            return full_jitter_1.fullJitter;
        case "none":
        default:
            return no_jitter_1.noJitter;
    }
}
exports.JitterFactory = JitterFactory;
//# sourceMappingURL=jitter.factory.js.map