import { isEmpty } from '../../operator/isEmpty';
declare module '../../Observable' {
    interface Observable<T> {
        isEmpty: typeof isEmpty;
    }
}
