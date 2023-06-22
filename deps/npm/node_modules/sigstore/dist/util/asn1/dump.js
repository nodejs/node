"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.dump = void 0;
const tag_1 = require("./tag");
// Utility function to dump the contents of an ASN1Obj to the console.
function dump(obj, indent = 0) {
    let str = ' '.repeat(indent);
    str += tagToString(obj.tag) + ' ';
    if (obj.tag.isUniversal()) {
        switch (obj.tag.number) {
            case tag_1.UNIVERSAL_TAG.BOOLEAN:
                str += obj.toBoolean();
                break;
            case tag_1.UNIVERSAL_TAG.INTEGER:
                str += `(${obj.value.length} byte) `;
                str += obj.toInteger();
                break;
            case tag_1.UNIVERSAL_TAG.BIT_STRING: {
                const bits = obj.toBitString();
                str += `(${bits.length} bit) `;
                str += truncate(bits.map((bit) => bit.toString()).join(''));
                break;
            }
            case tag_1.UNIVERSAL_TAG.OBJECT_IDENTIFIER:
                str += obj.toOID();
                break;
            case tag_1.UNIVERSAL_TAG.SEQUENCE:
            case tag_1.UNIVERSAL_TAG.SET:
                str += `(${obj.subs.length} elem) `;
                break;
            case tag_1.UNIVERSAL_TAG.PRINTABLE_STRING:
                str += obj.value.toString('ascii');
                break;
            case tag_1.UNIVERSAL_TAG.UTC_TIME:
            case tag_1.UNIVERSAL_TAG.GENERALIZED_TIME:
                str += obj.toDate().toUTCString();
                break;
            default:
                str += `(${obj.value.length} byte) `;
                str += isASCII(obj.value)
                    ? obj.value.toString('ascii')
                    : truncate(obj.value.toString('hex').toUpperCase());
        }
    }
    else {
        if (obj.tag.constructed) {
            str += `(${obj.subs.length} elem) `;
        }
        else {
            str += `(${obj.value.length} byte) `;
            str += isASCII(obj.value)
                ? obj.value.toString('ascii')
                : obj.value.toString('hex').toUpperCase();
        }
    }
    console.log(str);
    // Recursive call for children
    obj.subs.forEach((sub) => dump(sub, indent + 2));
}
exports.dump = dump;
function tagToString(tag) {
    if (tag.isContextSpecific()) {
        return `[${tag.number.toString(16)}]`;
    }
    else {
        switch (tag.number) {
            case tag_1.UNIVERSAL_TAG.BOOLEAN:
                return 'BOOLEAN';
            case tag_1.UNIVERSAL_TAG.INTEGER:
                return 'INTEGER';
            case tag_1.UNIVERSAL_TAG.BIT_STRING:
                return 'BIT STRING';
            case tag_1.UNIVERSAL_TAG.OCTET_STRING:
                return 'OCTET STRING';
            case tag_1.UNIVERSAL_TAG.OBJECT_IDENTIFIER:
                return 'OBJECT IDENTIFIER';
            case tag_1.UNIVERSAL_TAG.SEQUENCE:
                return 'SEQUENCE';
            case tag_1.UNIVERSAL_TAG.SET:
                return 'SET';
            case tag_1.UNIVERSAL_TAG.PRINTABLE_STRING:
                return 'PrintableString';
            case tag_1.UNIVERSAL_TAG.UTC_TIME:
                return 'UTCTime';
            case tag_1.UNIVERSAL_TAG.GENERALIZED_TIME:
                return 'GeneralizedTime';
            default:
                return tag.number.toString(16);
        }
    }
}
function isASCII(buf) {
    return buf.every((b) => b >= 32 && b <= 126);
}
function truncate(str) {
    return str.length > 70 ? str.substring(0, 69) + '...' : str;
}
