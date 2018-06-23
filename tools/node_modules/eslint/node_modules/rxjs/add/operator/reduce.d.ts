import { reduce } from '../../operator/reduce';
declare module '../../Observable' {
    interface Observable<T> {
        reduce: typeof reduce;
    }
}
