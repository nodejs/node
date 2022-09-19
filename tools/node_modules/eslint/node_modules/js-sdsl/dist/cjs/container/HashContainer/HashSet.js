"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const index_1 = __importDefault(require("./Base/index"));
const Vector_1 = __importDefault(require("../SequentialContainer/Vector"));
const OrderedSet_1 = __importDefault(require("../TreeContainer/OrderedSet"));
class HashSet extends index_1.default {
    constructor(container = [], initBucketNum, hashFunc) {
        super(initBucketNum, hashFunc);
        this.hashTable = [];
        container.forEach(element => this.insert(element));
    }
    reAllocate() {
        if (this.bucketNum >= index_1.default.maxBucketNum)
            return;
        const newHashTable = [];
        const originalBucketNum = this.bucketNum;
        this.bucketNum <<= 1;
        const keys = Object.keys(this.hashTable);
        const keyNums = keys.length;
        for (let i = 0; i < keyNums; ++i) {
            const index = parseInt(keys[i]);
            const container = this.hashTable[index];
            const size = container.size();
            if (size === 0)
                continue;
            if (size === 1) {
                const element = container.front();
                newHashTable[this.hashFunc(element) & (this.bucketNum - 1)] = new Vector_1.default([element], false);
                continue;
            }
            const lowList = [];
            const highList = [];
            container.forEach(element => {
                const hashCode = this.hashFunc(element);
                if ((hashCode & originalBucketNum) === 0) {
                    lowList.push(element);
                }
                else
                    highList.push(element);
            });
            if (container instanceof OrderedSet_1.default) {
                if (lowList.length > index_1.default.untreeifyThreshold) {
                    newHashTable[index] = new OrderedSet_1.default(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector_1.default(lowList, false);
                }
                if (highList.length > index_1.default.untreeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedSet_1.default(highList);
                }
                else if (highList.length) {
                    newHashTable[index + originalBucketNum] = new Vector_1.default(highList, false);
                }
            }
            else {
                if (lowList.length >= index_1.default.treeifyThreshold) {
                    newHashTable[index] = new OrderedSet_1.default(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector_1.default(lowList, false);
                }
                if (highList.length >= index_1.default.treeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedSet_1.default(highList);
                }
                else if (highList.length) {
                    newHashTable[index + originalBucketNum] = new Vector_1.default(highList, false);
                }
            }
        }
        this.hashTable = newHashTable;
    }
    forEach(callback) {
        const containers = Object.values(this.hashTable);
        const containersNum = containers.length;
        let index = 0;
        for (let i = 0; i < containersNum; ++i) {
            containers[i].forEach(element => callback(element, index++));
        }
    }
    /**
     * @description Insert element to hash set.
     * @param element The element you want to insert.
     */
    insert(element) {
        const index = this.hashFunc(element) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container) {
            this.hashTable[index] = new Vector_1.default([element], false);
            this.length += 1;
        }
        else {
            const preSize = container.size();
            if (container instanceof Vector_1.default) {
                if (!container.find(element)
                    .equals(container.end()))
                    return;
                container.pushBack(element);
                if (preSize + 1 >= index_1.default.treeifyThreshold) {
                    if (this.bucketNum <= index_1.default.minTreeifySize) {
                        this.length += 1;
                        this.reAllocate();
                        return;
                    }
                    this.hashTable[index] = new OrderedSet_1.default(container);
                }
                this.length += 1;
            }
            else {
                container.insert(element);
                const curSize = container.size();
                this.length += curSize - preSize;
            }
        }
        if (this.length > this.bucketNum * index_1.default.sigma) {
            this.reAllocate();
        }
    }
    eraseElementByKey(key) {
        const index = this.hashFunc(key) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container)
            return;
        const preSize = container.size();
        if (preSize === 0)
            return;
        if (container instanceof Vector_1.default) {
            container.eraseElementByValue(key);
            const curSize = container.size();
            this.length += curSize - preSize;
        }
        else {
            container.eraseElementByKey(key);
            const curSize = container.size();
            this.length += curSize - preSize;
            if (curSize <= index_1.default.untreeifyThreshold) {
                this.hashTable[index] = new Vector_1.default(container);
            }
        }
    }
    find(element) {
        const index = this.hashFunc(element) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container)
            return false;
        return !container.find(element)
            .equals(container.end());
    }
    [Symbol.iterator]() {
        return function* () {
            const containers = Object.values(this.hashTable);
            const containersNum = containers.length;
            for (let i = 0; i < containersNum; ++i) {
                const container = containers[i];
                for (const element of container) {
                    yield element;
                }
            }
        }.bind(this)();
    }
}
exports.default = HashSet;
