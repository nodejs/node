var __extends = this && this.t || function() {
    var extendStatics = function(e, n) {
        extendStatics = Object.setPrototypeOf || {
            __proto__: []
        } instanceof Array && function(e, n) {
            e.__proto__ = n;
        } || function(e, n) {
            for (var i in n) if (Object.prototype.hasOwnProperty.call(n, i)) e[i] = n[i];
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
        this.W = undefined;
        this.L = undefined;
        this.Y = undefined;
        this.Z = undefined;
        this.tt = undefined;
        this.W = e;
        this.L = n;
    }
    TreeNode.prototype.pre = function() {
        var e = this;
        if (e.ee === 1 && e.tt.tt === e) {
            e = e.Z;
        } else if (e.Y) {
            e = e.Y;
            while (e.Z) {
                e = e.Z;
            }
        } else {
            var n = e.tt;
            while (n.Y === e) {
                e = n;
                n = e.tt;
            }
            e = n;
        }
        return e;
    };
    TreeNode.prototype.next = function() {
        var e = this;
        if (e.Z) {
            e = e.Z;
            while (e.Y) {
                e = e.Y;
            }
            return e;
        } else {
            var n = e.tt;
            while (n.Z === e) {
                e = n;
                n = e.tt;
            }
            if (e.Z !== n) {
                return n;
            } else return e;
        }
    };
    TreeNode.prototype.rotateLeft = function() {
        var e = this.tt;
        var n = this.Z;
        var i = n.Y;
        if (e.tt === this) e.tt = n; else if (e.Y === this) e.Y = n; else e.Z = n;
        n.tt = e;
        n.Y = this;
        this.tt = n;
        this.Z = i;
        if (i) i.tt = this;
        return n;
    };
    TreeNode.prototype.rotateRight = function() {
        var e = this.tt;
        var n = this.Y;
        var i = n.Z;
        if (e.tt === this) e.tt = n; else if (e.Y === this) e.Y = n; else e.Z = n;
        n.tt = e;
        n.Z = this;
        this.tt = n;
        this.Y = i;
        if (i) i.tt = this;
        return n;
    };
    return TreeNode;
}();

export { TreeNode };

var TreeNodeEnableIndex = function(e) {
    __extends(TreeNodeEnableIndex, e);
    function TreeNodeEnableIndex() {
        var n = e !== null && e.apply(this, arguments) || this;
        n.Y = undefined;
        n.Z = undefined;
        n.tt = undefined;
        n.rt = 1;
        return n;
    }
    TreeNodeEnableIndex.prototype.rotateLeft = function() {
        var n = e.prototype.rotateLeft.call(this);
        this.recount();
        n.recount();
        return n;
    };
    TreeNodeEnableIndex.prototype.rotateRight = function() {
        var n = e.prototype.rotateRight.call(this);
        this.recount();
        n.recount();
        return n;
    };
    TreeNodeEnableIndex.prototype.recount = function() {
        this.rt = 1;
        if (this.Y) this.rt += this.Y.rt;
        if (this.Z) this.rt += this.Z.rt;
    };
    return TreeNodeEnableIndex;
}(TreeNode);

export { TreeNodeEnableIndex };