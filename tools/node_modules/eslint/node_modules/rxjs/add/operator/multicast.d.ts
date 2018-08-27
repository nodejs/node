import { multicast } from '../../operator/multicast';
declare module '../../Observable' {
    interface Observable<T> {
        multicast: typeof multicast;
    }
}
