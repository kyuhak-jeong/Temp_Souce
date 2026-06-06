import struct
import sys

def parse_boxes(f, offset, end_offset):
    boxes = []
    f.seek(offset)
    while f.tell() < end_offset:
        box_offset = f.tell()
        header = f.read(8)
        if len(header) < 8:
            break
        size, type_raw = struct.unpack(">I4s", header)
        box_type = type_raw.decode('latin1')
        
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
        boxes.append((box_type, box_offset, data_offset, size, box_end))
        f.seek(box_end)
    return boxes

def find_track_info(f, track_data_offset, track_end):
    info = {'type': None, 'stts': [], 'stsz': [], 'stco': [], 'stsc': []}
    
    trak_boxes = parse_boxes(f, track_data_offset, track_end)
    print(f"DEBUG trak_boxes: {[b[0] for b in trak_boxes]}")
    mdia_box = next((b for b in trak_boxes if b[0] == 'mdia'), None)
    if not mdia_box:
        print("DEBUG: mdia_box not found in trak_boxes!")
        return None
        
    mdia_sub = parse_boxes(f, mdia_box[2], mdia_box[4])
    print(f"DEBUG mdia_sub: {[b[0] for b in mdia_sub]}")
    hdlr_box = next((b for b in mdia_sub if b[0] == 'hdlr'), None)
    if hdlr_box:
        f.seek(hdlr_box[2] + 8)
        handler_type = f.read(4).decode('latin1')
        info['type'] = handler_type
        
    minf_box = next((b for b in mdia_sub if b[0] == 'minf'), None)
    if minf_box:
        minf_sub = parse_boxes(f, minf_box[2], minf_box[4])
        print(f"DEBUG minf_sub: {[b[0] for b in minf_sub]}")
        stbl_box = next((b for b in minf_sub if b[0] == 'stbl'), None)
        if stbl_box:
            stbl_sub = parse_boxes(f, stbl_box[2], stbl_box[4])
            print(f"DEBUG stbl_sub: {[b[0] for b in stbl_sub]}")
            
            stts_box = next((b for b in stbl_sub if b[0] == 'stts'), None)
            if stts_box:
                f.seek(stts_box[2])
                version_flags = struct.unpack(">I", f.read(4))[0]
                num_entries = struct.unpack(">I", f.read(4))[0]
                for _ in range(num_entries):
                    count, delta = struct.unpack(">II", f.read(8))
                    info['stts'].append((count, delta))
                    
            stsz_box = next((b for b in stbl_sub if b[0] == 'stsz'), None)
            if stsz_box:
                f.seek(stsz_box[2])
                raw_data = f.read(12)
                version_flags, sample_size, num_entries = struct.unpack(">III", raw_data)
                print(f"DEBUG stsz box: offset={stsz_box[2]}, version_flags={version_flags}, sample_size={sample_size}, num_entries={num_entries}")
                if sample_size > 0:
                    info['stsz'] = [sample_size] * num_entries
                else:
                    for _ in range(num_entries):
                        info['stsz'].append(struct.unpack(">I", f.read(4))[0])
                        
            stco_box = next((b for b in stbl_sub if b[0] == 'stco'), None)
            if stco_box:
                f.seek(stco_box[2])
                version_flags = struct.unpack(">I", f.read(4))[0]
                num_entries = struct.unpack(">I", f.read(4))[0]
                for _ in range(num_entries):
                    info['stco'].append(struct.unpack(">I", f.read(4))[0])
            else:
                co64_box = next((b for b in stbl_sub if b[0] == 'co64'), None)
                if co64_box:
                    f.seek(co64_box[2])
                    version_flags = struct.unpack(">I", f.read(4))[0]
                    num_entries = struct.unpack(">I", f.read(4))[0]
                    for _ in range(num_entries):
                        info['stco'].append(struct.unpack(">Q", f.read(8))[0])
                        
            stsc_box = next((b for b in stbl_sub if b[0] == 'stsc'), None)
            if stsc_box:
                f.seek(stsc_box[2])
                version_flags = struct.unpack(">I", f.read(4))[0]
                num_entries = struct.unpack(">I", f.read(4))[0]
                for _ in range(num_entries):
                    first, count, desc_idx = struct.unpack(">III", f.read(12))
                    info['stsc'].append((first, count, desc_idx))
                    
            mdhd_box = next((b for b in mdia_sub if b[0] == 'mdhd'), None)
            if mdhd_box:
                f.seek(mdhd_box[2])
                version = struct.unpack("B", f.read(1))[0]
                f.read(3)
                if version == 1:
                    f.read(16)
                    timescale = struct.unpack(">I", f.read(4))[0]
                else:
                    f.read(8)
                    timescale = struct.unpack(">I", f.read(4))[0]
                info['timescale'] = timescale
                
    return info

