var sax = require('sax');
var fs = require('fs');
var parser = sax.parser(false, { lowercase: true });
var assert = require('assert');
var mkdirp = require('mkdirp');
var url = require('url');

var input = fs.createReadStream(process.argv[2]);
input.on('data', function(c) {
  parser.write(c.toString());
});
input.on('end', parser.end.bind(parser));

var post = null;
var author = null;
var authors = {};
mkdirp.sync('out');

parser.onopentag = function (tag) {
  switch (tag.name) {
    case 'wp:author':
      assert(author === null);
      author = {};
      author.text = '';
      return;

    case 'wp:author_login':
      assert(author);
      author.field = 'login';
      author.text = '';
      return;

    case 'wp:author_display_name':
      assert(author);
      author.field = 'name';
      author.text = '';
      return

    case 'wp:author_first_name':
      assert(author);
      author.field = 'first_name';
      author.text = '';
      return;

    case 'wp:author_last_name':
      assert(author);
      author.field = 'last_name';
      author.text = '';
      return;

    case 'item':
      assert(post === null);
      post = {};
      post.text = '';
      return;

    case 'title':
      if (post === null) return;
      post.field = 'title';
      return

    case 'pubDate':
    case 'wp:post_date':
      post.field = 'date';
      return;

    case 'dc:creator':
      post.field = 'author';
      return;

    case 'wp:status':
      post.field = 'status';
      return;

    case 'category':
      post.field = 'category';
      return;

    case 'content:encoded':
      post.field = 'body';
      return;

    case 'link':
      if (post) post.field = 'link';
      return;

    default:
      if (post) post.field = null;
      if (author) author.field = null;
      return;
  }
};

parser.onclosetag = function (tagName, tag) {
  switch (tagName) {
    case 'wp:author':
      assert(author);
      finishAuthor();
      return;
    case 'item':
      assert(post);
      finishPost();
      return;
    default:
      if (post && post.field || author && author.field) finishField();
      return;
  }
};

parser.ontext = parser.oncdata = function (text) {
  if (author) {
    if (author.field) author.text += text;
    else author.text = '';
  } else if (post) {
    if (post.field) post.text += text;
    else post.field = '';
  }
};

function finishField() {
  if (post && post.field) {
    post[post.field] = post.text;
    post.field = null;
    post.text = '';
  } else if (author && author.field) {
    author[author.field] = author.text;
    author.field = null;
    author.text = '';
  }
}

function finishPost() {
  // don't port drafts.
  if (post.status === 'draft') {
    return post = null;
  }
  post.date = new Date(post.date);

  if (post.link) {
    post.slug =
      url.parse(post.link)
        .pathname
        .replace(/\/+$/, '')
        .split('/')
        .pop();
  }
  if (!post.slug) {
    post.slug =
      (post.title + '-' + post.date.toISOString())
        .replace(/[^a-z0-9]+/gi, '-')
        .replace(/^-|-$/g, '')
        .toLowerCase();
  }
  post.slug = post.slug || '-';

  delete post.text
  delete post.link
  delete post.field
  post.author = authors[post.author] || post.author;

  post.body = post.body || '';

  // actually write it!
  var output = [];
  Object.keys(post)
  .filter(function (f) { return f !== 'body' }).forEach(function (k) {
    output.push(k + ': ' + post[k]);
  })
  output = output.join('\n') + '\n\n' + post.body.trim() + '\n';

  var f = 'out/' + post.category + '/' + post.slug + '.md';
  console.log(f, post.title);
  mkdirp.sync('out/' + post.category)
  fs.writeFileSync(f, output, 'utf8');

  post = null;
}

function finishAuthor () {
  author.name = author.name ||
                (author.first_name + ' ' + author.last_name) ||
                author.login;
  delete author.first_name
  delete author.last_name
  delete author.text
  delete author.field
  authors[author.login] = author.name
  author = null;
}
