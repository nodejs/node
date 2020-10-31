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

import json
import httplib2
import os
import logging
import threading

from datetime import datetime
from oauth2client.client import GoogleCredentials
from config import PROJECT

tls = threading.local()

# Caller has to initialize this
SCOPES = []


class ConcurrentModificationError(Exception):
  pass


def get_gerrit_credentials():
  '''Retrieve the credentials used to authenticate Gerrit requests

  Returns a tuple (user, gitcookie). These fields are obtained from the Gerrit
  'New HTTP password' page which generates a .gitcookie file and stored in the
  project datastore.
  user: typically looks like git-user.gmail.com.
  gitcookie: is the password after the = token.
  '''
  body = {'query': {'kind': [{'name': 'GerritAuth'}]}}
  res = req(
      'POST',
      'https://datastore.googleapis.com/v1/projects/%s:runQuery' % PROJECT,
      body=body)
  auth = res['batch']['entityResults'][0]['entity']['properties']
  user = auth['user']['stringValue']
  gitcookie = auth['gitcookie']['stringValue']
  return user, gitcookie


def req(method, uri, body=None, req_etag=False, etag=None, gerrit=False):
  '''Helper function to handle authenticated HTTP requests.

  Cloud API and Gerrit require two different types of authentication and as
  such need to be handled differently. The HTTP connection is cached in the
  TLS slot to avoid refreshing oauth tokens too often for back-to-back requests.
  Appengine takes care of clearing the TLS slot upon each frontend request so
  these connections won't be recycled for too long.
  '''
  hdr = {'Content-Type': 'application/json; charset=UTF-8'}
  tls_key = 'gerrit_http' if gerrit else 'oauth2_http'
  if hasattr(tls, tls_key):
    http = getattr(tls, tls_key)
  else:
    http = httplib2.Http()
    setattr(tls, tls_key, http)
    if gerrit:
      http.add_credentials(*get_gerrit_credentials())
    elif SCOPES:
      creds = GoogleCredentials.get_application_default().create_scoped(SCOPES)
      creds.authorize(http)

  if req_etag:
    hdr['X-Firebase-ETag'] = 'true'
  if etag:
    hdr['if-match'] = etag
  body = None if body is None else json.dumps(body)
  logging.debug('%s %s', method, uri)
  resp, res = http.request(uri, method=method, headers=hdr, body=body)
  if resp.status == 200:
    res = res[4:] if gerrit else res  # Strip Gerrit XSSI projection chars.
    return (json.loads(res), resp['etag']) if req_etag else json.loads(res)
  elif resp.status == 412:
    raise ConcurrentModificationError()
  else:
    delattr(tls, tls_key)
    raise Exception(resp, res)


# Datetime functions to deal with the fact that Javascript expects a trailing
# 'Z' (Z == 'Zulu' == UTC) for timestamps.


def parse_iso_time(time_str):
  return datetime.strptime(time_str, r'%Y-%m-%dT%H:%M:%SZ')


def utc_now_iso(utcnow=None):
  return (utcnow or datetime.utcnow()).strftime(r'%Y-%m-%dT%H:%M:%SZ')


def init_logging():
  logging.basicConfig(
      format='%(asctime)s %(levelname)-8s %(message)s',
      level=logging.DEBUG if os.getenv('VERBOSE') else logging.INFO,
      datefmt=r'%Y-%m-%d %H:%M:%S')
