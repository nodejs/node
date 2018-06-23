import { skipUntil } from '../../operator/skipUntil';
declare module '../../Observable' {
    interface Observable<T> {
        skipUntil: typeof skipUntil;
    }
}
