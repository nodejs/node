import { onErrorResumeNext as staticOnErrorResumeNext } from '../../observable/onErrorResumeNext';
declare module '../../Observable' {
    namespace Observable {
        let onErrorResumeNext: typeof staticOnErrorResumeNext;
    }
}
