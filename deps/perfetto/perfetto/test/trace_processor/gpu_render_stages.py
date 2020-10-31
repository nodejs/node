#!/usr/bin/python
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

from os import sys, path
sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

# Add a slice before any Specification packet is received.
trace.add_gpu_render_stages(
    ts=0, event_id=0, duration=5, hw_queue_id=1, stage_id=1, context=0)

trace.add_gpu_render_stages_stage_spec([{
    'name': 'stage 0'
}, {
    'name': 'stage 1',
    'description': 'stage desc 1'
}, {
    'name': 'stage 2'
}, {
    'name': 'stage 3'
}])

trace.add_gpu_render_stages_hw_queue_spec([{
    'name': 'queue 0',
    'description': 'queue desc 0'
}, {
    'name': 'queue 1',
    'description': 'queue desc 1'
}, {
    'name': 'queue 2',
}])

# Add some track to make sure Specification works properly
trace.add_gpu_render_stages(
    ts=0, event_id=0, duration=5, hw_queue_id=0, stage_id=0, context=42)
trace.add_gpu_render_stages(
    ts=10, event_id=1, duration=5, hw_queue_id=1, stage_id=1, context=42)
trace.add_gpu_render_stages(
    ts=20, event_id=2, duration=5, hw_queue_id=2, stage_id=2, context=42)
trace.add_gpu_render_stages(
    ts=30, event_id=3, duration=5, hw_queue_id=0, stage_id=3, context=42)
# Add a track without Specification for hw_queue_id/stage_id.
trace.add_gpu_render_stages(
    ts=40, event_id=3, duration=5, hw_queue_id=3, stage_id=4, context=42)

# Test extra_data
# event with single arg.
trace.add_gpu_render_stages(
    ts=50,
    event_id=5,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    extra_data={'key1': 'value1'})
# event with multiple args.
trace.add_gpu_render_stages(
    ts=60,
    event_id=6,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    extra_data={
        'key1': 'value1',
        'key2': 'value2'
    })
# event with args that only has key, but no value.
trace.add_gpu_render_stages(
    ts=70,
    event_id=7,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    extra_data={'key1': None})

##############################################
# Test stage naming with render target handle.

VK_OBJECT_TYPE_COMMAND_BUFFER = 6
VK_OBJECT_TYPE_RENDER_PASS = 18
VK_OBJECT_TYPE_FRAMEBUFFER = 24

trace.add_gpu_render_stages(
    ts=80, event_id=8, duration=5, hw_queue_id=0, stage_id=2, context=42)

# add render stage with render target handle without debug marker.
trace.add_gpu_render_stages(
    ts=90,
    event_id=9,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    render_target_handle=0x10,
    render_pass_handle=0x20,
    command_buffer_handle=0x30)

# adding a marker with COMMAND_BUFFER and RENDER_PASS should affect only the
# relevant handles.
trace.add_vk_debug_marker(
    ts=91,
    pid=100,
    vk_device=1,
    obj_type=VK_OBJECT_TYPE_COMMAND_BUFFER,
    obj=0x10,
    obj_name="command_buffer")
trace.add_gpu_render_stages(
    ts=100,
    event_id=10,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    render_target_handle=0x10,
    render_pass_handle=0x10,
    command_buffer_handle=0x10)

trace.add_vk_debug_marker(
    ts=101,
    pid=100,
    vk_device=1,
    obj_type=VK_OBJECT_TYPE_RENDER_PASS,
    obj=0x10,
    obj_name="render_pass")
trace.add_gpu_render_stages(
    ts=110,
    event_id=10,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    render_target_handle=0x10,
    render_pass_handle=0x10,
    command_buffer_handle=0x10)

# adding a marker with FRAMEBUFFER changes the name of the stage as well.
trace.add_vk_debug_marker(
    ts=111,
    pid=100,
    vk_device=1,
    obj_type=VK_OBJECT_TYPE_FRAMEBUFFER,
    obj=0x10,
    obj_name="framebuffer")
trace.add_gpu_render_stages(
    ts=120,
    event_id=10,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    render_target_handle=0x10,
    render_pass_handle=0x10,
    command_buffer_handle=0x10)

# setting the name again replace the name
trace.add_vk_debug_marker(
    ts=121,
    pid=100,
    vk_device=1,
    obj_type=VK_OBJECT_TYPE_FRAMEBUFFER,
    obj=0x10,
    obj_name="renamed_buffer")
trace.add_gpu_render_stages(
    ts=130,
    event_id=11,
    duration=5,
    hw_queue_id=0,
    stage_id=0,
    context=42,
    render_target_handle=0x10)

# Check that a hw_queue_id=-1 doesn't blow up.
trace.add_gpu_render_stages(
    ts=140, event_id=12, duration=5, hw_queue_id=-1, stage_id=-1, context=42)

print(trace.trace.SerializeToString())
