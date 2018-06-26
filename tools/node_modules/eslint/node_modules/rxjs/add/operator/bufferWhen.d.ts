import { bufferWhen } from '../../operator/bufferWhen';
declare module '../../Observable' {
    interface Observable<T> {
        bufferWhen: typeof bufferWhen;
    }
}
