"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.commonAncestorPath = void 0;
const node_path_1 = require("node:path");
function* commonArrayMembers(a, b) {
    const [l, s] = a.length > b.length ? [a, b] : [b, a];
    for (const x of s) {
        if (x === l.shift())
            yield x;
        else
            break;
    }
}
const cap = (a, b) => a === b ? a
    : !a || !b ? null
        : (0, node_path_1.parse)(a).root !== (0, node_path_1.parse)(b).root ? null
            : [...commonArrayMembers((0, node_path_1.normalize)(a).split(node_path_1.sep), (0, node_path_1.normalize)(b).split(node_path_1.sep))].join(node_path_1.sep);
const commonAncestorPath = (...paths) => paths.reduce(cap);
exports.commonAncestorPath = commonAncestorPath;
//# sourceMappingURL=index.js.map