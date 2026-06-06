import struct
import sys

def parse_boxes_recursive(f, offset, end_offset, depth=0):
    boxes = []
    f.seek(offset)
    while f.tell() < end_offset:
        box_offset = f.tell()
        header = f.read(8)
        if len(header) < 8:
            break
        size, type_raw = struct.unpack(">I4s", header)
        try:
            box_type = type_raw.decode('latin1')
        except:
            box_type = "????"
        
        header_size = 8
        if size == 1:
            ext_size_raw = f.read(8)
            size = struct.unpack(">Q", ext_size_raw)[0]
            header_size = 16
        elif size == 0:
            f.seek(0, 2)
            size = f.tell() - box_offset
            
        data_offset = box_offset + header_size
        box_end = box_offset + size
        
        container_types = {'moov', 'trak', 'mdia', 'minf', 'stbl', 'moof', 'traf'}
        children = []
        if box_type in container_types:
            children = parse_boxes_recursive(f, data_offset, box_end, depth + 1)
            
        boxes.append({
            'type': box_type,
            'offset': box_offset,
            'data_offset': data_offset,
            'size': size,
            'end': box_end,
            'children': children
        })
        f.seek(box_end)
    return boxes

def find_boxes_by_type(boxes, type_name):
    res = []
    for b in boxes:
        if b['type'] == type_name:
            res.append(b)
        if b['children']:
            res.extend(find_boxes_by_type(b['children'], type_name))
    return res

def parse_tfhd(f, data_offset):
    f.seek(data_offset)
    version_flags = struct.unpack(">I", f.read(4))[0]
    version = version_flags >> 24
    flags = version_flags & 0xFFFFFF
    
    track_id = struct.unpack(">I", f.read(4))[0]
    
    base_data_offset = None
    if flags & 0x000001:
        base_data_offset = struct.unpack(">Q", f.read(8))[0]
        
    sample_desc_index = None
    if flags & 0x000002:
        sample_desc_index = struct.unpack(">I", f.read(4))[0]
        
    default_duration = None
    if flags & 0x000008:
        default_duration = struct.unpack(">I", f.read(4))[0]
        
    default_size = None
    if flags & 0x000010:
        default_size = struct.unpack(">I", f.read(4))[0]
        
    default_flags = None
    if flags & 0x000020:
        default_flags = struct.unpack(">I", f.read(4))[0]
        
    return {
        'track_id': track_id,
        'base_data_offset': base_data_offset,
        'default_duration': default_duration,
        'default_size': default_size,
        'default_flags': default_flags
    }

def parse_tfdt(f, data_offset):
    f.seek(data_offset)
    version_flags = struct.unpack(">I", f.read(4))[0]
    version = version_flags >> 24
    if version == 1:
        base_time = struct.unpack(">Q", f.read(8))[0]
    else:
        base_time = struct.unpack(">I", f.read(4))[0]
    return base_time

def parse_trun(f, data_offset, tfhd_info, moof_offset):
    f.seek(data_offset)
    version_flags = struct.unpack(">I", f.read(4))[0]
    version = version_flags >> 24
    flags = version_flags & 0xFFFFFF
    
    sample_count = struct.unpack(">I", f.read(4))[0]
    
    data_offset_val = None
    if flags & 0x000001:
        data_offset_val = struct.unpack(">i", f.read(4))[0]  # signed int
        
    first_sample_flags = None
    if flags & 0x000004:
        first_sample_flags = struct.unpack(">I", f.read(4))[0]
        
    # Calculate base data offset for sample data
    # If base_data_offset is present in tfhd, it's used.
    # Otherwise, if data_offset is present in trun, it's relative to the start of moof.
    # Otherwise, it's relative to the end of the moof box (first byte of next box, usually mdat).
    # GStreamer mp4mux: data_offset is relative to moof_offset!
    base_offset = tfhd_info.get('base_data_offset')
    if base_offset is None:
        base_offset = moof_offset
        
    if data_offset_val is not None:
        actual_data_offset = base_offset + data_offset_val
    else:
        actual_data_offset = base_offset # fallback
        
    samples = []
    curr_offset = actual_data_offset
    
    for _ in range(sample_count):
        duration = tfhd_info.get('default_duration', 0)
        if flags & 0x000100:
            duration = struct.unpack(">I", f.read(4))[0]
            
        size = tfhd_info.get('default_size', 0)
        if flags & 0x000200:
            size = struct.unpack(">I", f.read(4))[0]
            
        sample_flags = tfhd_info.get('default_flags', 0)
        if flags & 0x000400:
            sample_flags = struct.unpack(">I", f.read(4))[0]
            
        cto = 0
        if flags & 0x000800:
            if version == 1:
                cto = struct.unpack(">i", f.read(4))[0]
            else:
                cto = struct.unpack(">I", f.read(4))[0]
                
        samples.append({
            'duration': duration,
            'size': size,
            'flags': sample_flags,
            'cto': cto,
            'offset': curr_offset
        })
        curr_offset += size
        
    return samples

