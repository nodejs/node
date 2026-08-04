export const defaultPerf = (typeof performance === 'object' &&
    performance &&
    typeof performance.now === 'function') ?
    /* c8 ignore start - this gets covered, but c8 gets confused */
    performance
    : /* c8 ignore stop */ Date;
//# sourceMappingURL=perf.js.map