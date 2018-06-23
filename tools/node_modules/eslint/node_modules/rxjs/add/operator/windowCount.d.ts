import { windowCount } from '../../operator/windowCount';
declare module '../../Observable' {
    interface Observable<T> {
        windowCount: typeof windowCount;
    }
}
