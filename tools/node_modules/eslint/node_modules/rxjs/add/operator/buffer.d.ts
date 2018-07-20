import { buffer } from '../../operator/buffer';
declare module '../../Observable' {
    interface Observable<T> {
        buffer: typeof buffer;
    }
}
