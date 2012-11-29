title: README.md
status: private

# How This Blog Works

Each `.md` file in this folder structure is a blog post.  It has a
few headers and a markdown body.  (HTML is allowed in the body as well.)

The relevant headers are:

1. title
2. author
3. status: Only posts with a status of "publish" are published.
4. category: The "release" category is treated a bit specially.
5. version: Only relevant for "release" category.
6. date
7. slug: The bit that goes on the url.  Must be unique, will be
   generated from the title and date if missing.

Posts in the "release" category are only shown in the main lists when
they are the most recent release for that version family.  The stable
branch supersedes its unstable counterpart, so the presence of a `0.8.2`
release notice will cause `0.7.10` to be hidden, but `0.6.19` would
be unaffected.

The folder structure in the blog source does not matter.  Organize files
here however makes sense.  The metadata will be sorted out in the build
later.
