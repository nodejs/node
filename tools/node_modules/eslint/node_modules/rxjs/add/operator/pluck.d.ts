import { pluck } from '../../operator/pluck';
declare module '../../Observable' {
    interface Observable<T> {
        pluck: typeof pluck;
    }
}
