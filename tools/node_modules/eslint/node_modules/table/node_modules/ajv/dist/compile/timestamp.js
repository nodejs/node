"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("./codegen");
const DATE_TIME = /^(\d\d\d\d)-(\d\d)-(\d\d)(?:t|\s)(\d\d):(\d\d):(\d\d)(?:\.\d+)?(?:z|([+-]\d\d)(?::?(\d\d))?)$/i;
const DAYS = [0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
function validTimestamp(str) {
    // http://tools.ietf.org/html/rfc3339#section-5.6
    const matches = DATE_TIME.exec(str);
    if (!matches)
        return false;
    const y = +matches[1];
    const m = +matches[2];
    const d = +matches[3];
    const hr = +matches[4];
    const min = +matches[5];
    const sec = +matches[6];
    const tzH = +(matches[7] || 0);
    const tzM = +(matches[8] || 0);
    return (m >= 1 &&
        m <= 12 &&
        d >= 1 &&
        (d <= DAYS[m] ||
            // leap year: https://tools.ietf.org/html/rfc3339#appendix-C
            (m === 2 && d === 29 && (y % 100 === 0 ? y % 400 === 0 : y % 4 === 0))) &&
        ((hr <= 23 && min <= 59 && sec <= 59) ||
            // leap second
            (hr - tzH === 23 && min - tzM === 59 && sec === 60)));
}
exports.default = validTimestamp;
validTimestamp.code = codegen_1._ `require("ajv/dist/compile/timestamp").default`;
//# sourceMappingURL=timestamp.js.map