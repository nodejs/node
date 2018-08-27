import { interval as staticInterval } from '../../observable/interval';
declare module '../../Observable' {
    namespace Observable {
        let interval: typeof staticInterval;
    }
}
