/**
 * no-op polyfills for non-node environments. tries to load the actual
 * diagnostics_channel module on platforms that support it, but fails
 * gracefully if not found. This means that the first tick of metrics
 * and tracing will be missed, but that probably doesn't matter much.
 */
// conditionally import from diagnostic_channel, fall back to dummyfill
// all we actually have to mock is the hasSubscribers, since we always check
/* v8 ignore next */
const dummy = { hasSubscribers: false };
export let metrics = dummy;
export let tracing = dummy;
import('node:diagnostics_channel')
    .then(dc => {
    metrics = dc.channel('lru-cache:metrics');
    tracing = dc.tracingChannel('lru-cache');
})
    .catch(() => { });
//# sourceMappingURL=diagnostics-channel-esm.mjs.map