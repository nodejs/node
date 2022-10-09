"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = void 0;

var _ContainerBase = require("../../ContainerBase");

class HashContainer extends _ContainerBase.Base {
    constructor(e = 16, t = (e => {
        let t;
        if (typeof e !== "string") {
            t = JSON.stringify(e);
        } else t = e;
        let r = 0;
        const s = t.length;
        for (let e = 0; e < s; e++) {
            const s = t.charCodeAt(e);
            r = (r << 5) - r + s;
            r |= 0;
        }
        return r >>> 0;
    })) {
        super();
        if (e < 16 || (e & e - 1) !== 0) {
            throw new RangeError("InitBucketNum range error");
        }
        this.u = this.te = e;
        this.l = t;
    }
    clear() {
        this.o = 0;
        this.u = this.te;
        this.i = [];
    }
}

var _default = HashContainer;

exports.default = _default;