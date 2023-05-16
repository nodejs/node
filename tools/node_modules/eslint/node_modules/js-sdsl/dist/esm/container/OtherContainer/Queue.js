var __extends = this && this.t || function() {
    var extendStatics = function(t, i) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(t, i) {
            t.__proto__ = i;
        } || function(t, i) {
            for (var n in i) if (Object.prototype.hasOwnProperty.call(i, n)) t[n] = i[n];
        };
        return extendStatics(t, i);
    };
    return function(t, i) {
        if (typeof i !== "function" && i !== null) throw new TypeError("Class extends value " + String(i) + " is not a constructor or null");
        extendStatics(t, i);
        function __() {
            this.constructor = t;
        }
        t.prototype = i === null ? Object.create(i) : (__.prototype = i.prototype, new __);
    };
}();

import { Base } from "../ContainerBase";

var Queue = function(t) {
    __extends(Queue, t);
    function Queue(i) {
        if (i === void 0) {
            i = [];
        }
        var n = t.call(this) || this;
        n.C = 0;
        n.T = [];
        var e = n;
        i.forEach((function(t) {
            e.push(t);
        }));
        return n;
    }
    Queue.prototype.clear = function() {
        this.T = [];
        this.i = this.C = 0;
    };
    Queue.prototype.push = function(t) {
        var i = this.T.length;
        if (this.C / i > .5 && this.C + this.i >= i && i > 4096) {
            var n = this.i;
            for (var e = 0; e < n; ++e) {
                this.T[e] = this.T[this.C + e];
            }
            this.C = 0;
            this.T[this.i] = t;
        } else this.T[this.C + this.i] = t;
        return ++this.i;
    };
    Queue.prototype.pop = function() {
        if (this.i === 0) return;
        var t = this.T[this.C++];
        this.i -= 1;
        return t;
    };
    Queue.prototype.front = function() {
        if (this.i === 0) return;
        return this.T[this.C];
    };
    return Queue;
}(Base);

export default Queue;
//# sourceMappingURL=Queue.js.map
