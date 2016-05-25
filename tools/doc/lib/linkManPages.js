// Syscalls which appear in the docs, but which only exist in BSD / OSX
var BSD_ONLY_SYSCALLS = new Set(['lchmod']);

// Handle references to man pages, eg "open(2)" or "lchmod(2)"
// Returns modified text, with such refs replace with HTML links, for example
// '<a href="http://man7.org/linux/man-pages/man2/open.2.html">open(2)</a>'
module.exports = function linkManPages(text) {
  return text.replace(/ ([a-z]+)\((\d)\)/gm, function(match, name, number) {
    // name consists of lowercase letters, number is a single digit
    var displayAs = name + '(' + number + ')';
    if (BSD_ONLY_SYSCALLS.has(name)) {
      return ' <a href="https://www.freebsd.org/cgi/man.cgi?query=' + name +
             '&sektion=' + number + '">' + displayAs + '</a>';
    } else {
      return ' <a href="http://man7.org/linux/man-pages/man' + number +
             '/' + name + '.' + number + '.html">' + displayAs + '</a>';
    }
  });
}
