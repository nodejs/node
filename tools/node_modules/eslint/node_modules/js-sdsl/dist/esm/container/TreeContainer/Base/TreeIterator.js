var __extends = this && this.t || function() {
    var extendStatics = function(t, r) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, r) {
            t.__proto__ = r;
        } || function(t, r) {
            for (var e in r) if (Object.prototype.hasOwnProperty.call(r, e)) t[e] = r[e];
        };
        return extendStatics(t, r);
    };
    return function(t, r) {
        if (typeof r !== "function" && r !== null) throw new TypeError("Class extends value " + String(r) + " is not a constructor or null");
        extendStatics(t, r);
        function __() {
            this.constructor = t;
        }
        t.prototype = r === null ? Object.create(r) : (__.prototype = r.prototype, new __);
    };
}();

import { ContainerIterator } from "../../ContainerBase";

var TreeIterator = function(t) {
    __extends(TreeIterator, t);
    function TreeIterator(r, e, n) {
        var i = t.call(this, n) || this;
        i.D = r;
        i.J = e;
        if (i.iteratorType === 0) {
            i.pre = function() {
                if (this.D === this.J.Y) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.D = this.D.pre();
                return this;
            };
            i.next = function() {
                if (this.D === this.J) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.D = this.D.next();
                return this;
            };
        } else {
            i.pre = function() {
                if (this.D === this.J.Z) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.D = this.D.next();
                return this;
            };
            i.next = function() {
                if (this.D === this.J) {
                    throw new RangeError("Tree iterator access denied!");
                }
                this.D = this.D.pre();
                return this;
            };
        }
        return i;
    }
    Object.defineProperty(TreeIterator.prototype, "index", {
        get: function() {
            var t = this.D;
            var r = this.J.tt;
            if (t === this.J) {
                if (r) {
                    return r.rt - 1;
                }
                return 0;
            }
            var e = 0;
            if (t.Y) {
                e += t.Y.rt;
            }
            while (t !== r) {
                var n = t.tt;
                if (t === n.Z) {
                    e += 1;
                    if (n.Y) {
                        e += n.Y.rt;
                    }
                }
                t = n;
            }
            return e;
        },
        enumerable: false,
        configurable: true
    });
    TreeIterator.prototype.equals = function(t) {
        return this.D === t.D;
    };
    return TreeIterator;
}(ContainerIterator);

export default TreeIterator;