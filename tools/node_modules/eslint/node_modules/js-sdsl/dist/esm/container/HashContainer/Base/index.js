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
import { Base } from "../../ContainerBase/index";
var HashContainer = /** @class */ (function (_super) {
    __extends(HashContainer, _super);
    function HashContainer(initBucketNum, hashFunc) {
        if (initBucketNum === void 0) { initBucketNum = 16; }
        if (hashFunc === void 0) { hashFunc = function (x) {
            var str;
            if (typeof x !== 'string') {
                str = JSON.stringify(x);
            }
            else
                str = x;
            var hashCode = 0;
            var strLength = str.length;
            for (var i = 0; i < strLength; i++) {
                var ch = str.charCodeAt(i);
                hashCode = ((hashCode << 5) - hashCode) + ch;
                hashCode |= 0;
            }
            return hashCode >>> 0;
        }; }
        var _this = _super.call(this) || this;
        if (initBucketNum < 16 || (initBucketNum & (initBucketNum - 1)) !== 0) {
            throw new RangeError('InitBucketNum range error');
        }
        _this.bucketNum = _this.initBucketNum = initBucketNum;
        _this.hashFunc = hashFunc;
        return _this;
    }
    HashContainer.prototype.clear = function () {
        this.length = 0;
        this.bucketNum = this.initBucketNum;
        this.hashTable = [];
    };
    HashContainer.sigma = 0.75;
    HashContainer.treeifyThreshold = 8;
    HashContainer.untreeifyThreshold = 6;
    HashContainer.minTreeifySize = 64;
    HashContainer.maxBucketNum = (1 << 30);
    return HashContainer;
}(Base));
export default HashContainer;
