import { delay } from '../../operator/delay';
declare module '../../Observable' {
    interface Observable<T> {
        delay: typeof delay;
    }
}
