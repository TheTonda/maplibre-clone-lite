#!/usr/bin/env python3
"""
Generate a tiny synthetic OSM test dataset in protobuf format.

Creates a small city block (~500×500m) with:
- 3 buildings of varying heights
- 2 roads (primary + residential)
- 1 park
- 1 water body

Usage: python3 generate_test_data.py [-o data/test_scene.osm_data]
"""

import argparse, os, sys

_PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_BUILD = os.path.join(_PROJ, '_build')
for p in (_PROJ, _BUILD):
    if p not in sys.path:
        sys.path.insert(0, p)

import osm_data_pb2 as pb

SCHEMA = 2

def make_building(b, x, z, w, d, h, btype='yes'):
    """Axis-aligned rectangular building centred at (x,z) with width w, depth d, height h."""
    hw, hd = w/2, d/2
    b.footprint.add(x=round(x-hw,1), z=round(z-hd,1))
    b.footprint.add(x=round(x+hw,1), z=round(z-hd,1))
    b.footprint.add(x=round(x+hw,1), z=round(z+hd,1))
    b.footprint.add(x=round(x-hw,1), z=round(z+hd,1))
    b.height_m = h
    b.height_source = 'default'
    b.type = btype

def make_road_pts(r, pts, rtype='residential', width=4.0):
    for x,z in pts:
        r.line.add(x=x, z=z)
    r.type = rtype
    r.width_m = width

def make_polygon(p, pts, ptype):
    for x,z in pts:
        p.polygon.add(x=x, z=z)
    p.type = ptype

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output', default='data/test_scene.osm_data')
    args = parser.parse_args()

    data = pb.OSMDataProto()
    data.schema_version = SCHEMA
    data.center_x = 0.0
    data.center_z = 0.0

    # --- Buildings ---
    bldgs = [
        # (x, z, width, depth, height, type)
        ( -60,  -40, 20, 15, 12.0, 'apartments'),
        (   0,  -30, 25, 20, 25.0, 'commercial'),
        (  80,  -60, 18, 18,  6.0, 'house'),
        ( -80,   50, 15, 12,  9.0, 'house'),
        (  40,   30, 30, 18, 35.0, 'office'),
        ( -30,   80, 22, 22,  9.0, 'house'),
    ]
    bid = 1000
    for x,z,w,d,h,bt in bldgs:
        b = data.buildings.add()
        b.id = bid
        bid += 1
        make_building(b, x, z, w, d, h, bt)
    data.min_x = min(p.x for b in list(data.buildings) for p in b.footprint)
    data.max_x = max(p.x for b in list(data.buildings) for p in b.footprint)
    data.min_z = min(p.z for b in list(data.buildings) for p in b.footprint)
    data.max_z = max(p.z for b in list(data.buildings) for p in b.footprint)

    # --- Roads ---
    # Primary road: east-west at z=0
    r = data.roads.add()
    r.id = 2000
    pts = [(-300, 0), (-150, 0), (0, 0), (150, 0), (300, 0)]
    make_road_pts(r, pts, 'primary', 8.0)

    # Residential road: north-south at x=0
    r = data.roads.add()
    r.id = 2001
    pts = [(0, -200), (0, -100), (0, 0), (0, 100), (0, 200)]
    make_road_pts(r, pts, 'residential', 4.0)

    # Service road: diagonal
    r = data.roads.add()
    r.id = 2002
    pts = [(-80, -80), (-30, -30), (0, 0), (30, 30), (80, 80)]
    make_road_pts(r, pts, 'service', 3.0)

    # --- Parks ---
    p = data.parks.add()
    make_polygon(p, [(100, 50), (180, 40), (200, 100), (160, 150), (80, 120)], 'park')

    # --- Water ---
    w = data.water.add()
    make_polygon(w, [(-200, -150), (-100, -160), (-80, -100), (-150, -50), (-250, -80)], 'water')

    # --- Landuse ---
    l = data.landuse.add()
    make_polygon(l, [(50, -150), (250, -120), (280, -50), (100, -30), (30, -80)], 'residential')

    # Bounds
    for r in list(data.roads):
        for pt in r.line:
            data.min_x = min(data.min_x, pt.x)
            data.max_x = max(data.max_x, pt.x)
            data.min_z = min(data.min_z, pt.z)
            data.max_z = max(data.max_z, pt.z)

    for feat in list(data.parks) + list(data.water) + list(data.landuse):
        for pt in feat.polygon:
            data.min_x = min(data.min_x, pt.x)
            data.max_x = max(data.max_x, pt.x)
            data.min_z = min(data.min_z, pt.z)
            data.max_z = max(data.max_z, pt.z)

    out_path = os.path.join(_PROJ, args.output)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(data.SerializeToString())

    print(f"[INFO] Wrote {out_path}")
    print(f"       Buildings: {len(data.buildings)}  Roads: {len(data.roads)}")
    print(f"       Parks: {len(data.parks)}  Water: {len(data.water)}  Landuse: {len(data.landuse)}")
    print(f"       Bounds: X[{data.min_x:.0f},{data.max_x:.0f}] Z[{data.min_z:.0f},{data.max_z:.0f}]")

if __name__ == '__main__':
    main()
