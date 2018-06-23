import { timer as staticTimer } from '../../observable/timer';
declare module '../../Observable' {
    namespace Observable {
        let timer: typeof staticTimer;
    }
}
