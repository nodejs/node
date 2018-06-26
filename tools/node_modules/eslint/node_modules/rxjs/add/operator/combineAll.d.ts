import { combineAll } from '../../operator/combineAll';
declare module '../../Observable' {
    interface Observable<T> {
        combineAll: typeof combineAll;
    }
}
