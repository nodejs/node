import { zipAll } from '../../operator/zipAll';
declare module '../../Observable' {
    interface Observable<T> {
        zipAll: typeof zipAll;
    }
}
