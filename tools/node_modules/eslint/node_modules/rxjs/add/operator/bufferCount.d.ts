import { bufferCount } from '../../operator/bufferCount';
declare module '../../Observable' {
    interface Observable<T> {
        bufferCount: typeof bufferCount;
    }
}
