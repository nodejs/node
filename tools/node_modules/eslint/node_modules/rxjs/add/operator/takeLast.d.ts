import { takeLast } from '../../operator/takeLast';
declare module '../../Observable' {
    interface Observable<T> {
        takeLast: typeof takeLast;
    }
}
