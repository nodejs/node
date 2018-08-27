import { skipWhile } from '../../operator/skipWhile';
declare module '../../Observable' {
    interface Observable<T> {
        skipWhile: typeof skipWhile;
    }
}
