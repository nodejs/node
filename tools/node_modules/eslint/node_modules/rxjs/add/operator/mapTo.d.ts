import { mapTo } from '../../operator/mapTo';
declare module '../../Observable' {
    interface Observable<T> {
        mapTo: typeof mapTo;
    }
}
