import { distinct } from '../../operator/distinct';
declare module '../../Observable' {
    interface Observable<T> {
        distinct: typeof distinct;
    }
}
