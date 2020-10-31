# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from google.appengine.api import memcache
from google.appengine.api import urlfetch

import re
import webapp2

MEMCACHE_TTL_SEC = 60 * 60 * 24
BASE = 'https://google.github.io/perfetto/%s'
HEADERS = {
    'last-modified', 'content-type', 'content-length', 'content-encoding',
    'etag'
}


class MainHandler(webapp2.RequestHandler):

  def get(self):
    handler = GithubMirrorHandler()
    handler.initialize(self.request, self.response)
    return handler.get("index.html")


class GithubMirrorHandler(webapp2.RequestHandler):

  def get(self, resource):
    if not re.match('^[a-zA-Z0-9-_./]*$', resource):
      self.response.set_status(403)
      return

    url = BASE % resource
    cache = memcache.get(url)
    if not cache or self.request.get('reload'):
      result = urlfetch.fetch(url)
      if result.status_code != 200:
        memcache.delete(url)
        self.response.set_status(result.status_code)
        self.response.write(result.content)
        return
      cache = {'content-type': 'text/html'}
      for k, v in result.headers.iteritems():
        k = k.lower()
        if k in HEADERS:
          cache[k] = v
      cache['content'] = result.content
      memcache.set(url, cache, time=MEMCACHE_TTL_SEC)

    for k, v in cache.iteritems():
      if k != 'content':
        self.response.headers[k] = v
    self.response.headers['cache-control'] = 'public,max-age=600'
    self.response.write(cache['content'])


app = webapp2.WSGIApplication([
    ('/', MainHandler),
    ('/(.+)', GithubMirrorHandler),
],
                              debug=True)
