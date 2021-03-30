"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const resolve_1 = require("./resolve");
class MissingRefError extends Error {
    constructor(baseId, ref, msg) {
        super(msg || `can't resolve reference ${ref} from id ${baseId}`);
        this.missingRef = resolve_1.resolveUrl(baseId, ref);
        this.missingSchema = resolve_1.normalizeId(resolve_1.getFullPath(this.missingRef));
    }
}
exports.default = MissingRefError;
//# sourceMappingURL=ref_error.js.map