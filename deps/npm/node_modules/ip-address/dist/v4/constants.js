"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RE_SUBNET_STRING = exports.RE_ADDRESS = exports.GROUPS = exports.BITS = void 0;
exports.BITS = 32;
exports.GROUPS = 4;
// Each octet is 0-255 written without a leading zero. A leading zero is
// octal to the WHATWG URL parser, inet_aton, and getaddrinfo, but decimal to
// parseInt(part, 10), so accepting the notation would make this library
// disagree with the network stack about which host a string names.
exports.RE_ADDRESS = /^(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/g;
exports.RE_SUBNET_STRING = /\/\d{1,2}$/;
//# sourceMappingURL=constants.js.map