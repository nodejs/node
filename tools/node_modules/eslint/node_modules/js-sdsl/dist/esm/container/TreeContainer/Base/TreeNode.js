var TreeNode = /** @class */ (function () {
    function TreeNode(key, value) {
        this.color = true;
        this.key = undefined;
        this.value = undefined;
        this.left = undefined;
        this.right = undefined;
        this.parent = undefined;
        this.key = key;
        this.value = value;
    }
    /**
     * @description Get the pre node.
     * @return TreeNode about the pre node.
     */
    TreeNode.prototype.pre = function () {
        var preNode = this;
        if (preNode.color === TreeNode.RED &&
            preNode.parent.parent === preNode) {
            preNode = preNode.right;
        }
        else if (preNode.left) {
            preNode = preNode.left;
            while (preNode.right) {
                preNode = preNode.right;
            }
        }
        else {
            var pre = preNode.parent;
            while (pre.left === preNode) {
                preNode = pre;
                pre = preNode.parent;
            }
            preNode = pre;
        }
        return preNode;
    };
    /**
     * @description Get the next node.
     * @return TreeNode about the next node.
     */
    TreeNode.prototype.next = function () {
        var nextNode = this;
        if (nextNode.right) {
            nextNode = nextNode.right;
            while (nextNode.left) {
                nextNode = nextNode.left;
            }
        }
        else {
            var pre = nextNode.parent;
            while (pre.right === nextNode) {
                nextNode = pre;
                pre = nextNode.parent;
            }
            if (nextNode.right !== pre) {
                nextNode = pre;
            }
        }
        return nextNode;
    };
    /**
     * @description Rotate left.
     * @return TreeNode about moved to original position after rotation.
     */
    TreeNode.prototype.rotateLeft = function () {
        var PP = this.parent;
        var V = this.right;
        var R = V.left;
        if (PP.parent === this)
            PP.parent = V;
        else if (PP.left === this)
            PP.left = V;
        else
            PP.right = V;
        V.parent = PP;
        V.left = this;
        this.parent = V;
        this.right = R;
        if (R)
            R.parent = this;
        return V;
    };
    /**
     * @description Rotate left.
     * @return TreeNode about moved to original position after rotation.
     */
    TreeNode.prototype.rotateRight = function () {
        var PP = this.parent;
        var F = this.left;
        var K = F.right;
        if (PP.parent === this)
            PP.parent = F;
        else if (PP.left === this)
            PP.left = F;
        else
            PP.right = F;
        F.parent = PP;
        F.right = this;
        this.parent = F;
        this.left = K;
        if (K)
            K.parent = this;
        return F;
    };
    /**
     * @description Remove this.
     */
    TreeNode.prototype.remove = function () {
        var parent = this.parent;
        if (this === parent.left) {
            parent.left = undefined;
        }
        else
            parent.right = undefined;
    };
    TreeNode.RED = true;
    TreeNode.BLACK = false;
    return TreeNode;
}());
export default TreeNode;
