import { window } from '../../operator/window';
declare module '../../Observable' {
    interface Observable<T> {
        window: typeof window;
    }
}
