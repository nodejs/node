import { ignoreElements } from '../../operator/ignoreElements';
declare module '../../Observable' {
    interface Observable<T> {
        ignoreElements: typeof ignoreElements;
    }
}
