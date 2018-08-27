import { exhaust } from '../../operator/exhaust';
declare module '../../Observable' {
    interface Observable<T> {
        exhaust: typeof exhaust;
    }
}
