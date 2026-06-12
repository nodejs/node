#!/usr/bin/env python3
#
# Copyright 2024 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
import sys, os, os.path, glob, json, re

re_version = re.compile(r'''^OpenSSL/[0-9]+\.[0-9]\.[0-9](-[^ ]+)? ([^)]+)''')

class Unexpected(Exception):
    def __init__(self, filename, msg):
        Exception.__init__(self, f"file {repr(filename)}: {msg}")

class Malformed(Exception):
    def __init__(self, line, msg):
        Exception.__init__(self, f"{line}: {msg}")

event_type_counts = {}
frame_type_counts = {}

def load_file(filename):
    objs = []
    with open(filename, 'r') as fi:
        for line in fi:
            if line[0] != '\x1e':
                raise Unexpected(filename, "expected JSON-SEQ leader")

            line = line[1:]
            try:
                objs.append(json.loads(line))
            except:
                fi.seek(0)
                fdata = fi.read()
                print(fdata)
                raise Malformed(line, "Malformed json input")
    return objs

def check_header(filename, hdr):
    if not 'qlog_format' in hdr:
        raise Unexpected(filename, "must have qlog_format in header line")

    if not 'qlog_version' in hdr:
        raise Unexpected(filename, "must have qlog_version in header line")

    if not 'trace' in hdr:
        raise Unexpected(filename, "must have trace in header line")

    hdr_trace = hdr["trace"]
    if not 'common_fields' in hdr_trace:
        raise Unexpected(filename, "must have common_fields in header line")

    if not 'vantage_point' in hdr_trace:
        raise Unexpected(filename, "must have vantage_point in header line")

    if hdr_trace["vantage_point"].get('type') not in ('client', 'server'):
        raise Unexpected(filename, "unexpected vantage_point")

    vp_name = hdr_trace["vantage_point"].get('name')
    if type(vp_name) != str:
        raise Unexpected(filename, "expected vantage_point name")

    if not re_version.match(vp_name):
        raise Unexpected(filename, "expected correct vantage_point format")

    hdr_common_fields = hdr_trace["common_fields"]
    if hdr_common_fields.get("time_format") != "delta":
        raise Unexpected(filename, "must have expected time_format")

    if hdr_common_fields.get("protocol_type") != ["QUIC"]:
        raise Unexpected(filename, "must have expected protocol_type")

    if hdr["qlog_format"] != "JSON-SEQ":
        raise Unexpected(filename, "unexpected qlog_format")

    if hdr["qlog_version"] != "0.3":
        raise Unexpected(filename, "unexpected qlog_version")

def check_event(filename, event):
    name = event.get("name")

    if type(name) != str:
        raise Unexpected(filename, "expected event to have name")

    event_type_counts.setdefault(name, 0)
    event_type_counts[name] += 1

    if type(event.get("time")) != int:
        raise Unexpected(filename, "expected event to have time")

    data = event.get('data')
    if type(data) != dict:
        raise Unexpected(filename, "expected event to have data")

    if "qlog_format" in event:
        raise Unexpected(filename, "event must not be header line")

    if name in ('transport:packet_sent', 'transport:packet_received'):
        check_packet_header(filename, event, data.get('header'))

        datagram_id = data.get('datagram_id')
        if type(datagram_id) != int:
            raise Unexpected(filename, "datagram ID must be integer")

        for frame in data.get('frames', []):
            check_frame(filename, event, frame)

def check_packet_header(filename, event, header):
    if type(header) != dict:
        raise Unexpected(filename, "expected object for packet header")

    # packet type -> has frames?
    packet_types = {
            'version_negotiation': False,
            'retry': False,
            'initial': True,
            'handshake': True,
            '0RTT': True,
            '1RTT': True,
    }

    data = event['data']
    packet_type = header.get('packet_type')
    if packet_type not in packet_types:
        raise Unexpected(filename, f"unexpected packet type: {packet_type}")

    if type(header.get('dcid')) != str:
        raise Unexpected(filename, "expected packet event to have DCID")
    if packet_type != '1RTT' and type(header.get('scid')) != str:
        raise Unexpected(filename, "expected packet event to have SCID")

    if type(data.get('datagram_id')) != int:
        raise Unexpected(filename, "expected packet event to have datagram ID")

    if packet_types[packet_type]:
        if type(header.get('packet_number')) != int:
            raise Unexpected(filename, f"expected packet event to have packet number")
        if type(data.get('frames')) != list:
            raise Unexpected(filename, "expected packet event to have frames")

def check_frame(filename, event, frame):
    frame_type = frame.get('frame_type')
    if type(frame_type) != str:
        raise Unexpected(filename, "frame must have frame_type field")

    frame_type_counts.setdefault(event['name'], {})
    counts = frame_type_counts[event['name']]

    counts.setdefault(frame_type, 0)
    counts[frame_type] += 1

def check_file(filename):
    objs = load_file(filename)
    if len(objs) < 2:
        raise Unexpected(filename, "must have at least two objects")

    check_header(filename, objs[0])
    for event in objs[1:]:
        check_event(filename, event)

def run():
    num_files = 0

    # Check each file for validity.
    qlogdir = os.environ['QLOGDIR']
    for filename in glob.glob(os.path.join(qlogdir, '*.sqlog')):
        check_file(filename)
        num_files += 1

    # Check that all supported events were generated.
    required_events = (
        "transport:parameters_set",
        "connectivity:connection_state_updated",
        "connectivity:connection_started",
        "transport:packet_sent",
        "transport:packet_received",
        "connectivity:connection_closed"
    )

    if num_files < 300:
        raise Unexpected(qlogdir, f"unexpectedly few output files: {num_files}")

    for required_event in required_events:
        count = event_type_counts.get(required_event, 0)
        if count < 100:
            raise Unexpected(qlogdir, f"unexpectedly low count of event '{required_event}': got {count}")

    # For each direction, ensure that at least one of the tests we run generated
    # a given frame type.
    required_frame_types = (
        "padding",
        "ping",
        "ack",

        "crypto",
        "handshake_done",
        "connection_close",

        "path_challenge",
        "path_response",

        "stream",
        "reset_stream",
        "stop_sending",

        "new_connection_id",
        "retire_connection_id",

        "max_streams",
        "streams_blocked",

        "max_stream_data",
        "stream_data_blocked",

        "max_data",
        "data_blocked",

        "new_token",
    )

    for required_frame_type in required_frame_types:
        sent_count = frame_type_counts.get('transport:packet_sent', {}).get(required_frame_type, 0)
        if sent_count < 1:
            raise Unexpected(qlogdir, f"unexpectedly did not send any '{required_frame_type}' frames")

        received_count = frame_type_counts.get('transport:packet_received', {}).get(required_frame_type, 0)
        if received_count < 1:
            raise Unexpected(qlogdir, f"unexpectedly did not receive any '{required_frame_type}' frames")

    return 0

if __name__ == '__main__':
    sys.exit(run())
