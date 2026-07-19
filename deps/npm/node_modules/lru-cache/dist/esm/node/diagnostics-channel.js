// simple node version that imports from node builtin
// this is built to both ESM and CommonJS on the 'node' import path
import { tracingChannel, channel } from 'node:diagnostics_channel';
export const metrics = channel('lru-cache:metrics');
export const tracing = tracingChannel('lru-cache');
//# sourceMappingURL=diagnostics-channel-node.js.map