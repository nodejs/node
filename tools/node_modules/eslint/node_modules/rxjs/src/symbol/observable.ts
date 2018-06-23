import { root } from '../util/root';

export function getSymbolObservable(context: any) {
  let $$observable: any;
  let Symbol = context.Symbol;

  if (typeof Symbol === 'function') {
    if (Symbol.observable) {
      $$observable = Symbol.observable;
    } else {
        $$observable = Symbol('observable');
        Symbol.observable = $$observable;
    }
  } else {
    $$observable = '@@observable';
  }

  return $$observable;
}

export const observable = getSymbolObservable(root);

/**
 * @deprecated use observable instead
 */
export const $$observable = observable;
