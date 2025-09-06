import * as mod from 'node:module';

mod.register(new URL('hooks.js', import.meta.url).toString());
