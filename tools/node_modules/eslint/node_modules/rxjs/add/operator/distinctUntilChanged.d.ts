import { distinctUntilChanged } from '../../operator/distinctUntilChanged';
declare module '../../Observable' {
    interface Observable<T> {
        distinctUntilChanged: typeof distinctUntilChanged;
    }
}
