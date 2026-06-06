import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
Gst.init(None)
p = Gst.parse_launch("appsrc name=mysrc caps=text/x-raw,format=utf8 ! mp4mux ! fakesink")
src = p.get_by_name("mysrc")
p.set_state(Gst.State.PLAYING)
b = Gst.Buffer.new_allocate(None, 5, None)
b.fill(0, b"hello")
src.emit("push-buffer", b)
src.emit("end-of-stream")
bus = p.get_bus()
bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)
print("done")
