# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


CLIENT_PORT = 9991  # Port for the local client to connect to.
PEER_PORT = 9992  # Port for peers on the network to connect to.
PRESENCE_PORT = 9993  # Port for presence daemon.
STATUS_PORT = 9994  # Port for network requests not related to workpackets.

END_OF_STREAM = "end of dtest stream"  # Marker for end of network requests.
SIZE_T = 4  # Number of bytes used for network request size header.

# Messages understood by the local request handler.
ADD_TRUSTED = "add trusted"
INFORM_DURATION = "inform about duration"
REQUEST_PEERS = "get peers"
UNRESPONSIVE_PEER = "unresponsive peer"
REQUEST_PUBKEY_FINGERPRINT = "get pubkey fingerprint"
REQUEST_STATUS = "get status"
UPDATE_PERF = "update performance"

# Messages understood by the status request handler.
LIST_TRUSTED_PUBKEYS = "list trusted pubkeys"
GET_SIGNED_PUBKEY = "pass on signed pubkey"
NOTIFY_NEW_TRUSTED = "new trusted peer"
TRUST_YOU_NOW = "trust you now"
DO_YOU_TRUST = "do you trust"
