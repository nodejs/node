"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.sync = void 0;
const walker_1 = require("./walker");
function sync(root, options) {
    const walker = new walker_1.Walker(root, options);
    return walker.start();
}
exports.sync = sync;
