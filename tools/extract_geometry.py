#!/usr/bin/env python3
"""
extract_geometry.py — OSM PBF → protobuf preprocessor.

Reads an OSM PBF file, converts WGS84 coordinates to local ENU metres,
applies building height fallback rules, and serialises the output to a
compact protobuf binary consumed by the C++ renderer.

Usage:
    python3 extract_geometry.py input.osm.pbf [-o output.osm_data]

Dependencies:
    pip install osmium protobuf
"""

import argparse
import math
import sys
import struct

try:
    import osmium
except ImportError:
    osmium = None
    print("[WARN] osmium not installed. Install: pip install osmium", file=sys.stderr)

try:
    import google.protobuf
except ImportError:
    print("[ERROR] protobuf not installed. Install: pip install protobuf", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Protobuf import (generated)
# ---------------------------------------------------------------------------
# The .proto file lives at src/proto/osm_data.proto. The generated Python
# module is placed in the _build directory after cmake.  We add both the
# project root and the build dir to the path.
import os
_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_BUILD_DIR = os.path.join(_PROJECT_ROOT, '_build')
for p in (_PROJECT_ROOT, _BUILD_DIR):
    if p not in sys.path:
        sys.path.insert(0, p)

try:
    import osm_data_pb2
except ImportError:
    # Generate the Python bindings on demand if protoc is available.
    proto_file = os.path.join(_PROJECT_ROOT, 'src', 'proto', 'osm_data.proto')
    out_dir = os.path.join(_PROJECT_ROOT, '_build')
    if os.path.exists(proto_file) and os.path.exists(out_dir):
        import subprocess
        subprocess.run([
            'protoc', f'--proto_path={os.path.dirname(proto_file)}',
            f'--python_out={out_dir}',
            proto_file
        ], check=True)
        sys.path.insert(0, out_dir)
        import osm_data_pb2
    else:
        print("[ERROR] Cannot find osm_data_pb2 module or protoc.", file=sys.stderr)
        sys.exit(1)

SCHEMA_VERSION = 2

# ---------------------------------------------------------------------------
# Coordinate conversion
# ---------------------------------------------------------------------------

def enu_convert(lat: float, lon: float, center_lat: float, center_lon: float):
    """WGS84 → local ENU (metres). Uses spherical approximation.

    Returns (easting, northing).
    """
    R = 6_371_000.0  # Earth radius (m)
    d_lat = math.radians(lat - center_lat)
    d_lon = math.radians(lon - center_lon)
    avg_lat = math.radians((lat + center_lat) / 2.0)
    x = R * d_lon * math.cos(avg_lat)
    z = R * d_lat
    return x, z

# ---------------------------------------------------------------------------
# Height heuristics
# ---------------------------------------------------------------------------

def get_height(tags: dict) -> tuple:
    """Determine building height from OSM tags.

    Returns (height_metres, source_string).
    """
    height_str = tags.get('height')
    if height_str:
        try:
            # Handle "12.5 m" or "12.5"
            height_str = height_str.replace(' m', '').replace('m', '').strip()
            return float(height_str), 'tag'
        except (ValueError, TypeError):
            pass

    levels_str = tags.get('building:levels')
    if levels_str:
        try:
            levels = float(levels_str)
            # Cap realistic levels at 100 floors
            if 0 < levels <= 100:
                return levels * 3.0, 'levels'
        except (ValueError, TypeError):
            pass

    return 9.0, 'default'  # fallback


def guess_road_width(road_type: str) -> float:
    """Return a plausible width (metres) for a road type."""
    widths = {
        'motorway': 12.0, 'trunk': 10.0, 'primary': 8.0,
        'secondary': 6.0, 'tertiary': 5.0, 'residential': 4.0,
        'service': 3.0, 'footway': 2.0, 'path': 1.5,
        'cycleway': 2.0, 'track': 3.0,
    }
    return widths.get(road_type, 5.0)


# ---------------------------------------------------------------------------
# OSM handler
# ---------------------------------------------------------------------------

class OSMHandler(osmium.SimpleHandler):
    """osmium handler that extracts buildings, roads, and polygons."""

    def __init__(self, center_lat, center_lon):
        super().__init__()
        self._clat = center_lat
        self._clon = center_lon
        self.buildings = []
        self.roads = []
        self.parks = []
        self.water = []
        self.landuse = []

    def _to_points(self, nodes) -> list:
        pts = []
        for n in nodes:
            if n.location.valid():
                x, z = enu_convert(n.lat, n.lon, self._clat, self._clon)
                pts.append((x, z))
        return pts

    def way(self, w):
        tags = dict(w.tags)
        if not tags:
            return

        pts = self._to_points(w.nodes)
        if len(pts) < 2:
            return

        # --- Buildings ---
        if tags.get('building') and tags.get('building') != 'no':
            if len(pts) >= 3:  # need a polygon (closed)
                height, source = get_height(tags)
                self.buildings.append({
                    'id': w.id,
                    'footprint': pts,
                    'height': height,
                    'height_source': source,
                    'type': tags.get('building', 'yes'),
                })
            return

        # --- Roads ---
        if 'highway' in tags:
            road_type = tags['highway']
            width = guess_road_width(road_type)
            self.roads.append({
                'id': w.id,
                'line': pts,
                'type': road_type,
                'width': width,
            })
            return

        # --- Polygons ---
        if len(pts) >= 3:
            landuse = tags.get('landuse', '')
            natural = tags.get('natural', '')
            leisure = tags.get('leisure', '')

            if natural == 'water' or ('water' in tags and landuse == 'reservoir'):
                self.water.append({'polygon': pts, 'type': 'water'})
            elif leisure == 'park' or landuse == 'grass' or natural == 'grassland':
                self.parks.append({'polygon': pts, 'type': 'park'})
            elif landuse in ('residential', 'commercial', 'industrial', 'farmland',
                             'forest', 'meadow', 'orchard', 'vineyard'):
                self.landuse.append({'polygon': pts, 'type': landuse})


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Extract geometry from OSM PBF and serialize to protobuf.')
    parser.add_argument('input', help='Input .osm.pbf file')
    parser.add_argument('-o', '--output', default=None,
                        help='Output file (default: input file with .osm_data)')
    parser.add_argument('--center-lat', type=float, default=None,
                        help='Override dataset center latitude')
    parser.add_argument('--center-lon', type=float, default=None,
                        help='Override dataset center longitude')
    args = parser.parse_args()

    if osmium is None:
        print("[ERROR] osmium library required. pip install osmium", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(args.input):
        print(f"[ERROR] Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    output = args.output or args.input + '.osm_data'

    # --- First pass: compute bounds from node locations ---
    print(f"[INFO]  Scanning {args.input} for bounds...")
    min_lat, max_lat = 90.0, -90.0
    min_lon, max_lon = 180.0, -180.0
    node_count = 0
    for entity in osmium.FileProcessor(args.input, osmium.osm.NODE):
        if entity.location.valid():
            min_lat = min(min_lat, entity.lat)
            max_lat = max(max_lat, entity.lat)
            min_lon = min(min_lon, entity.lon)
            max_lon = max(max_lon, entity.lon)
            node_count += 1

    if node_count == 0:
        print("[ERROR] No valid nodes found in file.", file=sys.stderr)
        sys.exit(1)

    center_lat = args.center_lat or ((min_lat + max_lat) / 2.0)
    center_lon = args.center_lon or ((min_lon + max_lon) / 2.0)
    print(f"[INFO]  Bounds: lat [{min_lat:.4f}, {max_lat:.4f}]  "
          f"lon [{min_lon:.4f}, {max_lon:.4f}]")
    print(f"[INFO]  Center: ({center_lat:.4f}, {center_lon:.4f})")

    # --- Second pass: extract features ---
    print(f"[INFO]  Extracting features...")
    handler = OSMHandler(center_lat, center_lon)
    handler.apply_file(args.input)

    # --- Compute bounding box in ENU ---
    min_ex, max_ex = float('inf'), float('-inf')
    min_ez, max_ez = float('inf'), float('-inf')

    def extend_bounds(pts):
        nonlocal min_ex, max_ex, min_ez, max_ez
        for x, z in pts:
            min_ex = min(min_ex, x)
            max_ex = max(max_ex, x)
            min_ez = min(min_ez, z)
            max_ez = max(max_ez, z)

    for b in handler.buildings:
        extend_bounds(b['footprint'])
    for r in handler.roads:
        extend_bounds(r['line'])
    for p in handler.parks:
        extend_bounds(p['polygon'])
    for w in handler.water:
        extend_bounds(w['polygon'])
    for l in handler.landuse:
        extend_bounds(l['polygon'])

    if min_ex == float('inf'):
        print("[WARN]  No features extracted; writing empty dataset.")
        min_ex = max_ex = min_ez = max_ez = 0.0

    center_x, center_z = enu_convert(center_lat, center_lon, center_lat, center_lon)

    # --- Build protobuf ---
    data = osm_data_pb2.OSMDataProto()
    data.schema_version = SCHEMA_VERSION
    data.center_x = center_x
    data.center_z = center_z
    data.min_x = min_ex
    data.min_z = min_ez
    data.max_x = max_ex
    data.max_z = max_ez

    for b in handler.buildings:
        pb = data.buildings.add()
        pb.id = b['id']
        pb.height_m = b['height']
        pb.height_source = b['height_source']
        pb.type = b['type']
        for x, z in b['footprint']:
            pt = pb.footprint.add()
            pt.x = x
            pt.z = z

    for r in handler.roads:
        pb = data.roads.add()
        pb.id = r['id']
        pb.type = r['type']
        pb.width_m = r['width']
        for x, z in r['line']:
            pt = pb.line.add()
            pt.x = x
            pt.z = z

    for p in handler.parks:
        pb = data.parks.add()
        pb.type = p['type']
        for x, z in p['polygon']:
            pt = pb.polygon.add()
            pt.x = x
            pt.z = z

    for w in handler.water:
        pb = data.water.add()
        pb.type = w['type']
        for x, z in w['polygon']:
            pt = pb.polygon.add()
            pt.x = x
            pt.z = z

    for l in handler.landuse:
        pb = data.landuse.add()
        pb.type = l['type']
        for x, z in l['polygon']:
            pt = pb.polygon.add()
            pt.x = x
            pt.z = z

    # --- Write ---
    with open(output, 'wb') as f:
        f.write(data.SerializeToString())

    print(f"[INFO]  Wrote {output}")
    print(f"[INFO]  Buildings: {len(handler.buildings)}")
    print(f"[INFO]  Roads:     {len(handler.roads)}")
    print(f"[INFO]  Parks:     {len(handler.parks)}")
    print(f"[INFO]  Water:     {len(handler.water)}")
    print(f"[INFO]  Landuse:   {len(handler.landuse)}")


if __name__ == '__main__':
    main()
