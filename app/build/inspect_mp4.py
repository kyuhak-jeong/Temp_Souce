import sys
import struct

def read_uint32(data, offset):
    return struct.unpack(">I", data[offset:offset+4])[0]

def inspect_fmp4(file_path):
    print(f"Inspecting fragmented MP4: {file_path}")
    try:
        with open(file_path, "rb") as f:
            data = f.read()
    except Exception as e:
        print(f"Error reading file: {e}")
        return

    # 1. Parse tracks in moov
    trak_indices = []
    offset = 0
    while True:
        idx = data.find(b'trak', offset)
        if idx == -1: break
        trak_indices.append(idx)
        offset = idx + 4

    track_count = len(trak_indices)
    print(f"Tracks defined in moov: {track_count}")
    
    # 2. Count samples in moof/trun
    # We will scan for 'traf' (track fragment) boxes.
    # Inside each 'traf', there is a 'tfhd' (track fragment header) which contains track_id (at offset 8 from 'tfhd' start)
    # and a 'trun' (track fragment run) which contains sample_count (at offset 16 from 'trun' start).
    track_samples = {}
    
    offset = 0
    while True:
        traf_idx = data.find(b'traf', offset)
        if traf_idx == -1: break
        
        # Search for tfhd inside this traf window
        window = data[traf_idx:traf_idx+200]
        tfhd_idx = window.find(b'tfhd')
        track_id = -1
        if tfhd_idx != -1:
            flags = struct.unpack(">I", b'\x00' + window[tfhd_idx+5:tfhd_idx+8])[0]
            # track_id is always at offset 8 (after 4 bytes size, 4 bytes type, 4 bytes version/flags)
            track_id = read_uint32(window, tfhd_idx + 8)
            
        # Search for trun inside this traf window
        trun_idx = window.find(b'trun')
        samples = 0
        if trun_idx != -1:
            flags = struct.unpack(">I", b'\x00' + window[trun_idx+5:trun_idx+8])[0]
            # sample_count is at offset 8 (after size, type, version/flags)
            samples = read_uint32(window, trun_idx + 8)
            
        if track_id != -1 and samples > 0:
            track_samples[track_id] = track_samples.get(track_id, 0) + samples
            
        offset = traf_idx + 4
        
    for tid, count in sorted(track_samples.items()):
        print(f"  Track ID {tid}: Total Samples (including fragments) = {count}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        inspect_fmp4(sys.argv[1])
    else:
        print("Usage: python3 inspect_mp4.py <mp4_file_path>")
