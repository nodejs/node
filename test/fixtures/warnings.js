'use strict';

const securityWarning = new Error('a security warning');
securityWarning.name = 'SecurityWarning';
process.emit('warning', securityWarning);


const badPracticeWarning = new Error('a bad practice warning');
badPracticeWarning.name = 'BadPracticeWarning';
process.emit('warning', badPracticeWarning);


const deprecationWarning = new Error('a deprecation warning');
deprecationWarning.name = 'DeprecationWarning';
process.emit('warning', deprecationWarning);
