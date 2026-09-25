export function resolve(specifier, context, next) {
    if (specifier === 'never-settle-resolve') return new Promise(() => {});
    if (specifier === 'never-settle-load') return { __proto__: null, shortCircuit: true, url: 'never-settle:///' };
    return next(specifier, context);
}

export function load(url, context, next) {
    if (url === 'never-settle:///') return new Promise(() => {});
    return next(url, context);
}
