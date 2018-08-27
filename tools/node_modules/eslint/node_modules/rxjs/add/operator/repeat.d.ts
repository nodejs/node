import { repeat } from '../../operator/repeat';
declare module '../../Observable' {
    interface Observable<T> {
        repeat: typeof repeat;
    }
}
