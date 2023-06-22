"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
function fullJitter(delay) {
    var jitteredDelay = Math.random() * delay;
    return Math.round(jitteredDelay);
}
exports.fullJitter = fullJitter;
//# sourceMappingURL=full.jitter.js.map