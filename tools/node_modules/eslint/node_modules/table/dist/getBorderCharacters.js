"use strict";
/* eslint-disable sort-keys-fix/sort-keys-fix */
Object.defineProperty(exports, "__esModule", { value: true });
exports.getBorderCharacters = void 0;
const getBorderCharacters = (name) => {
    if (name === 'honeywell') {
        return {
            topBody: '═',
            topJoin: '╤',
            topLeft: '╔',
            topRight: '╗',
            bottomBody: '═',
            bottomJoin: '╧',
            bottomLeft: '╚',
            bottomRight: '╝',
            bodyLeft: '║',
            bodyRight: '║',
            bodyJoin: '│',
            headerJoin: '┬',
            joinBody: '─',
            joinLeft: '╟',
            joinRight: '╢',
            joinJoin: '┼',
        };
    }
    if (name === 'norc') {
        return {
            topBody: '─',
            topJoin: '┬',
            topLeft: '┌',
            topRight: '┐',
            bottomBody: '─',
            bottomJoin: '┴',
            bottomLeft: '└',
            bottomRight: '┘',
            bodyLeft: '│',
            bodyRight: '│',
            bodyJoin: '│',
            headerJoin: '┬',
            joinBody: '─',
            joinLeft: '├',
            joinRight: '┤',
            joinJoin: '┼',
        };
    }
    if (name === 'ramac') {
        return {
            topBody: '-',
            topJoin: '+',
            topLeft: '+',
            topRight: '+',
            bottomBody: '-',
            bottomJoin: '+',
            bottomLeft: '+',
            bottomRight: '+',
            bodyLeft: '|',
            bodyRight: '|',
            bodyJoin: '|',
            headerJoin: '+',
            joinBody: '-',
            joinLeft: '|',
            joinRight: '|',
            joinJoin: '|',
        };
    }
    if (name === 'void') {
        return {
            topBody: '',
            topJoin: '',
            topLeft: '',
            topRight: '',
            bottomBody: '',
            bottomJoin: '',
            bottomLeft: '',
            bottomRight: '',
            bodyLeft: '',
            bodyRight: '',
            bodyJoin: '',
            headerJoin: '',
            joinBody: '',
            joinLeft: '',
            joinRight: '',
            joinJoin: '',
        };
    }
    throw new Error('Unknown border template "' + name + '".');
};
exports.getBorderCharacters = getBorderCharacters;
