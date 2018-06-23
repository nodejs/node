import { skip } from '../../operator/skip';
declare module '../../Observable' {
    interface Observable<T> {
        skip: typeof skip;
    }
}