def inspect_fragmented_mp4(filepath):
    print(f"Analyzing Fragmented MP4: {filepath}")
    with open(filepath, "rb") as f:
        f.seek(0, 2)
        file_size = f.tell()
        
        print(f"Parsing boxes...")
        top_boxes = parse_boxes_recursive(f, 0, file_size)
        
        # 1. Find tracks in moov
        moov = next((b for b in top_boxes if b['type'] == 'moov'), None)
        if not moov:
            print("Error: No 'moov' box found.")
            return
            
        traks = find_boxes_by_type(moov['children'], 'trak')
        subtitle_track_ids = set()
        track_timescales = {}
        
        for idx, trak in enumerate(traks):
            hdlr = next((b for b in find_boxes_by_type(trak['children'], 'hdlr')), None)
            tkhd = next((b for b in find_boxes_by_type(trak['children'], 'tkhd')), None)
            mdhd = next((b for b in find_boxes_by_type(trak['children'], 'mdhd')), None)
            
            track_id = None
            if tkhd:
                f.seek(tkhd['data_offset'])
                version = struct.unpack("B", f.read(1))[0]
                f.read(3)
                if version == 1:
                    f.read(16) # creation/modification times
                    track_id = struct.unpack(">I", f.read(4))[0]
                else:
                    f.read(8)
                    track_id = struct.unpack(">I", f.read(4))[0]
                    
            timescale = 1000
            if mdhd:
                f.seek(mdhd['data_offset'])
                version = struct.unpack("B", f.read(1))[0]
                f.read(3)
                if version == 1:
                    f.read(16)
                    timescale = struct.unpack(">I", f.read(4))[0]
                else:
                    f.read(8)
                    timescale = struct.unpack(">I", f.read(4))[0]
            
            if track_id is not None:
                track_timescales[track_id] = timescale
                
            handler_type = None
            if hdlr:
                f.seek(hdlr['data_offset'] + 8)
                handler_type = f.read(4).decode('latin1')
                
            print(f"Track Index {idx} | ID {track_id} | Type '{handler_type}' | Timescale {timescale}")
            if handler_type in ('subt', 'text', 'sbtl'):
                if track_id is not None:
                    subtitle_track_ids.add(track_id)
                    
        print(f"Subtitle Track IDs: {subtitle_track_ids}")
        if not subtitle_track_ids:
            print("No subtitle tracks found.")
            return
            
        # 2. Iterate through moof boxes and extract samples for all tracks
        moofs = [b for b in top_boxes if b['type'] == 'moof']
        print(f"Found {len(moofs)} moof boxes")
        
        all_sub_samples = []
        track_sample_counts = {}
        
        for moof_idx, moof in enumerate(moofs):
            trafs = find_boxes_by_type(moof['children'], 'traf')
            for traf in trafs:
                tfhd_box = next((b for b in traf['children'] if b['type'] == 'tfhd'), None)
                tfdt_box = next((b for b in traf['children'] if b['type'] == 'tfdt'), None)
                trun_box = next((b for b in traf['children'] if b['type'] == 'trun'), None)
                
                if not tfhd_box or not trun_box:
                    continue
                    
                tfhd_info = parse_tfhd(f, tfhd_box['data_offset'])
                track_id = tfhd_info['track_id']
                
                # Parse base media decode time (base PTS)
                base_time = 0
                if tfdt_box:
                    base_time = parse_tfdt(f, tfdt_box['data_offset'])
                    
                samples = parse_trun(f, trun_box['data_offset'], tfhd_info, moof['offset'])
                
                if track_id not in track_sample_counts:
                    track_sample_counts[track_id] = 0
                    
                timescale = track_timescales.get(track_id, 1000)
                
                curr_time = base_time
                for s in samples:
                    pts = curr_time
                    duration = s['duration']
                    
                    if track_id in subtitle_track_ids:
                        # Read sample data
                        f.seek(s['offset'])
                        data = f.read(s['size'])
                        
                        # Decode text
                        text = ""
                        if len(data) >= 2:
                            text_len = struct.unpack(">H", data[:2])[0]
                            if text_len <= len(data) - 2:
                                text = data[2:2+text_len].decode('utf-8', errors='replace').strip()
                            else:
                                text = data.decode('utf-8', errors='replace').strip()
                        else:
                            text = data.decode('utf-8', errors='replace').strip()
                            
                        clean_text = ''.join(c for c in text if c.isprintable() or c in '\n\r\t')
                        
                        pts_sec = pts / timescale
                        dur_sec = duration / timescale
                        
                        all_sub_samples.append({
                            'moof_idx': moof_idx,
                            'pts': pts_sec,
                            'duration': dur_sec,
                            'text': clean_text,
                            'size': s['size'],
                            'offset': s['offset']
                        })
                    else:
                        if track_sample_counts[track_id] < 5:
                            pts_sec = pts / timescale
                            print(f"Track {track_id} sample {track_sample_counts[track_id]} | PTS: {pts_sec:.3f}s | Duration: {duration/timescale:.3f}s")
                            
                    track_sample_counts[track_id] += 1
                    curr_time += duration
                    
        print(f"\n--- Fragmented Subtitle Track Dump ({len(all_sub_samples)} samples) ---")
        for i, s in enumerate(all_sub_samples):
            print(f"Sample {i:03d} | Moof {s['moof_idx']:02d} | PTS: {s['pts']:.3f}s | Duration: {s['duration']:.3f}s | Size: {s['size']} | Text: {s['text']}")

if __name__ == "__main__":
    filepath = "E_test.mp4"
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    inspect_fragmented_mp4(filepath)
