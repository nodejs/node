var __extends = this && this.t || function() {
    var extendStatics = function(r, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(r, t) {
            r.__proto__ = t;
        } || function(r, t) {
            for (var e in t) if (Object.prototype.hasOwnProperty.call(t, e)) r[e] = t[e];
        };
        return extendStatics(r, t);
    };
    return function(r, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(r, t);
        function __() {
            this.constructor = r;
        }
        r.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

import { ContainerIterator } from "../../ContainerBase";

import { throwIteratorAccessError } from "../../../utils/throwError";

var TreeIterator = function(r) {
    __extends(TreeIterator, r);
    function TreeIterator(t, e, i) {
        var n = r.call(this, i) || this;
        n.o = t;
        n.u = e;
        if (n.iteratorType === 0) {
            n.pre = function() {
                if (this.o === this.u.Y) {
                    throwIteratorAccessError();
                }
                this.o = this.o.L();
                return this;
            };
            n.next = function() {
                if (this.o === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m();
                return this;
            };
        } else {
            n.pre = function() {
                if (this.o === this.u.Z) {
                    throwIteratorAccessError();
                }
                this.o = this.o.m();
                return this;
            };
            n.next = function() {
                if (this.o === this.u) {
                    throwIteratorAccessError();
                }
                this.o = this.o.L();
                return this;
            };
        }
        return n;
    }
    Object.defineProperty(TreeIterator.prototype, "index", {
        get: function() {
            var r = this.o;
            var t = this.u.sr;
            if (r === this.u) {
                if (t) {
                    return t.hr - 1;
                }
                return 0;
            }
            var e = 0;
            if (r.Y) {
                e += r.Y.hr;
            }
            while (r !== t) {
                var i = r.sr;
                if (r === i.Z) {
                    e += 1;
                    if (i.Y) {
                        e += i.Y.hr;
                    }
                }
                r = i;
            }
            return e;
        },
        enumerable: false,
        configurable: true
    });
    return TreeIterator;
}(ContainerIterator);

export default TreeIterator;
//# sourceMappingURL=TreeIterator.js.map
