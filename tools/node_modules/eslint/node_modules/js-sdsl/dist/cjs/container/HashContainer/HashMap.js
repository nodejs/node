"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const index_1 = __importDefault(require("./Base/index"));
const Vector_1 = __importDefault(require("../SequentialContainer/Vector"));
const OrderedMap_1 = __importDefault(require("../TreeContainer/OrderedMap"));
class HashMap extends index_1.default {
    constructor(container = [], initBucketNum, hashFunc) {
        super(initBucketNum, hashFunc);
        this.hashTable = [];
        container.forEach(element => this.setElement(element[0], element[1]));
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
                newHashTable[this.hashFunc(element[0]) & (this.bucketNum - 1)] = new Vector_1.default([element], false);
                continue;
            }
            const lowList = [];
            const highList = [];
            container.forEach(element => {
                const hashCode = this.hashFunc(element[0]);
                if ((hashCode & originalBucketNum) === 0) {
                    lowList.push(element);
                }
                else
                    highList.push(element);
            });
            if (container instanceof OrderedMap_1.default) {
                if (lowList.length > index_1.default.untreeifyThreshold) {
                    newHashTable[index] = new OrderedMap_1.default(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector_1.default(lowList, false);
                }
                if (highList.length > index_1.default.untreeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedMap_1.default(highList);
                }
                else if (highList.length) {
                    newHashTable[index + originalBucketNum] = new Vector_1.default(highList, false);
                }
            }
            else {
                if (lowList.length >= index_1.default.treeifyThreshold) {
                    newHashTable[index] = new OrderedMap_1.default(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector_1.default(lowList, false);
                }
                if (highList.length >= index_1.default.treeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedMap_1.default(highList);
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
     * @description Insert a new key-value pair to hash map or set value by key.
     * @param key The key you want to insert.
     * @param value The value you want to insert.
     * @example HashMap.setElement(1, 2); // insert a key-value pair [1, 2]
     */
    setElement(key, value) {
        const index = this.hashFunc(key) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container) {
            this.length += 1;
            this.hashTable[index] = new Vector_1.default([[key, value]], false);
        }
        else {
            const preSize = container.size();
            if (container instanceof Vector_1.default) {
                for (const pair of container) {
                    if (pair[0] === key) {
                        pair[1] = value;
                        return;
                    }
                }
                container.pushBack([key, value]);
                if (preSize + 1 >= HashMap.treeifyThreshold) {
                    if (this.bucketNum <= HashMap.minTreeifySize) {
                        this.length += 1;
                        this.reAllocate();
                        return;
                    }
                    this.hashTable[index] = new OrderedMap_1.default(this.hashTable[index]);
                }
                this.length += 1;
            }
            else {
                container.setElement(key, value);
                const curSize = container.size();
                this.length += curSize - preSize;
            }
        }
        if (this.length > this.bucketNum * HashMap.sigma) {
            this.reAllocate();
        }
    }
    /**
     * @description Get the value of the element which has the specified key.
     * @param key The key you want to get.
     */
    getElementByKey(key) {
        const index = this.hashFunc(key) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container)
            return undefined;
        if (container instanceof OrderedMap_1.default) {
            return container.getElementByKey(key);
        }
        else {
            for (const pair of container) {
                if (pair[0] === key)
                    return pair[1];
            }
            return undefined;
        }
    }
    eraseElementByKey(key) {
        const index = this.hashFunc(key) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container)
            return;
        if (container instanceof Vector_1.default) {
            let pos = 0;
            for (const pair of container) {
                if (pair[0] === key) {
                    container.eraseElementByPos(pos);
                    this.length -= 1;
                    return;
                }
                pos += 1;
            }
        }
        else {
            const preSize = container.size();
            container.eraseElementByKey(key);
            const curSize = container.size();
            this.length += curSize - preSize;
            if (curSize <= index_1.default.untreeifyThreshold) {
                this.hashTable[index] = new Vector_1.default(container);
            }
        }
    }
    find(key) {
        const index = this.hashFunc(key) & (this.bucketNum - 1);
        const container = this.hashTable[index];
        if (!container)
            return false;
        if (container instanceof OrderedMap_1.default) {
            return !container.find(key)
                .equals(container.end());
        }
        for (const pair of container) {
            if (pair[0] === key)
                return true;
        }
        return false;
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
exports.default = HashMap;
