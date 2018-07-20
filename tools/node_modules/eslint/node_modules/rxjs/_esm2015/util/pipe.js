import { noop } from './noop';
/* tslint:enable:max-line-length */
export function pipe(...fns) {
    return pipeFromArray(fns);
}
/* @internal */
export function pipeFromArray(fns) {
    if (!fns) {
        return noop;
    }
    if (fns.length === 1) {
        return fns[0];
    }
    return function piped(input) {
        return fns.reduce((prev, fn) => fn(prev), input);
    };
}
//# sourceMappingURL=pipe.js.map