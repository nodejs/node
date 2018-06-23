import { debounce } from '../../operator/debounce';
declare module '../../Observable' {
    interface Observable<T> {
        debounce: typeof debounce;
    }
}
