var __extends = this && this.t || function() {
    var extendStatics = function(e, t) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, t) {
            e.__proto__ = t;
        } || function(e, t) {
            for (var n in t) if (Object.prototype.hasOwnProperty.call(t, n)) e[n] = t[n];
        };
        return extendStatics(e, t);
    };
    return function(e, t) {
        if (typeof t !== "function" && t !== null) throw new TypeError("Class extends value " + String(t) + " is not a constructor or null");
        extendStatics(e, t);
        function __() {
            this.constructor = e;
        }
        e.prototype = t === null ? Object.create(t) : (__.prototype = t.prototype, new __);
    };
}();

var TreeNode = function() {
    function TreeNode(e, t, n) {
        if (n === void 0) {
            n = 1;
        }
        this.Y = undefined;
        this.Z = undefined;
        this.sr = undefined;
        this.p = e;
        this.H = t;
        this.ee = n;
    }
    TreeNode.prototype.L = function() {
        var e = this;
        if (e.ee === 1 && e.sr.sr === e) {
            e = e.Z;
        } else if (e.Y) {
            e = e.Y;
            while (e.Z) {
                e = e.Z;
            }
        } else {
            var t = e.sr;
            while (t.Y === e) {
                e = t;
                t = e.sr;
            }
            e = t;
        }
        return e;
    };
    TreeNode.prototype.m = function() {
        var e = this;
        if (e.Z) {
            e = e.Z;
            while (e.Y) {
                e = e.Y;
            }
            return e;
        } else {
            var t = e.sr;
            while (t.Z === e) {
                e = t;
                t = e.sr;
            }
            if (e.Z !== t) {
                return t;
            } else return e;
        }
    };
    TreeNode.prototype.te = function() {
        var e = this.sr;
        var t = this.Z;
        var n = t.Y;
        if (e.sr === this) e.sr = t; else if (e.Y === this) e.Y = t; else e.Z = t;
        t.sr = e;
        t.Y = this;
        this.sr = t;
        this.Z = n;
        if (n) n.sr = this;
        return t;
    };
    TreeNode.prototype.ne = function() {
        var e = this.sr;
        var t = this.Y;
        var n = t.Z;
        if (e.sr === this) e.sr = t; else if (e.Y === this) e.Y = t; else e.Z = t;
        t.sr = e;
        t.Z = this;
        this.sr = t;
        this.Y = n;
        if (n) n.sr = this;
        return t;
    };
    return TreeNode;
}();

export { TreeNode };

var TreeNodeEnableIndex = function(e) {
    __extends(TreeNodeEnableIndex, e);
    function TreeNodeEnableIndex() {
        var t = e !== null && e.apply(this, arguments) || this;
        t.hr = 1;
        return t;
    }
    TreeNodeEnableIndex.prototype.te = function() {
        var t = e.prototype.te.call(this);
        this.ie();
        t.ie();
        return t;
    };
    TreeNodeEnableIndex.prototype.ne = function() {
        var t = e.prototype.ne.call(this);
        this.ie();
        t.ie();
        return t;
    };
    TreeNodeEnableIndex.prototype.ie = function() {
        this.hr = 1;
        if (this.Y) {
            this.hr += this.Y.hr;
        }
        if (this.Z) {
            this.hr += this.Z.hr;
        }
    };
    return TreeNodeEnableIndex;
}(TreeNode);

export { TreeNodeEnableIndex };
//# sourceMappingURL=TreeNode.js.map
