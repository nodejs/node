import { _finally } from '../../operator/finally';
declare module '../../Observable' {
    interface Observable<T> {
        finally: typeof _finally;
        _finally: typeof _finally;
    }
}
