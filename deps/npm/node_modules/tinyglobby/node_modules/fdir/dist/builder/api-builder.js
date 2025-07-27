"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.APIBuilder = void 0;
const async_1 = require("../api/async");
const sync_1 = require("../api/sync");
class APIBuilder {
    root;
    options;
    constructor(root, options) {
        this.root = root;
        this.options = options;
    }
    withPromise() {
        return (0, async_1.promise)(this.root, this.options);
    }
    withCallback(cb) {
        (0, async_1.callback)(this.root, this.options, cb);
    }
    sync() {
        return (0, sync_1.sync)(this.root, this.options);
    }
}
exports.APIBuilder = APIBuilder;