def inspect_mp4(filepath):
    print(f"Opening MP4 file: {filepath}")
    with open(filepath, "rb") as f:
        f.seek(0, 2)
        file_size = f.tell()
        
        top_boxes = parse_boxes(f, 0, file_size)
        print(f"DEBUG top_boxes: {[b[0] for b in top_boxes]}")
        moov_box = next((b for b in top_boxes if b[0] == 'moov'), None)
        if not moov_box:
            print("Error: 'moov' box not found")
            return
            
        moov_sub = parse_boxes(f, moov_box[2], moov_box[4])
        trak_boxes = [b for b in moov_sub if b[0] == 'trak']
        
        print(f"Found {len(trak_boxes)} tracks")
        
        for idx, trak in enumerate(trak_boxes):
            info = find_track_info(f, trak[2], trak[4])
            track_type = info['type'] if info else None
            if info:
                print(f"Track {idx} type: {track_type} | stsz count: {len(info.get('stsz', []))} | stco count: {len(info.get('stco', []))} | stts count: {len(info.get('stts', []))}")
            else:
                print(f"Track {idx} type: {track_type} | No info")
            if not info or info['type'] not in ('subt', 'text', 'sbtl', 'clcp'):
                continue
                
            print(f"\n--- Subtitle Track found (Index: {idx}, Type: '{info['type']}') ---")
            timescale = info.get('timescale', 1000)
            print(f"Timescale: {timescale}")
            
            stsc = info['stsc']
            stco = info['stco']
            stsz = info['stsz']
            stts = info['stts']
            print(f"DEBUG: len(stsc)={len(stsc)}, len(stco)={len(stco)}, len(stsz)={len(stsz)}, len(stts)={len(stts)}")
            print(f"DEBUG: stsc={stsc}")
            print(f"DEBUG: stco={stco}")
            print(f"DEBUG: stts={stts}")
            
            sample_offsets = []
            sample_idx = 0
            
            for chunk_idx, chunk_offset in enumerate(stco):
                samples_in_chunk = 0
                for entry in reversed(stsc):
                    if chunk_idx + 1 >= entry[0]:
                        samples_in_chunk = entry[1]
                        break
                        
                curr_offset = chunk_offset
                for _ in range(samples_in_chunk):
                    if sample_idx >= len(stsz):
                        break
                    sz = stsz[sample_idx]
                    sample_offsets.append((curr_offset, sz))
                    curr_offset += sz
                    sample_idx += 1
                    
            print(f"Total subtitle samples: {len(sample_offsets)}")
            
            sample_times = []
            curr_time = 0
            for entry in stts:
                count, delta = entry
                for _ in range(count):
                    sample_times.append((curr_time, delta))
                    curr_time += delta
                    
            for i, (offset, size) in enumerate(sample_offsets):
                if i >= len(sample_times):
                    break
                pts, duration = sample_times[i]
                pts_sec = pts / timescale
                dur_sec = duration / timescale
                
                f.seek(offset)
                data = f.read(size)
                
                text = ""
                if len(data) >= 2:
                    text_len = struct.unpack(">H", data[:2])[0]
                    if text_len <= len(data) - 2:
                        text = data[2:2+text_len].decode('utf-8', errors='replace').strip()
                    else:
                        text = data.decode('utf-8', errors='replace').strip()
                else:
                    text = data.decode('utf-8', errors='replace').strip()
                
                # Filter out raw formatting codes or printable-only characters
                clean_text = ''.join(c for c in text if c.isprintable() or c in '\n\r\t')
                print(f"Sample {i:03d} | PTS: {pts_sec:.3f}s (PTS raw: {pts}) | Duration: {dur_sec:.3f}s | Text: {clean_text}")

if __name__ == "__main__":
    filepath = "E_test.mp4"
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    inspect_mp4(filepath)
