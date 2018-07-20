import { exhaustMap } from '../../operator/exhaustMap';
declare module '../../Observable' {
    interface Observable<T> {
        exhaustMap: typeof exhaustMap;
    }
}
