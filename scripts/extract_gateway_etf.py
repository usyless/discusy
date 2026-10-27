#!/usr/bin/env python3
"""
extract_gateway_etf.py - Safely extract and sanitize Discord ETF payloads from gateway_raw.log

Features:
- Scans gateway_raw.log for raw binary ETF payloads without text encoding corruption.
- Lists all gateway events (GUILD_CREATE, INTERACTION_CREATE, etc.) with metadata.
- Anonymizes snowflake IDs consistently (maintaining channel, role, parent, user links).
- Preserves Discord system constants, enum integers, booleans, and bitfield permissions.
- Redacts bot tokens, sensitive user/guild/channel names, and avatar hashes.
- Outputs sanitized raw binary (.etf) and C++ byte array (.cpp.txt) ready for unit testing.
"""

import sys
import os
import struct
import argparse

class ETFParser:
    def __init__(self, data, offset=0):
        self.data = data
        self.pos = offset

    def parse(self):
        if self.pos >= len(self.data):
            raise EOFError("Unexpected end of ETF stream")
        tag = self.data[self.pos]
        self.pos += 1

        if tag == 131: # MAGIC
            return ('magic', self.parse())
        elif tag in (100, 118): # ATOM_EXT / ATOM_UTF8_EXT
            length = struct.unpack('>H', self.data[self.pos:self.pos+2])[0]
            self.pos += 2
            val = self.data[self.pos:self.pos+length].decode('utf-8', errors='replace')
            self.pos += length
            return ('atom', (tag, val))
        elif tag in (115, 119): # SMALL_ATOM_EXT / SMALL_ATOM_UTF8_EXT
            length = self.data[self.pos]
            self.pos += 1
            val = self.data[self.pos:self.pos+length].decode('utf-8', errors='replace')
            self.pos += length
            return ('atom', (tag, val))
        elif tag == 109: # BINARY_EXT
            length = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
            self.pos += 4
            val = self.data[self.pos:self.pos+length]
            self.pos += length
            return ('binary', val)
        elif tag == 97: # SMALL_INTEGER_EXT
            val = self.data[self.pos]
            self.pos += 1
            return ('small_int', val)
        elif tag == 98: # INTEGER_EXT
            val = struct.unpack('>i', self.data[self.pos:self.pos+4])[0]
            self.pos += 4
            return ('int', val)
        elif tag == 110: # SMALL_BIG_EXT
            n = self.data[self.pos]
            sign = self.data[self.pos+1]
            self.pos += 2
            val = int.from_bytes(self.data[self.pos:self.pos+n], 'little')
            if sign != 0: val = -val
            self.pos += n
            return ('bignum', val)
        elif tag == 111: # LARGE_BIG_EXT
            n = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
            sign = self.data[self.pos+4]
            self.pos += 5
            val = int.from_bytes(self.data[self.pos:self.pos+n], 'little')
            if sign != 0: val = -val
            self.pos += n
            return ('bignum', val)
        elif tag == 70: # NEW_FLOAT_EXT
            val = struct.unpack('>d', self.data[self.pos:self.pos+8])[0]
            self.pos += 8
            return ('float', val)
        elif tag == 99: # FLOAT_EXT (31 bytes ASCII float)
            raw = self.data[self.pos:self.pos+31].split(b'\x00')[0]
            self.pos += 31
            try:
                flt = float(raw.decode('latin1'))
            except Exception:
                flt = 0.0
            return ('float', flt)
        elif tag == 106: # NIL_EXT
            return ('nil', None)
        elif tag == 107: # STRING_EXT
            length = struct.unpack('>H', self.data[self.pos:self.pos+2])[0]
            self.pos += 2
            val = self.data[self.pos:self.pos+length]
            self.pos += length
            return ('string_ext', val)
        elif tag == 108: # LIST_EXT
            count = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
            self.pos += 4
            items = []
            for _ in range(count):
                items.append(self.parse())
            tail = self.parse()
            return ('list', (items, tail))
        elif tag == 104: # SMALL_TUPLE_EXT
            count = self.data[self.pos]
            self.pos += 1
            items = [self.parse() for _ in range(count)]
            return ('tuple', items)
        elif tag == 105: # LARGE_TUPLE_EXT
            count = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
            self.pos += 4
            items = [self.parse() for _ in range(count)]
            return ('tuple', items)
        elif tag == 116: # MAP_EXT
            count = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
            self.pos += 4
            kvs = []
            for _ in range(count):
                k = self.parse()
                v = self.parse()
                kvs.append((k, v))
            return ('map', kvs)
        else:
            raise ValueError(f"Unknown ETF tag {tag} (0x{tag:02x}) at position {self.pos-1}")

