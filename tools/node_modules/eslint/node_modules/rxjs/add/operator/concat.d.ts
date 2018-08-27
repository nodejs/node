import { concat } from '../../operator/concat';
declare module '../../Observable' {
    interface Observable<T> {
        concat: typeof concat;
    }
}
