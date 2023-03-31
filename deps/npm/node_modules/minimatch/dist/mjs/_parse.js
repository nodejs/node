// parse a single path portion
import { parseClass } from './brace-expressions.js';
import { assertValidPattern } from './assert-valid-pattern.js';
const globUnescape = (s) => s.replace(/\\(.)/g, '$1');
const regExpEscape = (s) => s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
// "abc" -> { a:true, b:true, c:true }
const charSet = (s) => s.split('').reduce((set, c) => {
    set[c] = true;
    return set;
}, {});
const plTypes = {
    '!': { open: '(?:(?!(?:', close: '))[^/]*?)' },
    '?': { open: '(?:', close: ')?' },
    '+': { open: '(?:', close: ')+' },
    '*': { open: '(?:', close: ')*' },
    '@': { open: '(?:', close: ')' },
};
// characters that need to be escaped in RegExp.
const reSpecials = charSet('().*{}+?[]^$\\!');
// characters that indicate we have to add the pattern start
const addPatternStartSet = charSet('[.(');
// any single thing other than /
// don't need to escape / when using new RegExp()
const qmark = '[^/]';
// * => any number of characters
const star = qmark + '*?';
// TODO: take an offset and length, so we can sub-parse the extglobs
export const parse = (options, pattern, debug) => {
    assertValidPattern(pattern);
    if (pattern === '')
        return '';
    let re = '';
    let hasMagic = false;
    let escaping = false;
    // ? => one single character
    const patternListStack = [];
    const negativeLists = [];
    let stateChar = false;
    let uflag = false;
    let pl;
    // . and .. never match anything that doesn't start with .,
    // even when options.dot is set.  However, if the pattern
    // starts with ., then traversal patterns can match.
    let dotTravAllowed = pattern.charAt(0) === '.';
    let dotFileAllowed = options.dot || dotTravAllowed;
    const patternStart = () => dotTravAllowed
        ? ''
        : dotFileAllowed
            ? '(?!(?:^|\\/)\\.{1,2}(?:$|\\/))'
            : '(?!\\.)';
    const subPatternStart = (p) => p.charAt(0) === '.'
        ? ''
        : options.dot
            ? '(?!(?:^|\\/)\\.{1,2}(?:$|\\/))'
            : '(?!\\.)';
    const clearStateChar = () => {
        if (stateChar) {
            // we had some state-tracking character
            // that wasn't consumed by this pass.
            switch (stateChar) {
                case '*':
                    re += star;
                    hasMagic = true;
                    break;
                case '?':
                    re += qmark;
                    hasMagic = true;
                    break;
                default:
                    re += '\\' + stateChar;
                    break;
            }
            debug('clearStateChar %j %j', stateChar, re);
            stateChar = false;
        }
    };
    for (let i = 0, c; i < pattern.length && (c = pattern.charAt(i)); i++) {
        debug('%s\t%s %s %j', pattern, i, re, c);
        // skip over any that are escaped.
        if (escaping) {
            // completely not allowed, even escaped.
            // should be impossible.
            /* c8 ignore start */
            if (c === '/') {
                return false;
            }
            /* c8 ignore stop */
            if (reSpecials[c]) {
                re += '\\';
            }
            re += c;
            escaping = false;
            continue;
        }
        switch (c) {
            // Should already be path-split by now.
            /* c8 ignore start */
            case '/': {
                return false;
            }
            /* c8 ignore stop */
            case '\\':
                clearStateChar();
                escaping = true;
                continue;
            // the various stateChar values
            // for the "extglob" stuff.
            case '?':
            case '*':
            case '+':
            case '@':
            case '!':
                debug('%s\t%s %s %j <-- stateChar', pattern, i, re, c);
                // if we already have a stateChar, then it means
                // that there was something like ** or +? in there.
                // Handle the stateChar, then proceed with this one.
                debug('call clearStateChar %j', stateChar);
                clearStateChar();
                stateChar = c;
                // if extglob is disabled, then +(asdf|foo) isn't a thing.
                // just clear the statechar *now*, rather than even diving into
                // the patternList stuff.
                if (options.noext)
                    clearStateChar();
                continue;
            case '(': {
                if (!stateChar) {
                    re += '\\(';
                    continue;
                }
                const plEntry = {
                    type: stateChar,
                    start: i - 1,
                    reStart: re.length,
                    open: plTypes[stateChar].open,
                    close: plTypes[stateChar].close,
                };
                debug(pattern, '\t', plEntry);
                patternListStack.push(plEntry);
                // negation is (?:(?!(?:js)(?:<rest>))[^/]*)
                re += plEntry.open;
                // next entry starts with a dot maybe?
                if (plEntry.start === 0 && plEntry.type !== '!') {
                    dotTravAllowed = true;
                    re += subPatternStart(pattern.slice(i + 1));
                }
                debug('plType %j %j', stateChar, re);
                stateChar = false;
                continue;
            }
            case ')': {
                const plEntry = patternListStack[patternListStack.length - 1];
                if (!plEntry) {
                    re += '\\)';
                    continue;
                }
                patternListStack.pop();
                // closing an extglob
                clearStateChar();
                hasMagic = true;
                pl = plEntry;
                // negation is (?:(?!js)[^/]*)
                // The others are (?:<pattern>)<type>
                re += pl.close;
                if (pl.type === '!') {
                    negativeLists.push(Object.assign(pl, { reEnd: re.length }));
                }
                continue;
            }
            case '|': {
                const plEntry = patternListStack[patternListStack.length - 1];
                if (!plEntry) {
                    re += '\\|';
                    continue;
                }
                clearStateChar();
                re += '|';
                // next subpattern can start with a dot?
                if (plEntry.start === 0 && plEntry.type !== '!') {
                    dotTravAllowed = true;
                    re += subPatternStart(pattern.slice(i + 1));
                }
                continue;
            }
            // these are mostly the same in regexp and glob
            case '[':
                // swallow any state-tracking char before the [
                clearStateChar();
                const [src, needUflag, consumed, magic] = parseClass(pattern, i);
                if (consumed) {
                    re += src;
                    uflag = uflag || needUflag;
                    i += consumed - 1;
                    hasMagic = hasMagic || magic;
                }
                else {
                    re += '\\[';
                }
                continue;
            case ']':
                re += '\\' + c;
                continue;
            default:
                // swallow any state char that wasn't consumed
                clearStateChar();
                re += regExpEscape(c);
                break;
        } // switch
    } // for
    // handle the case where we had a +( thing at the *end*
    // of the pattern.
    // each pattern list stack adds 3 chars, and we need to go through
    // and escape any | chars that were passed through as-is for the regexp.
    // Go through and escape them, taking care not to double-escape any
    // | chars that were already escaped.
    for (pl = patternListStack.pop(); pl; pl = patternListStack.pop()) {
        let tail;
        tail = re.slice(pl.reStart + pl.open.length);
        debug(pattern, 'setting tail', re, pl);
        // maybe some even number of \, then maybe 1 \, followed by a |
        tail = tail.replace(/((?:\\{2}){0,64})(\\?)\|/g, (_, $1, $2) => {
            if (!$2) {
                // the | isn't already escaped, so escape it.
                $2 = '\\';
                // should already be done
                /* c8 ignore start */
            }
            /* c8 ignore stop */
            // need to escape all those slashes *again*, without escaping the
            // one that we need for escaping the | character.  As it works out,
            // escaping an even number of slashes can be done by simply repeating
            // it exactly after itself.  That's why this trick works.
            //
            // I am sorry that you have to see this.
            return $1 + $1 + $2 + '|';
        });
        debug('tail=%j\n   %s', tail, tail, pl, re);
        const t = pl.type === '*' ? star : pl.type === '?' ? qmark : '\\' + pl.type;
        hasMagic = true;
        re = re.slice(0, pl.reStart) + t + '\\(' + tail;
    }
    // handle trailing things that only matter at the very end.
    clearStateChar();
    if (escaping) {
        // trailing \\
        re += '\\\\';
    }
    // only need to apply the nodot start if the re starts with
    // something that could conceivably capture a dot
    const addPatternStart = addPatternStartSet[re.charAt(0)];
    // Hack to work around lack of negative lookbehind in JS
    // A pattern like: *.!(x).!(y|z) needs to ensure that a name
    // like 'a.xyz.yz' doesn't match.  So, the first negative
    // lookahead, has to look ALL the way ahead, to the end of
    // the pattern.
    for (let n = negativeLists.length - 1; n > -1; n--) {
        const nl = negativeLists[n];
        const nlBefore = re.slice(0, nl.reStart);
        const nlFirst = re.slice(nl.reStart, nl.reEnd - 8);
        let nlAfter = re.slice(nl.reEnd);
        const nlLast = re.slice(nl.reEnd - 8, nl.reEnd) + nlAfter;
        // Handle nested stuff like *(*.js|!(*.json)), where open parens
        // mean that we should *not* include the ) in the bit that is considered
        // "after" the negated section.
        const closeParensBefore = nlBefore.split(')').length;
        const openParensBefore = nlBefore.split('(').length - closeParensBefore;
        let cleanAfter = nlAfter;
        for (let i = 0; i < openParensBefore; i++) {
            cleanAfter = cleanAfter.replace(/\)[+*?]?/, '');
        }
        nlAfter = cleanAfter;
        const dollar = nlAfter === '' ? '(?:$|\\/)' : '';
        re = nlBefore + nlFirst + nlAfter + dollar + nlLast;
    }
    // if the re is not "" at this point, then we need to make sure
    // it doesn't match against an empty path part.
    // Otherwise a/* will match a/, which it should not.
    if (re !== '' && hasMagic) {
        re = '(?=.)' + re;
    }
    if (addPatternStart) {
        re = patternStart() + re;
    }
    // if it's nocase, and the lcase/uppercase don't match, it's magic
    if (options.nocase && !hasMagic && !options.nocaseMagicOnly) {
        hasMagic = pattern.toUpperCase() !== pattern.toLowerCase();
    }
    // skip the regexp for non-magical patterns
    // unescape anything in it, though, so that it'll be
    // an exact match against a file etc.
    if (!hasMagic) {
        return globUnescape(re);
    }
    return re;
};
//# sourceMappingURL=_parse.js.map