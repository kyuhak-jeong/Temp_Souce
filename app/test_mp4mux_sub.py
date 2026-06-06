import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

Gst.init(None)

pipeline_str = """
appsrc name=mysrc is-live=true format=time caps=text/x-raw,format=utf8 ! queue ! mp4mux name=mux ! filesink location=test_sub.mp4
videotestsrc num-buffers=30 ! video/x-raw,width=320,height=240,framerate=10/1 ! x264enc ! mux.
"""
pipeline = Gst.parse_launch(pipeline_str)
src = pipeline.get_by_name('mysrc')

def push_data():
    buf = Gst.Buffer.new_allocate(None, 12, None)
    buf.fill(0, b"Hello World\n")
    buf.pts = 0
    buf.duration = 2 * Gst.SECOND
    src.emit('push-buffer', buf)
    
    buf2 = Gst.Buffer.new_allocate(None, 10, None)
    buf2.fill(0, b"Subtitle 2")
    buf2.pts = 2 * Gst.SECOND
    buf2.duration = 1 * Gst.SECOND
    src.emit('push-buffer', buf2)
    src.emit('end-of-stream')

pipeline.set_state(Gst.State.PLAYING)
push_data()

bus = pipeline.get_bus()
msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)
if msg.type == Gst.MessageType.ERROR:
    err, debug = msg.parse_error()
    print(f"Error: {err}, {debug}")
else:
    print("Success")

pipeline.set_state(Gst.State.NULL)
