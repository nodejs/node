"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const TreeNode_1 = __importDefault(require("./TreeNode"));
const index_1 = require("../../ContainerBase/index");
const checkParams_1 = require("../../../utils/checkParams");
class TreeContainer extends index_1.Container {
    constructor(cmp = (x, y) => {
        if (x < y)
            return -1;
        if (x > y)
            return 1;
        return 0;
    }) {
        super();
        this.root = undefined;
        this.header = new TreeNode_1.default();
        /**
         * @description InOrder traversal the tree.
         * @protected
         */
        this.inOrderTraversal = (curNode, callback) => {
            if (curNode === undefined)
                return false;
            const ifReturn = this.inOrderTraversal(curNode.left, callback);
            if (ifReturn)
                return true;
            if (callback(curNode))
                return true;
            return this.inOrderTraversal(curNode.right, callback);
        };
        this.cmp = cmp;
    }
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is greater than or equals to the given key.
     * @protected
     */
    _lowerBound(curNode, key) {
        let resNode;
        while (curNode) {
            const cmpResult = this.cmp(curNode.key, key);
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
    }
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is greater than the given key.
     * @protected
     */
    _upperBound(curNode, key) {
        let resNode;
        while (curNode) {
            const cmpResult = this.cmp(curNode.key, key);
            if (cmpResult <= 0) {
                curNode = curNode.right;
            }
            else if (cmpResult > 0) {
                resNode = curNode;
                curNode = curNode.left;
            }
        }
        return resNode === undefined ? this.header : resNode;
    }
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is less than or equals to the given key.
     * @protected
     */
    _reverseLowerBound(curNode, key) {
        let resNode;
        while (curNode) {
            const cmpResult = this.cmp(curNode.key, key);
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
    }
    /**
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @return TreeNode which key is less than the given key.
     * @protected
     */
    _reverseUpperBound(curNode, key) {
        let resNode;
        while (curNode) {
            const cmpResult = this.cmp(curNode.key, key);
            if (cmpResult < 0) {
                resNode = curNode;
                curNode = curNode.right;
            }
            else if (cmpResult >= 0) {
                curNode = curNode.left;
            }
        }
        return resNode === undefined ? this.header : resNode;
    }
    /**
     * @description Make self balance after erase a node.
     * @param curNode The node want to remove.
     * @protected
     */
    eraseNodeSelfBalance(curNode) {
        while (true) {
            const parentNode = curNode.parent;
            if (parentNode === this.header)
                return;
            if (curNode.color === TreeNode_1.default.RED) {
                curNode.color = TreeNode_1.default.BLACK;
                return;
            }
            if (curNode === parentNode.left) {
                const brother = parentNode.right;
                if (brother.color === TreeNode_1.default.RED) {
                    brother.color = TreeNode_1.default.BLACK;
                    parentNode.color = TreeNode_1.default.RED;
                    if (parentNode === this.root) {
                        this.root = parentNode.rotateLeft();
                    }
                    else
                        parentNode.rotateLeft();
                }
                else if (brother.color === TreeNode_1.default.BLACK) {
                    if (brother.right && brother.right.color === TreeNode_1.default.RED) {
                        brother.color = parentNode.color;
                        parentNode.color = TreeNode_1.default.BLACK;
                        brother.right.color = TreeNode_1.default.BLACK;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateLeft();
                        }
                        else
                            parentNode.rotateLeft();
                        return;
                    }
                    else if (brother.left && brother.left.color === TreeNode_1.default.RED) {
                        brother.color = TreeNode_1.default.RED;
                        brother.left.color = TreeNode_1.default.BLACK;
                        brother.rotateRight();
                    }
                    else {
                        brother.color = TreeNode_1.default.RED;
                        curNode = parentNode;
                    }
                }
            }
            else {
                const brother = parentNode.left;
                if (brother.color === TreeNode_1.default.RED) {
                    brother.color = TreeNode_1.default.BLACK;
                    parentNode.color = TreeNode_1.default.RED;
                    if (parentNode === this.root) {
                        this.root = parentNode.rotateRight();
                    }
                    else
                        parentNode.rotateRight();
                }
                else {
                    if (brother.left && brother.left.color === TreeNode_1.default.RED) {
                        brother.color = parentNode.color;
                        parentNode.color = TreeNode_1.default.BLACK;
                        brother.left.color = TreeNode_1.default.BLACK;
                        if (parentNode === this.root) {
                            this.root = parentNode.rotateRight();
                        }
                        else
                            parentNode.rotateRight();
                        return;
                    }
                    else if (brother.right && brother.right.color === TreeNode_1.default.RED) {
                        brother.color = TreeNode_1.default.RED;
                        brother.right.color = TreeNode_1.default.BLACK;
                        brother.rotateLeft();
                    }
                    else {
                        brother.color = TreeNode_1.default.RED;
                        curNode = parentNode;
                    }
                }
            }
        }
    }
    /**
     * @description Remove a node.
     * @param curNode The node you want to remove.
     * @protected
     */
    eraseNode(curNode) {
        if (this.length === 1) {
            this.clear();
            return;
        }
        let swapNode = curNode;
        while (swapNode.left || swapNode.right) {
            if (swapNode.right) {
                swapNode = swapNode.right;
                while (swapNode.left)
                    swapNode = swapNode.left;
            }
            else if (swapNode.left) {
                swapNode = swapNode.left;
            }
            [curNode.key, swapNode.key] = [swapNode.key, curNode.key];
            [curNode.value, swapNode.value] = [swapNode.value, curNode.value];
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
        this.root.color = TreeNode_1.default.BLACK;
    }
    /**
     * @description Make self balance after insert a node.
     * @param curNode The node want to insert.
     * @protected
     */
    insertNodeSelfBalance(curNode) {
        while (true) {
            const parentNode = curNode.parent;
            if (parentNode.color === TreeNode_1.default.BLACK)
                return;
            const grandParent = parentNode.parent;
            if (parentNode === grandParent.left) {
                const uncle = grandParent.right;
                if (uncle && uncle.color === TreeNode_1.default.RED) {
                    uncle.color = parentNode.color = TreeNode_1.default.BLACK;
                    if (grandParent === this.root)
                        return;
                    grandParent.color = TreeNode_1.default.RED;
                    curNode = grandParent;
                    continue;
                }
                else if (curNode === parentNode.right) {
                    curNode.color = TreeNode_1.default.BLACK;
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
                        const GP = grandParent.parent;
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
                    parentNode.color = TreeNode_1.default.BLACK;
                    if (grandParent === this.root) {
                        this.root = grandParent.rotateRight();
                    }
                    else
                        grandParent.rotateRight();
                }
                grandParent.color = TreeNode_1.default.RED;
            }
            else {
                const uncle = grandParent.left;
                if (uncle && uncle.color === TreeNode_1.default.RED) {
                    uncle.color = parentNode.color = TreeNode_1.default.BLACK;
                    if (grandParent === this.root)
                        return;
                    grandParent.color = TreeNode_1.default.RED;
                    curNode = grandParent;
                    continue;
                }
                else if (curNode === parentNode.left) {
                    curNode.color = TreeNode_1.default.BLACK;
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
                        const GP = grandParent.parent;
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
                    parentNode.color = TreeNode_1.default.BLACK;
                    if (grandParent === this.root) {
                        this.root = grandParent.rotateLeft();
                    }
                    else
                        grandParent.rotateLeft();
                }
                grandParent.color = TreeNode_1.default.RED;
            }
            return;
        }
    }
    /**
     * @description Find node which key is equals to the given key.
     * @param curNode The starting node of the search.
     * @param key The key you want to search.
     * @protected
     */
    findElementNode(curNode, key) {
        while (curNode) {
            const cmpResult = this.cmp(curNode.key, key);
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
    }
    /**
     * @description Insert a key-value pair or set value by the given key.
     * @param key The key want to insert.
     * @param value The value want to set.
     * @param hint You can give an iterator hint to improve insertion efficiency.
     * @protected
     */
    set(key, value, hint) {
        if (this.root === undefined) {
            this.length += 1;
            this.root = new TreeNode_1.default(key, value);
            this.root.color = TreeNode_1.default.BLACK;
            this.root.parent = this.header;
            this.header.parent = this.root;
            this.header.left = this.root;
            this.header.right = this.root;
            return;
        }
        let curNode;
        const minNode = this.header.left;
        const compareToMin = this.cmp(minNode.key, key);
        if (compareToMin === 0) {
            minNode.value = value;
            return;
        }
        else if (compareToMin > 0) {
            minNode.left = new TreeNode_1.default(key, value);
            minNode.left.parent = minNode;
            curNode = minNode.left;
            this.header.left = curNode;
        }
        else {
            const maxNode = this.header.right;
            const compareToMax = this.cmp(maxNode.key, key);
            if (compareToMax === 0) {
                maxNode.value = value;
                return;
            }
            else if (compareToMax < 0) {
                maxNode.right = new TreeNode_1.default(key, value);
                maxNode.right.parent = maxNode;
                curNode = maxNode.right;
                this.header.right = curNode;
            }
            else {
                if (hint !== undefined) {
                    // @ts-ignore
                    const iterNode = hint.node;
                    if (iterNode !== this.header) {
                        const iterCmpRes = this.cmp(iterNode.key, key);
                        if (iterCmpRes === 0) {
                            iterNode.value = value;
                            return;
                        }
                        else if (iterCmpRes > 0) {
                            const preNode = iterNode.pre();
                            const preCmpRes = this.cmp(preNode.key, key);
                            if (preCmpRes === 0) {
                                preNode.value = value;
                                return;
                            }
                            else if (preCmpRes < 0) {
                                curNode = new TreeNode_1.default(key, value);
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
                        const cmpResult = this.cmp(curNode.key, key);
                        if (cmpResult > 0) {
                            if (curNode.left === undefined) {
                                curNode.left = new TreeNode_1.default(key, value);
                                curNode.left.parent = curNode;
                                curNode = curNode.left;
                                break;
                            }
                            curNode = curNode.left;
                        }
                        else if (cmpResult < 0) {
                            if (curNode.right === undefined) {
                                curNode.right = new TreeNode_1.default(key, value);
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
    }
    clear() {
        this.length = 0;
        this.root = undefined;
        this.header.parent = undefined;
        this.header.left = this.header.right = undefined;
    }
    /**
     * @description Update node's key by iterator.
     * @param iter The iterator you want to change.
     * @param key The key you want to update.
     * @return Boolean about if the modification is successful.
     */
    updateKeyByIterator(iter, key) {
        // @ts-ignore
        const node = iter.node;
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
        const preKey = node.pre().key;
        if (this.cmp(preKey, key) >= 0)
            return false;
        const nextKey = node.next().key;
        if (this.cmp(nextKey, key) <= 0)
            return false;
        node.key = key;
        return true;
    }
    eraseElementByPos(pos) {
        (0, checkParams_1.checkWithinAccessParams)(pos, 0, this.length - 1);
        let index = 0;
        this.inOrderTraversal(this.root, curNode => {
            if (pos === index) {
                this.eraseNode(curNode);
                return true;
            }
            index += 1;
            return false;
        });
    }
    /**
     * @description Remove the element of the specified key.
     * @param key The key you want to remove.
     */
    eraseElementByKey(key) {
        if (!this.length)
            return;
        const curNode = this.findElementNode(this.root, key);
        if (curNode === undefined)
            return;
        this.eraseNode(curNode);
    }
    eraseElementByIterator(iter) {
        // @ts-ignore
        const node = iter.node;
        if (node === this.header) {
            throw new RangeError('Invalid iterator');
        }
        if (node.right === undefined) {
            iter = iter.next();
        }
        this.eraseNode(node);
        return iter;
    }
    /**
     * @description Get the height of the tree.
     * @return Number about the height of the RB-tree.
     */
    getHeight() {
        if (!this.length)
            return 0;
        const traversal = (curNode) => {
            if (!curNode)
                return 0;
            return Math.max(traversal(curNode.left), traversal(curNode.right)) + 1;
        };
        return traversal(this.root);
    }
}
exports.default = TreeContainer;
