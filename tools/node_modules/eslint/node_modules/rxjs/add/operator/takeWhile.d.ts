import { takeWhile } from '../../operator/takeWhile';
declare module '../../Observable' {
    interface Observable<T> {
        takeWhile: typeof takeWhile;
    }
}
