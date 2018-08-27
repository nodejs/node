import { subscribeOn } from '../../operator/subscribeOn';
declare module '../../Observable' {
    interface Observable<T> {
        subscribeOn: typeof subscribeOn;
    }
}
