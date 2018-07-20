import { find } from '../../operator/find';
declare module '../../Observable' {
    interface Observable<T> {
        find: typeof find;
    }
}
