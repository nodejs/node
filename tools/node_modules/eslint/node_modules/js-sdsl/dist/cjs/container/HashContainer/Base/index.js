"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const index_1 = require("../../ContainerBase/index");
class HashContainer extends index_1.Base {
    constructor(initBucketNum = 16, hashFunc = (x) => {
        let str;
        if (typeof x !== 'string') {
            str = JSON.stringify(x);
        }
        else
            str = x;
        let hashCode = 0;
        const strLength = str.length;
        for (let i = 0; i < strLength; i++) {
            const ch = str.charCodeAt(i);
            hashCode = ((hashCode << 5) - hashCode) + ch;
            hashCode |= 0;
        }
        return hashCode >>> 0;
    }) {
        super();
        if (initBucketNum < 16 || (initBucketNum & (initBucketNum - 1)) !== 0) {
            throw new RangeError('InitBucketNum range error');
        }
        this.bucketNum = this.initBucketNum = initBucketNum;
        this.hashFunc = hashFunc;
    }
    clear() {
        this.length = 0;
        this.bucketNum = this.initBucketNum;
        this.hashTable = [];
    }
}
HashContainer.sigma = 0.75;
HashContainer.treeifyThreshold = 8;
HashContainer.untreeifyThreshold = 6;
HashContainer.minTreeifySize = 64;
HashContainer.maxBucketNum = (1 << 30);
exports.default = HashContainer;
