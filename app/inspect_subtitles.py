import sys
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

Gst.init(None)

class SubtitleInspector:
    def __init__(self, filepath):
        self.filepath = filepath
        self.loop = GLib.MainLoop()
        
        # Create elements
        self.pipeline = Gst.Pipeline.new("sub-pipeline")
        self.src = Gst.ElementFactory.make("filesrc", "src")
        self.src.set_property("location", self.filepath)
        self.demux = Gst.ElementFactory.make("qtdemux", "demux")
        
        self.pipeline.add(self.src)
        self.pipeline.add(self.demux)
        self.src.link(self.demux)
        
        # Connect pad-added signal
        self.demux.connect("pad-added", self.on_pad_added)
        
        # Monitor bus
        self.bus = self.pipeline.get_bus()
        self.bus.add_signal_watch()
        self.bus.connect("message", self.on_message)
        
        self.subtitles = []

    def on_pad_added(self, element, pad):
        caps = pad.get_current_caps()
        if not caps:
            caps = pad.query_caps(None)
        structure = caps.get_structure(0)
        name = structure.get_name()
        
        # Create a fakesink/appsink for the pad
        sink = Gst.ElementFactory.make("fakesink", f"sink_{pad.get_name()}")
        self.pipeline.add(sink)
        sink.sync_state_with_parent()
        pad.link(sink.get_static_pad("sink"))
        
        if name.startswith("text/"):
            # It's a subtitle pad, enable dump or signal handoff
            sink.set_property("signal-handoffs", True)
            sink.connect("handoff", self.on_handoff)
            print(f"Linked subtitle pad: {pad.get_name()} ({name})")
        else:
            # Discard video/audio
            pass

    def on_handoff(self, element, buf, pad):
        pts = buf.pts
        duration = buf.duration
        
        # Extract data
        (result, map_info) = buf.map(Gst.MapFlags.READ)
        if result:
            try:
                text = map_info.data.decode('utf-8', errors='replace').strip()
            except Exception as e:
                text = f"<decode error: {e}>"
            buf.unmap(map_info)
        else:
            text = "<failed to map buffer>"
            
        pts_sec = pts / Gst.SECOND if pts != Gst.CLOCK_TIME_NONE else -1
        dur_sec = duration / Gst.SECOND if duration != Gst.CLOCK_TIME_NONE else -1
        self.subtitles.append((pts_sec, dur_sec, text))

    def on_message(self, bus, message):
        t = message.type
        if t == Gst.MessageType.EOS:
            self.loop.quit()
        elif t == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"Error: {err} | {debug}")
            self.loop.quit()

    def run(self):
        self.pipeline.set_state(Gst.State.PLAYING)
        try:
            self.loop.run()
        finally:
            self.pipeline.set_state(Gst.State.NULL)
            
        # Print results
        print("\n--- Subtitle Track Dump ---")
        for i, (pts, dur, text) in enumerate(self.subtitles):
            print(f"[{i:03d}] PTS: {pts:.3f}s | Duration: {dur:.3f}s | Text: {text}")

if __name__ == "__main__":
    filepath = "E_test.mp4"
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    inspector = SubtitleInspector(filepath)
    inspector.run()
