var __extends = this && this.t || function() {
    var extendStatics = function(n, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(n, t) {
            n.__proto__ = t;
        } || function(n, t) {
            for (var r in t) if (Object.prototype.hasOwnProperty.call(t, r)) n[r] = t[r];
        };
        return extendStatics(n, t);
    };
    return function(n, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(n, t);
        function __() {
            this.constructor = n;
        }
        n.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

import { Base } from "../../ContainerBase";

var HashContainer = function(n) {
    __extends(HashContainer, n);
    function HashContainer(t, r) {
        if (t === void 0) {
            t = 16;
        }
        if (r === void 0) {
            r = function(n) {
                var t;
                if (typeof n !== "string") {
                    t = JSON.stringify(n);
                } else t = n;
                var r = 0;
                var i = t.length;
                for (var e = 0; e < i; e++) {
                    var o = t.charCodeAt(e);
                    r = (r << 5) - r + o;
                    r |= 0;
                }
                return r >>> 0;
            };
        }
        var i = n.call(this) || this;
        if (t < 16 || (t & t - 1) !== 0) {
            throw new RangeError("InitBucketNum range error");
        }
        i.l = i.nn = t;
        i.p = r;
        return i;
    }
    HashContainer.prototype.clear = function() {
        this.o = 0;
        this.l = this.nn;
        this.h = [];
    };
    return HashContainer;
}(Base);

export default HashContainer;