import { toArray } from '../../operator/toArray';
declare module '../../Observable' {
    interface Observable<T> {
        toArray: typeof toArray;
    }
}
