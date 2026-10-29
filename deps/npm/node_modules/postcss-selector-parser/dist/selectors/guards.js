"use strict";
var _a;
Object.defineProperty(exports, "__esModule", { value: true });
exports.isUniversal = exports.isTag = exports.isString = exports.isSelector = exports.isRoot = exports.isPseudo = exports.isNesting = exports.isIdentifier = exports.isComment = exports.isCombinator = exports.isClassName = exports.isAttribute = void 0;
exports.isNode = isNode;
exports.isPseudoElement = isPseudoElement;
exports.isPseudoClass = isPseudoClass;
exports.isContainer = isContainer;
exports.isNamespace = isNamespace;
var types_1 = require("./types");
var IS_TYPE = (_a = {},
    _a[types_1.ATTRIBUTE] = true,
    _a[types_1.CLASS] = true,
    _a[types_1.COMBINATOR] = true,
    _a[types_1.COMMENT] = true,
    _a[types_1.ID] = true,
    _a[types_1.NESTING] = true,
    _a[types_1.PSEUDO] = true,
    _a[types_1.ROOT] = true,
    _a[types_1.SELECTOR] = true,
    _a[types_1.STRING] = true,
    _a[types_1.TAG] = true,
    _a[types_1.UNIVERSAL] = true,
    _a);
function isNode(node) {
    return typeof node === "object" && IS_TYPE[node.type];
}
function isNodeType(type, node) {
    return isNode(node) && node.type === type;
}
exports.isAttribute = isNodeType.bind(null, types_1.ATTRIBUTE);
exports.isClassName = isNodeType.bind(null, types_1.CLASS);
exports.isCombinator = isNodeType.bind(null, types_1.COMBINATOR);
exports.isComment = isNodeType.bind(null, types_1.COMMENT);
exports.isIdentifier = isNodeType.bind(null, types_1.ID);
exports.isNesting = isNodeType.bind(null, types_1.NESTING);
exports.isPseudo = isNodeType.bind(null, types_1.PSEUDO);
exports.isRoot = isNodeType.bind(null, types_1.ROOT);
exports.isSelector = isNodeType.bind(null, types_1.SELECTOR);
exports.isString = isNodeType.bind(null, types_1.STRING);
exports.isTag = isNodeType.bind(null, types_1.TAG);
exports.isUniversal = isNodeType.bind(null, types_1.UNIVERSAL);
function isPseudoElement(node) {
    return ((0, exports.isPseudo)(node) &&
        node.value &&
        (node.value.startsWith("::") ||
            node.value.toLowerCase() === ":before" ||
            node.value.toLowerCase() === ":after" ||
            node.value.toLowerCase() === ":first-letter" ||
            node.value.toLowerCase() === ":first-line"));
}
function isPseudoClass(node) {
    return (0, exports.isPseudo)(node) && !isPseudoElement(node);
}
function isContainer(node) {
    return !!(isNode(node) && node.walk);
}
function isNamespace(node) {
    return (0, exports.isAttribute)(node) || (0, exports.isTag)(node);
}
//# sourceMappingURL=guards.js.map