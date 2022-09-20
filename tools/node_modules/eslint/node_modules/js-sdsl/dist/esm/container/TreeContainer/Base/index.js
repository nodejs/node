var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var __read = (this && this.__read) || function (o, n) {
    var m = typeof Symbol === "function" && o[Symbol.iterator];
    if (!m) return o;
    var i = m.call(o), r, ar = [], e;
    try {
        while ((n === void 0 || n-- > 0) && !(r = i.next()).done) ar.push(r.value);
    }
    catch (error) { e = { error: error }; }
    finally {
        try {
            if (r && !r.done && (m = i["return"])) m.call(i);
        }
        finally { if (e) throw e.error; }
    }
    return ar;
};
import TreeNode from './TreeNode';
import { Container } from "../../ContainerBase/index";
import { checkWithinAccessParams } from "../../../utils/checkParams";
var TreeContainer = /** @class */ (function (_super) {
    __extends(TreeContainer, _super);
    function TreeContainer(cmp) {
        if (cmp === void 0) { cmp = function (x, y) {
            if (x < y)
                return -1;
            if (x > y)
                return 1;
            return 0;
        }; }
        var _this = _super.call(this) || this;
        _this.root = undefined;
        _this.header = new TreeNode();
        /**
         * @description InOrder traversal the tree.
         * @protected
         */
        _this.inOrderTraversal = function (curNode, callback) {
            if (curNode === undefined)
                return false;
            var ifReturn = _this.inOrderTraversal(curNode.left, callback);
            if (ifReturn)
                return true;
            if (callback(curNode))
                return true;
            return _this.inOrderTraversal(curNode.right, callback);
        };
        _this.cmp = cmp;
        return _this;
    }
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is greater than or equals to the given key.
     * @protected
     */
    TreeContainer.prototype._lowerBound = function (curNode, key) {
        var resNode;
        while (curNode) {
            var cmpResult = this.cmp(curNode.key, key);
            if (cmpResult < 0) {
                curNode = curNode.right;
            }
            else if (cmpResult > 0) {
                resNode = curNode;
                curNode = curNode.left;
            }
            else
                return curNode;
        }
        return resNode === undefined ? this.header : resNode;
    };
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is greater than the given key.
     * @protected
     */
    TreeContainer.prototype._upperBound = function (curNode, key) {
        var resNode;
        while (curNode) {
            var cmpResult = this.cmp(curNode.key, key);
            if (cmpResult <= 0) {
                curNode = curNode.right;
            }
            else if (cmpResult > 0) {
                resNode = curNode;
                curNode = curNode.left;
            }
        }
        return resNode === undefined ? this.header : resNode;
    };
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is less than or equals to the given key.
     * @protected
     */
    TreeContainer.prototype._reverseLowerBound = function (curNode, key) {
        var resNode;
        while (curNode) {
            var cmpResult = this.cmp(curNode.key, key);
            if (cmpResult < 0) {
                resNode = curNode;
                curNode = curNode.right;
            }
            else if (cmpResult > 0) {
                curNode = curNode.left;
            }
            else
                return curNode;
        }
        return resNode === undefined ? this.header : resNode;
    };
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is less than the given key.
     * @protected
     */
    TreeContainer.prototype._reverseUpperBound = function (curNode, key) {
        var resNode;
        while (curNode) {
            var cmpResult = this.cmp(curNode.key, key);
            if (cmpResult < 0) {
                resNode = curNode;
                curNode = curNode.right;
            }
            else if (cmpResult >= 0) {
                curNode = curNode.left;
            }
        }
        return resNode === undefined ? this.header : resNode;
    };
    /**
     * @description Make self balance after erase a node.
     * @param curNode The node want to remove.
     * @protected
     */
    TreeContainer.prototype.eraseNodeSelfBalance = function (curNode) {
        while (true) {
            var parentNode = curNode.parent;
            if (parentNode === this.header)
                return;
            if (curNode.color === TreeNode.RED) {
                curNode.color = TreeNode.BLACK;
                return;
            }
            if (curNode === parentNode.left) {
                var brother = parentNode.right;
                if (brother.color === TreeNode.RED) {
                    brother.color = TreeNode.BLACK;
                    parentNode.color = TreeNode.RED;
                    if (parentNode === this.root) {
                        this.root = parentNode.rotateLeft();
                    }
                    else
                        parentNode.rotateLeft();
                }
                else if (brother.color === TreeNode.BLACK) {
                    if (brother.right && brother.right.color === TreeNode.RED) {
                        brother.color = parentNode.color;
                        parentNode.color = TreeNode.BLACK;
                        brother.right.color = TreeNode.BLACK;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateLeft();
                        }
                        else
                            parentNode.rotateLeft();
                        return;
                    }
                    else if (brother.left && brother.left.color === TreeNode.RED) {
                        brother.color = TreeNode.RED;
                        brother.left.color = TreeNode.BLACK;
                        brother.rotateRight();
                    }
                    else {
                        brother.color = TreeNode.RED;
                        curNode = parentNode;
                    }
                }
            }
            else {
                var brother = parentNode.left;
                if (brother.color === TreeNode.RED) {
                    brother.color = TreeNode.BLACK;
                    parentNode.color = TreeNode.RED;
                    if (parentNode === this.root) {
                        this.root = parentNode.rotateRight();
                    }
                    else
                        parentNode.rotateRight();
                }
                else {
                    if (brother.left && brother.left.color === TreeNode.RED) {
                        brother.color = parentNode.color;
                        parentNode.color = TreeNode.BLACK;
                        brother.left.color = TreeNode.BLACK;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateRight();
                        }
                        else
                            parentNode.rotateRight();
                        return;
                    }
                    else if (brother.right && brother.right.color === TreeNode.RED) {
                        brother.color = TreeNode.RED;
                        brother.right.color = TreeNode.BLACK;
                        brother.rotateLeft();
                    }
                    else {
                        brother.color = TreeNode.RED;
                        curNode = parentNode;
                    }
                }
            }
        }
    };
    /**
     * @description Remove a node.
     * @param curNode The node you want to remove.
     * @protected
     */
    TreeContainer.prototype.eraseNode = function (curNode) {
        var _a, _b;
        if (this.length === 1) {
            this.clear();
            return;
        }
        var swapNode = curNode;
        while (swapNode.left || swapNode.right) {
            if (swapNode.right) {
                swapNode = swapNode.right;
                while (swapNode.left)
                    swapNode = swapNode.left;
            }
            else if (swapNode.left) {
                swapNode = swapNode.left;
            }
            _a = __read([swapNode.key, curNode.key], 2), curNode.key = _a[0], swapNode.key = _a[1];
            _b = __read([swapNode.value, curNode.value], 2), curNode.value = _b[0], swapNode.value = _b[1];
            curNode = swapNode;
        }
        if (this.header.left === swapNode) {
            this.header.left = swapNode.parent;
        }
        else if (this.header.right === swapNode) {
            this.header.right = swapNode.parent;
        }
        this.eraseNodeSelfBalance(swapNode);
        swapNode.remove();
        this.length -= 1;
        this.root.color = TreeNode.BLACK;
    };
    /**
     * @description Make self balance after insert a node.
     * @param curNode The node want to insert.
     * @protected
     */
    TreeContainer.prototype.insertNodeSelfBalance = function (curNode) {
        while (true) {
            var parentNode = curNode.parent;
            if (parentNode.color === TreeNode.BLACK)
                return;
            var grandParent = parentNode.parent;
            if (parentNode === grandParent.left) {
                var uncle = grandParent.right;
                if (uncle && uncle.color === TreeNode.RED) {
                    uncle.color = parentNode.color = TreeNode.BLACK;
                    if (grandParent === this.root)
                        return;
                    grandParent.color = TreeNode.RED;
                    curNode = grandParent;
                    continue;
                }
                else if (curNode === parentNode.right) {
                    curNode.color = TreeNode.BLACK;
                    if (curNode.left)
                        curNode.left.parent = parentNode;
                    if (curNode.right)
                        curNode.right.parent = grandParent;
                    parentNode.right = curNode.left;
                    grandParent.left = curNode.right;
                    curNode.left = parentNode;
                    curNode.right = grandParent;
                    if (grandParent === this.root) {
                        this.root = curNode;
                        this.header.parent = curNode;
                    }
                    else {
                        var GP = grandParent.parent;
                        if (GP.left === grandParent) {
                            GP.left = curNode;
                        }
                        else
                            GP.right = curNode;
                    }
                    curNode.parent = grandParent.parent;
                    parentNode.parent = curNode;
                    grandParent.parent = curNode;
                }
                else {
                    parentNode.color = TreeNode.BLACK;
                    if (grandParent === this.root) {
                        this.root = grandParent.rotateRight();
                    }
                    else
                        grandParent.rotateRight();
                }
                grandParent.color = TreeNode.RED;
            }
            else {
                var uncle = grandParent.left;
                if (uncle && uncle.color === TreeNode.RED) {
                    uncle.color = parentNode.color = TreeNode.BLACK;
                    if (grandParent === this.root)
                        return;
                    grandParent.color = TreeNode.RED;
                    curNode = grandParent;
                    continue;
                }
                else if (curNode === parentNode.left) {
                    curNode.color = TreeNode.BLACK;
                    if (curNode.left)
                        curNode.left.parent = grandParent;
                    if (curNode.right)
                        curNode.right.parent = parentNode;
                    grandParent.right = curNode.left;
                    parentNode.left = curNode.right;
                    curNode.left = grandParent;
                    curNode.right = parentNode;
                    if (grandParent === this.root) {
                        this.root = curNode;
                        this.header.parent = curNode;
                    }
                    else {
                        var GP = grandParent.parent;
                        if (GP.left === grandParent) {
                            GP.left = curNode;
                        }
                        else
                            GP.right = curNode;
                    }
                    curNode.parent = grandParent.parent;
                    parentNode.parent = curNode;
                    grandParent.parent = curNode;
                }
                else {
                    parentNode.color = TreeNode.BLACK;
                    if (grandParent === this.root) {
                        this.root = grandParent.rotateLeft();
                    }
                    else
                        grandParent.rotateLeft();
                }
                grandParent.color = TreeNode.RED;
            }
            return;
        }
    };
    /**
     * @description Find node which key is equals to the given key.
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @protected
     */
    TreeContainer.prototype.findElementNode = function (curNode, key) {
        while (curNode) {
            var cmpResult = this.cmp(curNode.key, key);
            if (cmpResult < 0) {
                curNode = curNode.right;
            }
            else if (cmpResult > 0) {
                curNode = curNode.left;
            }
            else
                return curNode;
        }
        return curNode;
    };
    /**
     * @description Insert a key-value pair or set value by the given key.
     * @param key The key want to insert.
     * @param value The value want to set.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     * @protected
     */
    TreeContainer.prototype.set = function (key, value, hint) {
        if (this.root === undefined) {
            this.length += 1;
            this.root = new TreeNode(key, value);
            this.root.color = TreeNode.BLACK;
            this.root.parent = this.header;
            this.header.parent = this.root;
            this.header.left = this.root;
            this.header.right = this.root;
            return;
        }
        var curNode;
        var minNode = this.header.left;
        var compareToMin = this.cmp(minNode.key, key);
        if (compareToMin === 0) {
            minNode.value = value;
            return;
        }
        else if (compareToMin > 0) {
            minNode.left = new TreeNode(key, value);
            minNode.left.parent = minNode;
            curNode = minNode.left;
            this.header.left = curNode;
        }
        else {
            var maxNode = this.header.right;
            var compareToMax = this.cmp(maxNode.key, key);
            if (compareToMax === 0) {
                maxNode.value = value;
                return;
            }
            else if (compareToMax < 0) {
                maxNode.right = new TreeNode(key, value);
                maxNode.right.parent = maxNode;
                curNode = maxNode.right;
                this.header.right = curNode;
            }
            else {
                if (hint !== undefined) {
                    // @ts-ignore
                    var iterNode = hint.node;
                    if (iterNode !== this.header) {
                        var iterCmpRes = this.cmp(iterNode.key, key);
                        if (iterCmpRes === 0) {
                            iterNode.value = value;
                            return;
                        }
                        else if (iterCmpRes > 0) {
                            var preNode = iterNode.pre();
                            var preCmpRes = this.cmp(preNode.key, key);
                            if (preCmpRes === 0) {
                                preNode.value = value;
                                return;
                            }
                            else if (preCmpRes < 0) {
                                curNode = new TreeNode(key, value);
                                if (preNode.right === undefined) {
                                    preNode.right = curNode;
                                    curNode.parent = preNode;
                                }
                                else {
                                    iterNode.left = curNode;
                                    curNode.parent = iterNode;
                                }
                            }
                        }
                    }
                }
                if (curNode === undefined) {
                    curNode = this.root;
                    while (true) {
                        var cmpResult = this.cmp(curNode.key, key);
                        if (cmpResult > 0) {
                            if (curNode.left === undefined) {
                                curNode.left = new TreeNode(key, value);
                                curNode.left.parent = curNode;
                                curNode = curNode.left;
                                break;
                            }
                            curNode = curNode.left;
                        }
                        else if (cmpResult < 0) {
                            if (curNode.right === undefined) {
                                curNode.right = new TreeNode(key, value);
                                curNode.right.parent = curNode;
                                curNode = curNode.right;
                                break;
                            }
                            curNode = curNode.right;
                        }
                        else {
                            curNode.value = value;
                            return;
                        }
                    }
                }
            }
        }
        this.length += 1;
        this.insertNodeSelfBalance(curNode);
    };
    TreeContainer.prototype.clear = function () {
        this.length = 0;
        this.root = undefined;
        this.header.parent = undefined;
        this.header.left = this.header.right = undefined;
    };
    /**
     * @description Update node's key by iterator.
     * @param iter The iterator you want to change.
     * @param key The key you want to update.
     * @return Boolean about if the modification is successful.
     */
    TreeContainer.prototype.updateKeyByIterator = function (iter, key) {
        // @ts-ignore
        var node = iter.node;
        if (node === this.header) {
            throw new TypeError('Invalid iterator!');
        }
        if (this.length === 1) {
            node.key = key;
            return true;
        }
        if (node === this.header.left) {
            if (this.cmp(node.next().key, key) > 0) {
                node.key = key;
                return true;
            }
            return false;
        }
        if (node === this.header.right) {
            if (this.cmp(node.pre().key, key) < 0) {
                node.key = key;
                return true;
            }
            return false;
        }
        var preKey = node.pre().key;
        if (this.cmp(preKey, key) >= 0)
            return false;
        var nextKey = node.next().key;
        if (this.cmp(nextKey, key) <= 0)
            return false;
        node.key = key;
        return true;
    };
    TreeContainer.prototype.eraseElementByPos = function (pos) {
        var _this = this;
        checkWithinAccessParams(pos, 0, this.length - 1);
        var index = 0;
        this.inOrderTraversal(this.root, function (curNode) {
            if (pos === index) {
                _this.eraseNode(curNode);
                return true;
            }
            index += 1;
            return false;
        });
    };
    /**
     * @description Remove the element of the specified key.
     * @param key The key you want to remove.
     */
    TreeContainer.prototype.eraseElementByKey = function (key) {
        if (!this.length)
            return;
        var curNode = this.findElementNode(this.root, key);
        if (curNode === undefined)
            return;
        this.eraseNode(curNode);
    };
    TreeContainer.prototype.eraseElementByIterator = function (iter) {
        // @ts-ignore
        var node = iter.node;
        if (node === this.header) {
            throw new RangeError('Invalid iterator');
        }
        if (node.right === undefined) {
            iter = iter.next();
        }
        this.eraseNode(node);
        return iter;
    };
    /**
     * @description Get the height of the tree.
     * @return Number about the height of the RB-tree.
     */
    TreeContainer.prototype.getHeight = function () {
        if (!this.length)
            return 0;
        var traversal = function (curNode) {
            if (!curNode)
                return 0;
            return Math.max(traversal(curNode.left), traversal(curNode.right)) + 1;
        };
        return traversal(this.root);
    };
    return TreeContainer;
}(Container));
export default TreeContainer;
