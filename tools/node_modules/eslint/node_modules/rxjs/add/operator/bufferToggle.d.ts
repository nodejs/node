import { bufferToggle } from '../../operator/bufferToggle';
declare module '../../Observable' {
    interface Observable<T> {
        bufferToggle: typeof bufferToggle;
    }
}
