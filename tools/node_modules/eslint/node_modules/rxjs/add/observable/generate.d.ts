import { generate as staticGenerate } from '../../observable/generate';
declare module '../../Observable' {
    namespace Observable {
        let generate: typeof staticGenerate;
    }
}
