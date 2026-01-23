import { parse, sep, normalize as norm } from 'node:path';
function* commonArrayMembers(a, b) {
    const [l, s] = a.length > b.length ? [a, b] : [b, a];
    for (const x of s) {
        if (x === l.shift())
            yield x;
        else
            break;
    }
}
const cap = (a, b) => a === b ? a
    : !a || !b ? null
        : parse(a).root !== parse(b).root ? null
            : [...commonArrayMembers(norm(a).split(sep), norm(b).split(sep))].join(sep);
export const commonAncestorPath = (...paths) => paths.reduce(cap);
//# sourceMappingURL=index.js.map