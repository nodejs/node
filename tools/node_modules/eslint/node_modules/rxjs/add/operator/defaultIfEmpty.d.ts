import { defaultIfEmpty } from '../../operator/defaultIfEmpty';
declare module '../../Observable' {
    interface Observable<T> {
        defaultIfEmpty: typeof defaultIfEmpty;
    }
}
