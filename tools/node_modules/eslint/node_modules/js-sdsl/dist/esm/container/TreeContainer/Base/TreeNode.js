var __extends = this && this.t || function() {
    var extendStatics = function(e, n) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, n) {
            e.__proto__ = n;
        } || function(e, n) {
            for (var t in n) if (Object.prototype.hasOwnProperty.call(n, t)) e[t] = n[t];
        };
        return extendStatics(e, n);
    };
    return function(e, n) {
        if (typeof n !== "function" && n !== null) throw new TypeError("Class extends value " + String(n) + " is not a constructor or null");
        extendStatics(e, n);
        function __() {
            this.constructor = e;
        }
        e.prototype = n === null ? Object.create(n) : (__.prototype = n.prototype, new __);
    };
}();

var TreeNode = function() {
    function TreeNode(e, n) {
        this.ee = 1;
        this.p = undefined;
        this.H = undefined;
        this.er = undefined;
        this.tr = undefined;
        this.hr = undefined;
        this.p = e;
        this.H = n;
    }
    TreeNode.prototype.W = function() {
        var e = this;
        if (e.ee === 1 && e.hr.hr === e) {
            e = e.tr;
        } else if (e.er) {
            e = e.er;
            while (e.tr) {
                e = e.tr;
            }
        } else {
            var n = e.hr;
            while (n.er === e) {
                e = n;
                n = e.hr;
            }
            e = n;
        }
        return e;
    };
    TreeNode.prototype.m = function() {
        var e = this;
        if (e.tr) {
            e = e.tr;
            while (e.er) {
                e = e.er;
            }
            return e;
        } else {
            var n = e.hr;
            while (n.tr === e) {
                e = n;
                n = e.hr;
            }
            if (e.tr !== n) {
                return n;
            } else return e;
        }
    };
    TreeNode.prototype.ne = function() {
        var e = this.hr;
        var n = this.tr;
        var t = n.er;
        if (e.hr === this) e.hr = n; else if (e.er === this) e.er = n; else e.tr = n;
        n.hr = e;
        n.er = this;
        this.hr = n;
        this.tr = t;
        if (t) t.hr = this;
        return n;
    };
    TreeNode.prototype.te = function() {
        var e = this.hr;
        var n = this.er;
        var t = n.tr;
        if (e.hr === this) e.hr = n; else if (e.er === this) e.er = n; else e.tr = n;
        n.hr = e;
        n.tr = this;
        this.hr = n;
        this.er = t;
        if (t) t.hr = this;
        return n;
    };
    return TreeNode;
}();

export { TreeNode };

var TreeNodeEnableIndex = function(e) {
    __extends(TreeNodeEnableIndex, e);
    function TreeNodeEnableIndex() {
        var n = e !== null && e.apply(this, arguments) || this;
        n.cr = 1;
        return n;
    }
    TreeNodeEnableIndex.prototype.ne = function() {
        var n = e.prototype.ne.call(this);
        this.ie();
        n.ie();
        return n;
    };
    TreeNodeEnableIndex.prototype.te = function() {
        var n = e.prototype.te.call(this);
        this.ie();
        n.ie();
        return n;
    };
    TreeNodeEnableIndex.prototype.ie = function() {
        this.cr = 1;
        if (this.er) {
            this.cr += this.er.cr;
        }
        if (this.tr) {
            this.cr += this.tr.cr;
        }
    };
    return TreeNodeEnableIndex;
}(TreeNode);

export { TreeNodeEnableIndex };
//# sourceMappingURL=TreeNode.js.map
