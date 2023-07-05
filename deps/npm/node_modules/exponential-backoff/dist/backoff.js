"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
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
Object.defineProperty(exports, "__esModule", { value: true });
var options_1 = require("./options");
var delay_factory_1 = require("./delay/delay.factory");
function backOff(request, options) {
    if (options === void 0) { options = {}; }
    return __awaiter(this, void 0, void 0, function () {
        var sanitizedOptions, backOff;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    sanitizedOptions = options_1.getSanitizedOptions(options);
                    backOff = new BackOff(request, sanitizedOptions);
                    return [4 /*yield*/, backOff.execute()];
                case 1: return [2 /*return*/, _a.sent()];
            }
        });
    });
}
exports.backOff = backOff;
var BackOff = /** @class */ (function () {
    function BackOff(request, options) {
        this.request = request;
        this.options = options;
        this.attemptNumber = 0;
    }
    BackOff.prototype.execute = function () {
        return __awaiter(this, void 0, void 0, function () {
            var e_1, shouldRetry;
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        if (!!this.attemptLimitReached) return [3 /*break*/, 7];
                        _a.label = 1;
                    case 1:
                        _a.trys.push([1, 4, , 6]);
                        return [4 /*yield*/, this.applyDelay()];
                    case 2:
                        _a.sent();
                        return [4 /*yield*/, this.request()];
                    case 3: return [2 /*return*/, _a.sent()];
                    case 4:
                        e_1 = _a.sent();
                        this.attemptNumber++;
                        return [4 /*yield*/, this.options.retry(e_1, this.attemptNumber)];
                    case 5:
                        shouldRetry = _a.sent();
                        if (!shouldRetry || this.attemptLimitReached) {
                            throw e_1;
                        }
                        return [3 /*break*/, 6];
                    case 6: return [3 /*break*/, 0];
                    case 7: throw new Error("Something went wrong.");
                }
            });
        });
    };
    Object.defineProperty(BackOff.prototype, "attemptLimitReached", {
        get: function () {
            return this.attemptNumber >= this.options.numOfAttempts;
        },
        enumerable: true,
        configurable: true
    });
    BackOff.prototype.applyDelay = function () {
        return __awaiter(this, void 0, void 0, function () {
            var delay;
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        delay = delay_factory_1.DelayFactory(this.options, this.attemptNumber);
                        return [4 /*yield*/, delay.apply()];
                    case 1:
                        _a.sent();
                        return [2 /*return*/];
                }
            });
        });
    };
    return BackOff;
}());
//# sourceMappingURL=backoff.js.map