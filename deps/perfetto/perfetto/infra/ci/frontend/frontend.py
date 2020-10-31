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

import logging
import webapp2
import urllib

from google.appengine.api import urlfetch
from google.appengine.api import memcache
from config import GERRIT_HOST, GERRIT_PROJECT
''' Makes anonymous GET-only requests to Gerrit.

Solves the lack of CORS headers from AOSP gerrit.'''


def req_cached(url):
  '''Used for requests that return immutable data, avoid hitting Gerrit 500'''
  resp = memcache.get(url)
  if resp is not None:
    return 200, resp
  result = urlfetch.fetch(url)
  if result.status_code == 200:
    memcache.add(url, result.content, 3600 * 24)
  return result.status_code, result.content


class GerritCommitsHandler(webapp2.RequestHandler):

  def get(self, sha1):
    project = urllib.quote(GERRIT_PROJECT, '')
    url = 'https://%s/projects/%s/commits/%s' % (GERRIT_HOST, project, sha1)
    status, content = req_cached(url)
    self.response.status_int = status
    self.response.write(content[4:])  # 4: -> Strip Gerrit XSSI chars.


class GerritLogHandler(webapp2.RequestHandler):

  def get(self, first, second):
    url = 'https://%s/%s/+log/%s..%s?format=json' % (GERRIT_HOST.replace(
        '-review', ''), GERRIT_PROJECT, first, second)
    status, content = req_cached(url)
    self.response.status_int = status
    self.response.write(content[4:])  # 4: -> Strip Gerrit XSSI chars.


class GerritChangesHandler(webapp2.RequestHandler):

  def get(self):
    url = 'https://%s/changes/?q=project:%s+' % (GERRIT_HOST, GERRIT_PROJECT)
    url += self.request.query_string
    result = urlfetch.fetch(url)
    self.response.status_int = result.status_code
    self.response.write(result.content[4:])  # 4: -> Strip Gerrit XSSI chars.


app = webapp2.WSGIApplication([
    ('/gerrit/commits/([a-f0-9]+)', GerritCommitsHandler),
    ('/gerrit/log/([a-f0-9]+)..([a-f0-9]+)', GerritLogHandler),
    ('/gerrit/changes/', GerritChangesHandler),
],
                              debug=True)
