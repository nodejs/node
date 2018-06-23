import { skipLast } from '../../operator/skipLast';
declare module '../../Observable' {
    interface Observable<T> {
        skipLast: typeof skipLast;
    }
}
