import { single } from '../../operator/single';
declare module '../../Observable' {
    interface Observable<T> {
        single: typeof single;
    }
}
