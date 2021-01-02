"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const metaSchema = require("./schema.json");
const metaApplicator = require("./meta/applicator.json");
const metaContent = require("./meta/content.json");
const metaCore = require("./meta/core.json");
const metaFormat = require("./meta/format.json");
const metaMetadata = require("./meta/meta-data.json");
const metaValidation = require("./meta/validation.json");
const META_SUPPORT_DATA = ["/properties"];
function addMetaSchema2019($data) {
    ;
    [
        metaSchema,
        metaApplicator,
        metaContent,
        metaCore,
        with$data(this, metaFormat),
        metaMetadata,
        with$data(this, metaValidation),
    ].forEach((sch) => this.addMetaSchema(sch, undefined, false));
    return this;
    function with$data(ajv, sch) {
        return $data ? ajv.$dataMetaSchema(sch, META_SUPPORT_DATA) : sch;
    }
}
exports.default = addMetaSchema2019;
//# sourceMappingURL=index.js.map