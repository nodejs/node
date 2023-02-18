"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.canonicalize = void 0;
const QUOTATION_MARK = Buffer.from('"');
const COMMA = Buffer.from(',');
const COLON = Buffer.from(':');
const LEFT_SQUARE_BRACKET = Buffer.from('[');
const RIGHT_SQUARE_BRACKET = Buffer.from(']');
const LEFT_CURLY_BRACKET = Buffer.from('{');
const RIGHT_CURLY_BRACKET = Buffer.from('}');
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function canonicalize(object) {
    let buffer = Buffer.from('');
    if (object === null || typeof object !== 'object' || object.toJSON != null) {
        // Primitives or toJSONable objects
        if (typeof object === 'string') {
            buffer = Buffer.concat([
                buffer,
                QUOTATION_MARK,
                Buffer.from(object),
                QUOTATION_MARK,
            ]);
        }
        else {
            buffer = Buffer.concat([buffer, Buffer.from(JSON.stringify(object))]);
        }
    }
    else if (Array.isArray(object)) {
        // Array - maintain element order
        buffer = Buffer.concat([buffer, LEFT_SQUARE_BRACKET]);
        let first = true;
        object.forEach((element) => {
            if (!first) {
                buffer = Buffer.concat([buffer, COMMA]);
            }
            first = false;
            // recursive call
            buffer = Buffer.concat([buffer, canonicalize(element)]);
        });
        buffer = Buffer.concat([buffer, RIGHT_SQUARE_BRACKET]);
    }
    else {
        // Object - Sort properties before serializing
        buffer = Buffer.concat([buffer, LEFT_CURLY_BRACKET]);
        let first = true;
        Object.keys(object)
            .sort()
            .forEach((property) => {
            if (!first) {
                buffer = Buffer.concat([buffer, COMMA]);
            }
            first = false;
            buffer = Buffer.concat([buffer, Buffer.from(JSON.stringify(property))]);
            buffer = Buffer.concat([buffer, COLON]);
            // recursive call
            buffer = Buffer.concat([buffer, canonicalize(object[property])]);
        });
        buffer = Buffer.concat([buffer, RIGHT_CURLY_BRACKET]);
    }
    return buffer;
}
exports.canonicalize = canonicalize;
