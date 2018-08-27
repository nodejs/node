import { max } from '../../operator/max';
declare module '../../Observable' {
    interface Observable<T> {
        max: typeof max;
    }
}
