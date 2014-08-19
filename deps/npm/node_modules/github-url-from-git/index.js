// convert git:// form url to github URL, e.g.,
// git://github.com/bcoe/foo.git
// https://github.com/bcoe/foo.
function githubUrlFromGit(url, opts){
  try {
    var m = re(opts).exec(url.replace(/\.git(#.*)?$/, ''));
    var host = m[1];
    var path = m[2];
    return 'https://' + host + '/' + path;
  } catch (err) {
    // ignore
  }
};

// generate the git:// parsing regex
// with options, e.g., the ability
// to specify multiple GHE domains.
function re(opts) {
  opts = opts || {};
  // whitelist of URLs that should be treated as GitHub repos.
  var baseUrls = ['gist.github.com', 'github.com'].concat(opts.extraBaseUrls || []);
  // build regex from whitelist.
  return new RegExp(
    /^(?:https?:\/\/|git:\/\/)?(?:[^@]+@)?/.source +
    '(' + baseUrls.join('|') + ')' +
    /[:\/]([^\/]+\/[^\/]+?|[0-9]+)$/.source
  );
}

githubUrlFromGit.re = re();

module.exports = githubUrlFromGit;
