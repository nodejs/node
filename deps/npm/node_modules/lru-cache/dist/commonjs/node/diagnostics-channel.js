"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.tracing = exports.metrics = void 0;
// simple node version that imports from node builtin
// this is built to both ESM and CommonJS on the 'node' import path
const node_diagnostics_channel_1 = require("node:diagnostics_channel");
exports.metrics = (0, node_diagnostics_channel_1.channel)('lru-cache:metrics');
exports.tracing = (0, node_diagnostics_channel_1.tracingChannel)('lru-cache');
//# sourceMappingURL=diagnostics-channel-node.js.map