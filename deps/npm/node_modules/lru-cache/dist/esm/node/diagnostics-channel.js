// simple node version that imports from node builtin
// this gets compiled to a require() commonjs-style override,
// not using top level await on a conditional dynamic import
import { tracingChannel, channel } from 'node:diagnostics_channel';
export const metrics = channel('lru-cache:metrics');
export const tracing = tracingChannel('lru-cache');
//# sourceMappingURL=diagnostics-channel-node.mjs.map