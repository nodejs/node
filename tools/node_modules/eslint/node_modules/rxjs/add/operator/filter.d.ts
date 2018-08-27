import { filter } from '../../operator/filter';
declare module '../../Observable' {
    interface Observable<T> {
        filter: typeof filter;
    }
}
