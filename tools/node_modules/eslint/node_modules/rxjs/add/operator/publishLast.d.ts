import { publishLast } from '../../operator/publishLast';
declare module '../../Observable' {
    interface Observable<T> {
        publishLast: typeof publishLast;
    }
}
