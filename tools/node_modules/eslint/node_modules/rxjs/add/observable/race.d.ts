import { race as staticRace } from '../../observable/race';
declare module '../../Observable' {
    namespace Observable {
        let race: typeof staticRace;
    }
}
