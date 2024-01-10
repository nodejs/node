'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('eventsource');

runner.pretendGlobalThisAs('Window');

runner.setInitScript(`
  const document = {
    title: Window.META_TITLE,
    domain: 'localhost',
  }
`);

runner.runJsTests();
