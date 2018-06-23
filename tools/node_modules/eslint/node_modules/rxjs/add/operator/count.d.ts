import { count } from '../../operator/count';
declare module '../../Observable' {
    interface Observable<T> {
        count: typeof count;
    }
}
