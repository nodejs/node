import { root } from '../util/root';
export function getSymbolObservable(context) {
    let $$observable;
    let Symbol = context.Symbol;
    if (typeof Symbol === 'function') {
        if (Symbol.observable) {
            $$observable = Symbol.observable;
        }
        else {
            $$observable = Symbol('observable');
            Symbol.observable = $$observable;
        }
    }
    else {
        $$observable = '@@observable';
    }
    return $$observable;
}
export const observable = getSymbolObservable(root);
/**
 * @deprecated use observable instead
 */
export const $$observable = observable;
//# sourceMappingURL=observable.js.map