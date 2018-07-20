import { debounceTime } from '../../operator/debounceTime';
declare module '../../Observable' {
    interface Observable<T> {
        debounceTime: typeof debounceTime;
    }
}
