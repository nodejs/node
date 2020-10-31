# Copyright (C) 2019 The Android Open Source Project
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
import webapp2

import base64

BASE = 'https://android.googlesource.com/platform/external/perfetto.git/' \
       '+/master/%s?format=TEXT'

RESOURCES = {
    'traceconv': 'tools/traceconv',
    'trace_processor': 'tools/trace_processor',
}


class RedirectHandler(webapp2.RequestHandler):

  def get(self):
    self.error(301)
    self.response.headers['Location'] = 'https://www.perfetto.dev/'


class GitilesMirrorHandler(webapp2.RequestHandler):

  def get(self, resource):
    resource = resource.lower()
    if resource not in RESOURCES:
      self.error(404)
      self.response.out.write('Resource "%s" not found' % resource)
      return

    url = BASE % RESOURCES[resource]
    contents = memcache.get(url)
    if not contents or self.request.get('reload'):
      result = urlfetch.fetch(url)
      if result.status_code != 200:
        memcache.delete(url)
        self.response.set_status(result.status_code)
        self.response.write(
            'http error %d while fetching %s' % (result.status_code, url))
        return
      contents = base64.b64decode(result.content)
      memcache.set(url, contents, time=3600)  # 1h
    self.response.headers['Content-Type'] = 'text/plain'
    self.response.headers['Content-Disposition'] = \
        'attachment; filename="%s"' % resource
    self.response.write(contents)


app = webapp2.WSGIApplication([
    ('/', RedirectHandler),
    ('/(.*)', GitilesMirrorHandler),
],
                              debug=True)