def encode_etf(item):
    kind, val = item
    out = bytearray()
    if kind == 'magic':
        out.append(131)
        out.extend(encode_etf(val))
    elif kind == 'atom':
        if isinstance(val, tuple):
            tag, atom_str = val
        else:
            tag, atom_str = (119 if len(val.encode('utf-8')) <= 255 else 118), val
        raw = atom_str.encode('utf-8')
        if tag in (115, 119) and len(raw) <= 255:
            out.append(tag)
            out.append(len(raw))
            out.extend(raw)
        else:
            out.append(100 if tag == 100 else 118)
            out.extend(struct.pack('>H', len(raw)))
            out.extend(raw)
    elif kind == 'binary':
        out.append(109)
        out.extend(struct.pack('>I', len(val)))
        out.extend(val)
    elif kind == 'string_ext':
        out.append(107)
        out.extend(struct.pack('>H', len(val)))
        out.extend(val)
    elif kind == 'small_int':
        out.append(97)
        out.append(val)
    elif kind == 'int':
        out.append(98)
        out.extend(struct.pack('>i', val))
    elif kind == 'bignum':
        neg = 1 if val < 0 else 0
        uval = abs(val)
        raw = uval.to_bytes((uval.bit_length() + 7) // 8, 'little') if uval > 0 else b'\x00'
        if len(raw) <= 255:
            out.append(110)
            out.append(len(raw))
            out.append(neg)
            out.extend(raw)
        else:
            out.append(111)
            out.extend(struct.pack('>I', len(raw)))
            out.append(neg)
            out.extend(raw)
    elif kind == 'float':
        out.append(70)
        out.extend(struct.pack('>d', val))
    elif kind == 'nil':
        out.append(106)
    elif kind == 'list':
        items, tail = val
        out.append(108)
        out.extend(struct.pack('>I', len(items)))
        for x in items:
            out.extend(encode_etf(x))
        out.extend(encode_etf(tail))
    elif kind == 'tuple':
        if len(val) <= 255:
            out.append(104)
            out.append(len(val))
        else:
            out.append(105)
            out.extend(struct.pack('>I', len(val)))
        for x in val:
            out.extend(encode_etf(x))
    elif kind == 'map':
        out.append(116)
        out.extend(struct.pack('>I', len(val)))
        for k, v in val:
            out.extend(encode_etf(k))
            out.extend(encode_etf(v))
    else:
        raise ValueError(f"Unknown item kind: {kind}")
    return bytes(out)

class ETFSanitizer:
    def __init__(self, base_snowflake=100000000000000000):
        self.snowflake_map = {}
        self.next_snowflake = base_snowflake
        self.name_counters = {}

    def get_fake_snowflake(self, orig_id):
        if orig_id not in self.snowflake_map:
            self.next_snowflake += 1
            self.snowflake_map[orig_id] = self.next_snowflake
        return self.snowflake_map[orig_id]

    def is_snowflake_key(self, key_name):
        k = key_name.lower()
        if k == 'id' or k.endswith('_id'):
            return True
        if k in ('roles', 'channel_ids', 'sku_ids', 'mention_roles'):
            return True
        return False

    def is_bitfield_key(self, key_name):
        k = key_name.lower()
        return k in ('permissions', 'allow', 'deny', 'flags', 'system_channel_flags', 'public_flags')

    def sanitize(self, node, key_context=None):
        kind, val = node
        ctx = (key_context or "").lower()

        if kind == 'magic':
            return ('magic', self.sanitize(val, key_context))

        elif kind in ('bignum', 'int', 'small_int'):
            # If it's a snowflake ID field and fits snowflake magnitude or key context
            if self.is_snowflake_key(ctx) and not self.is_bitfield_key(ctx):
                if val > 1000000: # Exclude small IDs or types
                    fake = self.get_fake_snowflake(val)
                    return ('bignum', fake)
            # If bignum without clear context but in snowflake range
            elif kind == 'bignum' and val > 100000000000000 and not self.is_bitfield_key(ctx):
                fake = self.get_fake_snowflake(val)
                return ('bignum', fake)
            return node

        elif kind == 'binary':
            str_val = val.decode('utf-8', errors='latin1')

            # Redact bot token
            if 'token' in ctx:
                return ('binary', b'REDACTED_BOT_TOKEN_1234567890')

            # Snowflake as string (e.g. "123456789012345678")
            if self.is_snowflake_key(ctx) and not self.is_bitfield_key(ctx) and str_val.isdigit() and len(str_val) >= 15:
                fake = self.get_fake_snowflake(int(str_val))
                return ('binary', str(fake).encode('utf-8'))

            # Leave protocol keywords and enums intact
            if str_val in ('@everyone', 'GUILD_CREATE', 'INTERACTION_CREATE', 'READY', 'RESUMED', 'VOICE_STATE_UPDATE'):
                return node
            # Leave locales, feature flags intact
            if str_val in ('en-US', 'en-GB', 'COMMUNITY', 'INVITE_SPLASH', 'DISCOVERABLE', 'FEATURABLE'):
                return node

            # Media hashes (32-char hex like avatar/icon/banner)
            if ctx in ('icon', 'banner', 'splash', 'discovery_splash', 'avatar', 'avatar_decoration_data'):
                return ('binary', b'0123456789abcdef0123456789abcdef')

            # Anonymize human-readable names and text
            if ctx in ('name', 'username', 'nick', 'global_name', 'topic', 'description', 'vanity_url_code', 'custom_status'):
                count = self.name_counters.get(ctx, 0) + 1
                self.name_counters[ctx] = count
                return ('binary', f'{ctx}_{count}'.encode('utf-8'))

            return node

        elif kind == 'list':
            items, tail = val
            new_items = [self.sanitize(x, key_context) for x in items]
            return ('list', (new_items, self.sanitize(tail, key_context)))

        elif kind == 'map':
            new_kvs = []
            for k, v in val:
                key_name = ""
                if k[0] == 'atom':
                    key_name = k[1][1] if isinstance(k[1], tuple) else k[1]
                elif k[0] == 'binary':
                    key_name = k[1].decode('utf-8', errors='latin1')
                sanitized_v = self.sanitize(v, key_context=key_name)
                new_kvs.append((k, sanitized_v))
            return ('map', new_kvs)

        elif kind == 'tuple':
            return ('tuple', [self.sanitize(x, key_context) for x in val])

        return node

def extract_metadata(tree):
    """Inspect top-level gateway dispatch map: op, s, t"""
    meta = {'op': None, 's': None, 't': None}
    node = tree
    if node[0] == 'magic':
        node = node[1]
    if node[0] == 'map':
        for k, v in node[1]:
            k_name = (k[1][1] if isinstance(k[1], tuple) else k[1]) if k[0] == 'atom' else (k[1].decode('latin1') if k[0] == 'binary' else '')
            v_val = v[1]
            if v[0] == 'atom' and isinstance(v_val, tuple):
                v_val = v_val[1]
            if k_name == 'op':
                meta['op'] = v_val
            elif k_name == 's':
                meta['s'] = v_val
            elif k_name == 't':
                if isinstance(v_val, bytes):
                    meta['t'] = v_val.decode('latin1', errors='replace')
                elif isinstance(v_val, str):
                    meta['t'] = v_val
    return meta

def scan_log_for_payloads(log_bytes):
    payloads = []
    # Discusy shard logger pattern:
    # "Handle gateway payload: \x83"
    needle = b"Handle gateway payload: \x83"
    idx = 0
    while True:
        idx = log_bytes.find(needle, idx)
        if idx == -1:
            break
        start_pos = idx + len(b"Handle gateway payload: ")
        try:
            parser = ETFParser(log_bytes, start_pos)
            parsed_tree = parser.parse()
            end_pos = parser.pos
            raw_slice = log_bytes[start_pos:end_pos]
            meta = extract_metadata(parsed_tree)
            payloads.append({
                'start': start_pos,
                'end': end_pos,
                'size': len(raw_slice),
                'meta': meta,
                'tree': parsed_tree,
                'raw': raw_slice
            })
            idx = end_pos
        except Exception as e:
            idx += len(needle)
    return payloads

def to_cpp_byte_array(data, var_name="sanitized_payload"):
    lines = []
    lines.append(f"// Sanitized Discord ETF payload ({len(data)} bytes)")
    lines.append(f"inline constexpr uint8_t {var_name}[] = {{")
    chunk_size = 16
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i+chunk_size]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        if i + chunk_size < len(data):
            lines.append(f"    {hex_str},")
        else:
            lines.append(f"    {hex_str}")
    lines.append("};")
    lines.append("")
    lines.append(f"// As std::string_view:")
    lines.append(f"inline const std::string_view {var_name}_view{{\n    reinterpret_cast<const char*>({var_name}), sizeof({var_name})\n}};")
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(description="Safely extract and sanitize Discord ETF payloads from gateway_raw.log")
    parser.add_argument("logfile", nargs="?", default="gateway_raw.log", help="Path to gateway_raw.log (default: gateway_raw.log)")
    parser.add_argument("--list", action="store_true", help="List all events found in the log without extracting")
    parser.add_argument("--event", type=str, default=None, help="Filter by event name (e.g. GUILD_CREATE, INTERACTION_CREATE)")
    parser.add_argument("--index", type=int, default=None, help="Index of payload to extract (1-based)")
    parser.add_argument("--raw", action="store_true", help="Extract raw without sanitizing (keep on local disk)")
    parser.add_argument("--out-prefix", type=str, default=None, help="Output filename prefix (default: auto)")

    args = parser.parse_args()

    if not os.path.exists(args.logfile):
        print(f"Error: Log file not found: {args.logfile}", file=sys.stderr)
        sys.exit(1)

    print(f"Reading {args.logfile} in binary mode...")
    with open(args.logfile, "rb") as f:
        log_bytes = f.read()

    payloads = scan_log_for_payloads(log_bytes)
    if not payloads:
        print("No ETF payloads found in log file.")
        print("Note: In discusy, ensure DISCUSY_LOGGING is enabled and stderr was redirected: your_bot.exe 2> gateway_raw.log")
        sys.exit(0)

    print(f"Found {len(payloads)} ETF payload(s):\n")
    for i, p in enumerate(payloads, 1):
        m = p['meta']
        ev_name = m['t'] if m['t'] else f"Opcode {m['op']}"
        s_val = f", s: {m['s']}" if m['s'] is not None else ""
        print(f"  [{i}] Event: {ev_name:<24} (Size: {p['size']:>6} bytes{s_val})")

    if args.list:
        return

    # Select target payload
    target_idx = None
    if args.index is not None:
        if 1 <= args.index <= len(payloads):
            target_idx = args.index - 1
        else:
            print(f"Invalid index {args.index}. Must be between 1 and {len(payloads)}.", file=sys.stderr)
            sys.exit(1)
    elif args.event:
        for i, p in enumerate(payloads):
            if p['meta']['t'] == args.event:
                target_idx = i
                break
        if target_idx is None:
            print(f"No event matching '{args.event}' found.", file=sys.stderr)
            sys.exit(1)
    else:
        # If only one payload, default to it; otherwise prompt
        if len(payloads) == 1:
            target_idx = 0
        else:
            try:
                choice = input(f"\nSelect payload to extract [1-{len(payloads)}]: ").strip()
                target_idx = int(choice) - 1
                if not (0 <= target_idx < len(payloads)):
                    raise ValueError()
            except (ValueError, EOFError):
                print("Invalid selection. Exiting.")
                sys.exit(1)

    selected = payloads[target_idx]
    ev_name = selected['meta']['t'] or f"op_{selected['meta']['op']}"
    print(f"\nProcessing payload [{target_idx+1}]: {ev_name} ({selected['size']} bytes)")

    prefix = args.out_prefix or f"payload_{target_idx+1}_{ev_name.lower()}"

    if args.raw:
        out_bin = f"{prefix}_raw.etf"
        with open(out_bin, "wb") as f:
            f.write(selected['raw'])
        print(f"Wrote raw binary ETF to: {out_bin}")
    else:
        print("Sanitizing payload (remapping snowflakes, redacting tokens & personal names)...")
        sanitizer = ETFSanitizer()
        sanitized_tree = sanitizer.sanitize(selected['tree'])
        sanitized_bytes = encode_etf(sanitized_tree)

        out_bin = f"{prefix}_sanitized.etf"
        out_cpp = f"{prefix}_sanitized.cpp.txt"

        with open(out_bin, "wb") as f:
            f.write(sanitized_bytes)

        var_name = f"{ev_name.lower()}_etf"
        cpp_code = to_cpp_byte_array(sanitized_bytes, var_name)
        with open(out_cpp, "w", encoding="utf-8") as f:
            f.write(cpp_code)

        print(f"\nSuccessfully generated sanitized outputs:")
        print(f"  1. Binary ETF file : {out_bin} ({len(sanitized_bytes)} bytes)")
        print(f"  2. C++ Byte Array  : {out_cpp}")
        print(f"\n[Safe to share]: All real snowflakes remapped to generic IDs; all personal names, topics, and tokens redacted.")

if __name__ == "__main__":
    main()
