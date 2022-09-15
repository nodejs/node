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
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
var __values = (this && this.__values) || function(o) {
    var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
    if (m) return m.call(o);
    if (o && typeof o.length === "number") return {
        next: function () {
            if (o && i >= o.length) o = void 0;
            return { value: o && o[i++], done: !o };
        }
    };
    throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
};
import HashContainer from './Base/index';
import Vector from '../SequentialContainer/Vector';
import OrderedSet from '../TreeContainer/OrderedSet';
var HashSet = /** @class */ (function (_super) {
    __extends(HashSet, _super);
    function HashSet(container, initBucketNum, hashFunc) {
        if (container === void 0) { container = []; }
        var _this = _super.call(this, initBucketNum, hashFunc) || this;
        _this.hashTable = [];
        container.forEach(function (element) { return _this.insert(element); });
        return _this;
    }
    HashSet.prototype.reAllocate = function () {
        var _this = this;
        if (this.bucketNum >= HashContainer.maxBucketNum)
            return;
        var newHashTable = [];
        var originalBucketNum = this.bucketNum;
        this.bucketNum <<= 1;
        var keys = Object.keys(this.hashTable);
        var keyNums = keys.length;
        var _loop_1 = function (i) {
            var index = parseInt(keys[i]);
            var container = this_1.hashTable[index];
            var size = container.size();
            if (size === 0)
                return "continue";
            if (size === 1) {
                var element = container.front();
                newHashTable[this_1.hashFunc(element) & (this_1.bucketNum - 1)] = new Vector([element], false);
                return "continue";
            }
            var lowList = [];
            var highList = [];
            container.forEach(function (element) {
                var hashCode = _this.hashFunc(element);
                if ((hashCode & originalBucketNum) === 0) {
                    lowList.push(element);
                }
                else
                    highList.push(element);
            });
            if (container instanceof OrderedSet) {
                if (lowList.length > HashContainer.untreeifyThreshold) {
                    newHashTable[index] = new OrderedSet(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector(lowList, false);
                }
                if (highList.length > HashContainer.untreeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedSet(highList);
                }
                else if (highList.length) {
                    newHashTable[index + originalBucketNum] = new Vector(highList, false);
                }
            }
            else {
                if (lowList.length >= HashContainer.treeifyThreshold) {
                    newHashTable[index] = new OrderedSet(lowList);
                }
                else if (lowList.length) {
                    newHashTable[index] = new Vector(lowList, false);
                }
                if (highList.length >= HashContainer.treeifyThreshold) {
                    newHashTable[index + originalBucketNum] = new OrderedSet(highList);
                }
                else if (highList.length) {
                    newHashTable[index + originalBucketNum] = new Vector(highList, false);
                }
            }
        };
        var this_1 = this;
        for (var i = 0; i < keyNums; ++i) {
            _loop_1(i);
        }
        this.hashTable = newHashTable;
    };
    HashSet.prototype.forEach = function (callback) {
        var containers = Object.values(this.hashTable);
        var containersNum = containers.length;
        var index = 0;
        for (var i = 0; i < containersNum; ++i) {
            containers[i].forEach(function (element) { return callback(element, index++); });
        }
    };
    /**
     * @description Insert element to hash set.
     * @param element The element you want to insert.
     */
    HashSet.prototype.insert = function (element) {
        var index = this.hashFunc(element) & (this.bucketNum - 1);
        var container = this.hashTable[index];
        if (!container) {
            this.hashTable[index] = new Vector([element], false);
            this.length += 1;
        }
        else {
            var preSize = container.size();
            if (container instanceof Vector) {
                if (!container.find(element)
                    .equals(container.end()))
                    return;
                container.pushBack(element);
                if (preSize + 1 >= HashContainer.treeifyThreshold) {
                    if (this.bucketNum <= HashContainer.minTreeifySize) {
                        this.length += 1;
                        this.reAllocate();
                        return;
                    }
                    this.hashTable[index] = new OrderedSet(container);
                }
                this.length += 1;
            }
            else {
                container.insert(element);
                var curSize = container.size();
                this.length += curSize - preSize;
            }
        }
        if (this.length > this.bucketNum * HashContainer.sigma) {
            this.reAllocate();
        }
    };
    HashSet.prototype.eraseElementByKey = function (key) {
        var index = this.hashFunc(key) & (this.bucketNum - 1);
        var container = this.hashTable[index];
        if (!container)
            return;
        var preSize = container.size();
        if (preSize === 0)
            return;
        if (container instanceof Vector) {
            container.eraseElementByValue(key);
            var curSize = container.size();
            this.length += curSize - preSize;
        }
        else {
            container.eraseElementByKey(key);
            var curSize = container.size();
            this.length += curSize - preSize;
            if (curSize <= HashContainer.untreeifyThreshold) {
                this.hashTable[index] = new Vector(container);
            }
        }
    };
    HashSet.prototype.find = function (element) {
        var index = this.hashFunc(element) & (this.bucketNum - 1);
        var container = this.hashTable[index];
        if (!container)
            return false;
        return !container.find(element)
            .equals(container.end());
    };
    HashSet.prototype[Symbol.iterator] = function () {
        return function () {
            var containers, containersNum, i, container, container_1, container_1_1, element, e_1_1;
            var e_1, _a;
            return __generator(this, function (_b) {
                switch (_b.label) {
                    case 0:
                        containers = Object.values(this.hashTable);
                        containersNum = containers.length;
                        i = 0;
                        _b.label = 1;
                    case 1:
                        if (!(i < containersNum)) return [3 /*break*/, 10];
                        container = containers[i];
                        _b.label = 2;
                    case 2:
                        _b.trys.push([2, 7, 8, 9]);
                        container_1 = (e_1 = void 0, __values(container)), container_1_1 = container_1.next();
                        _b.label = 3;
                    case 3:
                        if (!!container_1_1.done) return [3 /*break*/, 6];
                        element = container_1_1.value;
                        return [4 /*yield*/, element];
                    case 4:
                        _b.sent();
                        _b.label = 5;
                    case 5:
                        container_1_1 = container_1.next();
                        return [3 /*break*/, 3];
                    case 6: return [3 /*break*/, 9];
                    case 7:
                        e_1_1 = _b.sent();
                        e_1 = { error: e_1_1 };
                        return [3 /*break*/, 9];
                    case 8:
                        try {
                            if (container_1_1 && !container_1_1.done && (_a = container_1.return)) _a.call(container_1);
                        }
                        finally { if (e_1) throw e_1.error; }
                        return [7 /*endfinally*/];
                    case 9:
                        ++i;
                        return [3 /*break*/, 1];
                    case 10: return [2 /*return*/];
                }
            });
        }.bind(this)();
    };
    return HashSet;
}(HashContainer));
export default HashSet;
