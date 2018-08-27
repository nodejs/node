import { last } from '../../operator/last';
declare module '../../Observable' {
    interface Observable<T> {
        last: typeof last;
    }
}
