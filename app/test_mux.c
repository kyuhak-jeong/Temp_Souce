#include <gst/gst.h>
#include <string.h>
#include <stdio.h>

void run_test(const char* filename, GstClockTime duration_val)
{
    printf("Running test for %s with duration_val = %lld\n", filename, (long long)duration_val);
    
    // We create a pipeline with a video generator (videotestsrc) and a subtitle generator (appsrc)
    // muxed together via mp4mux.
    char pipeline_str[512];
    snprintf(pipeline_str, sizeof(pipeline_str),
             "mp4mux name=mux ! filesink location=%s "
             "videotestsrc num-buffers=90 ! video/x-raw,width=320,height=240,framerate=30/1 ! x264enc ! mux.video_0 "
             "appsrc name=subsrc is-live=true format=time caps=\"text/x-raw,format=utf8\" ! queue ! mux.subtitle_0",
             filename);
             
    GstElement *pipeline = gst_parse_launch(pipeline_str, NULL);
    if (!pipeline) {
        printf("Failed to create pipeline\n");
        return;
    }
    
    GstElement *src = gst_bin_get_by_name(GST_BIN(pipeline), "subsrc");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    // Push 3 subtitles at 0.0s, 1.0s, 2.0s
    for (int i = 0; i < 3; i++)
    {
        g_usleep(100000); // Sleep 100ms to let pipeline spin up
        
        char text[64];
        snprintf(text, sizeof(text), "Subtitle %d at %d.0s", i, i);
        size_t len = strlen(text);
        
        GstBuffer *buf = gst_buffer_new_allocate(NULL, len, NULL);
        GstMapInfo map;
        gst_buffer_map(buf, &map, GST_MAP_WRITE);
        memcpy(map.data, text, len);
        gst_buffer_unmap(buf, &map);
        
        GST_BUFFER_PTS(buf) = i * GST_SECOND;
        GST_BUFFER_DTS(buf) = i * GST_SECOND;
        GST_BUFFER_DURATION(buf) = duration_val;
        
        printf("Pushing subtitle: %s, PTS = %d.0s, duration = %lld\n", text, i, (long long)duration_val);
        g_signal_emit_by_name(src, "push-buffer", buf, NULL);
        gst_buffer_unref(buf);
    }
    
    g_signal_emit_by_name(src, "end-of-stream", NULL);
    
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(src);
    gst_object_unref(pipeline);
    printf("Finished test for %s\n\n", filename);
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    
    // Test 1: with fixed 1-second duration
    run_test("test_duration_1s.mp4", GST_SECOND);
    
    // Test 2: with GST_CLOCK_TIME_NONE duration
    run_test("test_duration_none.mp4", GST_CLOCK_TIME_NONE);
    
    return 0;
}
