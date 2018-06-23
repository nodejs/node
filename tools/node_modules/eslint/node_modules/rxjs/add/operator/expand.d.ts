import { expand } from '../../operator/expand';
declare module '../../Observable' {
    interface Observable<T> {
        expand: typeof expand;
    }
}
