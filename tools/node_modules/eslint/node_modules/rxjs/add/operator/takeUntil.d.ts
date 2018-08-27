import { takeUntil } from '../../operator/takeUntil';
declare module '../../Observable' {
    interface Observable<T> {
        takeUntil: typeof takeUntil;
    }
}
