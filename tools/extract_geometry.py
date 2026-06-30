#!/usr/bin/env python3
"""
extract_geometry.py — Convert OSM PBF to JSON for the map renderer.

Two-pass approach:
  Pass 1: Collect all node (lon, lat) positions
  Pass 2: Extract buildings, roads, parks, water from ways

Uses osmium library for OSM PBF parsing.
"""

import osmium
import json
import math
import os
import sys
import argparse


def parse_height(tags):
    """Estimate building height from OSM tags. Returns meters or None."""
    for key in ("height", "building:height"):
        if key in tags:
            try:
                h = float(tags[key])
                if 0 < h < 500:
                    return h
            except (ValueError, TypeError):
                pass

    for key in ("building:levels", "building:levels:actual"):
        if key in tags:
            try:
                levels = float(tags[key])
                if levels > 0:
                    return levels * 3.0
            except (ValueError, TypeError):
                pass

    btype = tags.get("building", "").lower()
    defaults = {
        "commercial": 8.0,
        "office": 10.0,
        "apartments": 12.0,
        "residential": 6.0,
        "house": 4.0,
        "industrial": 8.0,
        "school": 5.0,
        "hospital": 8.0,
        "retail": 6.0,
        "terrace": 4.0,
        "garage": 3.0,
    }
    if btype in defaults:
        return defaults[btype]

    if "building" in tags:
        return 5.0

    return None


def get_building_color(btype, height):
    """Return RGB color for a building based on type and height."""
    btype = btype.lower() if btype else "yes"
    if height and height >= 15:
        return [0.45, 0.45, 0.50]
    if btype in ("commercial", "office", "retail"):
        return [0.55, 0.50, 0.48]
    if btype in ("apartments", "residential"):
        return [0.60, 0.48, 0.40]
    if btype == "industrial":
        return [0.50, 0.50, 0.55]
    if btype in ("school", "hospital"):
        return [0.65, 0.55, 0.50]
    return [0.60, 0.58, 0.55]


def get_road_color(highway_type):
    """Return RGB color for a road based on highway type."""
    colors = {
        "motorway": [0.85, 0.75, 0.30],
        "trunk": [0.80, 0.70, 0.25],
        "primary": [0.75, 0.65, 0.20],
        "secondary": [0.70, 0.60, 0.18],
        "tertiary": [0.65, 0.55, 0.15],
        "residential": [0.55, 0.50, 0.40],
        "service": [0.45, 0.42, 0.35],
        "unclassified": [0.50, 0.48, 0.40],
        "living_street": [0.45, 0.42, 0.35],
        "footway": [0.60, 0.55, 0.45],
    }
    return colors.get(highway_type, [0.55, 0.50, 0.40])


def get_road_width(highway_type):
    """Return line width for a road type."""
    widths = {
        "motorway": 4.0,
        "trunk": 3.5,
        "primary": 3.0,
        "secondary": 2.5,
        "tertiary": 2.0,
        "residential": 1.5,
        "service": 1.0,
        "unclassified": 1.2,
    }
    return widths.get(highway_type, 1.0)


def simplify_ring(ring, tolerance=0.00005):
    """Douglas-Peucker simplification. tolerance in degrees (~5m at Delhi lat)."""
    if len(ring) <= 2:
        return ring

    def sq_dist(p, a, b):
        dx = b[0] - a[0]
        dy = b[1] - a[1]
        if dx == 0 and dy == 0:
            return (p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2
        t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
        proj = [a[0] + t * dx, a[1] + t * dy]
        return (p[0] - proj[0]) ** 2 + (p[1] - proj[1]) ** 2

    def _simplify(points, tol):
        if len(points) < 3:
            return points
        dmax = 0
        idx = 0
        for i in range(1, len(points) - 1):
            d = math.sqrt(sq_dist(points[i], points[0], points[-1]))
            if d > dmax:
                dmax = d
                idx = i
        if dmax > tol:
            left = _simplify(points[: idx + 1], tol)
            right = _simplify(points[idx:], tol)
            return left[:-1] + right
        return [points[0], points[-1]]

    return _simplify(ring, tolerance)


def pass1_collect_nodes(input_path):
    """Pass 1: Collect all node positions from the PBF file."""
    print("Pass 1: Collecting node positions...")
    node_cache = {}

    class NodeCollector(osmium.SimpleHandler):
        def node(self, n):
            node_cache[n.id] = (n.lon, n.lat)

    collector = NodeCollector()
    collector.apply_file(input_path)
    print(f"  Collected {len(node_cache)} nodes.")
    return node_cache


def pass2_extract(input_path, node_cache, max_buildings=50000, simplify=True):
    """Pass 2: Extract buildings, roads, parks, water from ways."""
    print("Pass 2: Extracting features...")

    buildings = []
    roads = []
    parks = []
    water_polygons = []
    water_lines = []
    landuse = []

    road_types_include = {
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "residential", "service", "unclassified", "living_street",
    }

    class FeatureExtractor(osmium.SimpleHandler):
        def __init__(self):
            super().__init__()
            self.stats = {
                "ways_seen": 0,
                "buildings": 0,
                "roads": 0,
                "parks": 0,
                "water_poly": 0,
                "water_lines": 0,
                "landuse": 0,
                "unresolved": 0,
            }

        def way(self, w):
            self.stats["ways_seen"] += 1
            tags = {t.k: t.v for t in w.tags}
            refs = list(w.nodes)
            if len(refs) < 3:
                return

            # Build ring from resolved node references
            ring = []
            for r in refs:
                if r.ref in node_cache:
                    ring.append(node_cache[r.ref])
                else:
                    self.stats["unresolved"] += 1

            if len(ring) < 3:
                return

            if ring[0] != ring[-1]:
                ring.append(ring[0])

            if simplify and len(ring) > 10:
                ring = simplify_ring(ring)

            # --- Buildings ---
            if "building" in tags:
                height = parse_height(tags)
                if height is None or height <= 0:
                    return
                btype = tags.get("building", "yes")
                color = get_building_color(btype, height)
                name = tags.get("name", "")
                buildings.append({
                    "id": w.id,
                    "name": name,
                    "type": btype,
                    "height": round(height, 1),
                    "color": color,
                    "polygon": ring,
                })
                self.stats["buildings"] += 1
                return

            # --- Parks / green spaces ---
            if tags.get("leisure") in ("park", "garden", "playground", "nature_reserve"):
                parks.append({
                    "id": w.id,
                    "name": tags.get("name", ""),
                    "type": tags["leisure"],
                    "polygon": ring,
                })
                self.stats["parks"] += 1
                return

            # --- Landuse (green/residential areas) ---
            if tags.get("landuse") in ("residential", "forest", "meadow", "farmland", "grass", "cemetery"):
                landuse.append({
                    "id": w.id,
                    "name": tags.get("name", ""),
                    "type": tags["landuse"],
                    "polygon": ring,
                })
                self.stats["landuse"] += 1
                return

            # --- Water bodies (polygons) ---
            if tags.get("natural") == "water" and ring is not None:
                water_polygons.append({
                    "id": w.id,
                    "name": tags.get("name", ""),
                    "polygon": ring,
                })
                self.stats["water_poly"] += 1
                return

            if tags.get("waterway") in ("river", "stream", "canal", "dock"):
                line_coords = []
                for r in refs:
                    if r.ref in node_cache:
                        line_coords.append(node_cache[r.ref])
                if len(line_coords) >= 2:
                    water_lines.append({
                        "id": w.id,
                        "name": tags.get("name", ""),
                        "type": tags["waterway"],
                        "line": line_coords,
                    })
                    self.stats["water_lines"] += 1
                return

            # --- Roads ---
            highway = tags.get("highway")
            if highway and highway in road_types_include:
                line_coords = []
                for r in refs:
                    if r.ref in node_cache:
                        line_coords.append(node_cache[r.ref])
                if len(line_coords) >= 2:
                    roads.append({
                        "id": w.id,
                        "name": tags.get("name", ""),
                        "type": highway,
                        "color": get_road_color(highway),
                        "width": get_road_width(highway),
                        "line": line_coords,
                    })
                    self.stats["roads"] += 1

    extractor = FeatureExtractor()
    extractor.apply_file(input_path)

    print(f"  Extracted:")
    print(f"    Buildings: {extractor.stats['buildings']}")
    print(f"    Roads: {extractor.stats['roads']}")
    print(f"    Parks/green: {extractor.stats['parks']}")
    print(f"    Water bodies: {extractor.stats['water_poly']}")
    print(f"    Water lines: {extractor.stats['water_lines']}")
    print(f"    Landuse: {extractor.stats['landuse']}")
    print(f"    Unresolved node refs: {extractor.stats['unresolved']}")

    return buildings, roads, parks, water_polygons, water_lines, landuse


def postprocess(buildings, roads, parks, water_polygons, water_lines, landuse, max_buildings):
    """Sort and filter extracted data for rendering."""
    # Sort buildings by height (tallest first), cap at max
    buildings.sort(key=lambda b: -b["height"])
    if len(buildings) > max_buildings:
        buildings = buildings[:max_buildings]

    # Sort roads by priority
    road_priority = [
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "residential", "service", "unclassified", "living_street",
    ]
    roads.sort(key=lambda r: road_priority.index(r["type"]) if r["type"] in road_priority else 99)

    # Sort water lines by type priority
    water_priority = ["river", "canal", "stream", "dock"]
    water_lines.sort(key=lambda w: water_priority.index(w["type"]) if w["type"] in water_priority else 99)

    return buildings, roads, parks, water_polygons, water_lines, landuse


def main():
    parser = argparse.ArgumentParser(description="Extract geometry from OSM PBF for map renderer")
    parser.add_argument("input", help="Input OSM PBF file")
    parser.add_argument("output", help="Output JSON file")
    parser.add_argument("--max-buildings", type=int, default=40000,
                        help="Max buildings to include (default: 40000)")
    parser.add_argument("--no-simplify", action="store_true",
                        help="Disable polygon simplification")
    args = parser.parse_args()

    node_cache = pass1_collect_nodes(args.input)
    buildings, roads, parks, water_polygons, water_lines, landuse = pass2_extract(
        args.input, node_cache, max_buildings=args.max_buildings, simplify=not args.no_simplify
    )
    buildings, roads, parks, water_polygons, water_lines, landuse = postprocess(
        buildings, roads, parks, water_polygons, water_lines, landuse, args.max_buildings
    )

    output = {
        "buildings": buildings,
        "roads": roads,
        "parks": parks,
        "water_polygons": water_polygons,
        "water_lines": water_lines,
        "landuse": landuse,
    }

    print(f"\nWriting {args.output}...")
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        json.dump(output, f)

    size_mb = os.path.getsize(args.output) / (1024 * 1024)
    print(f"Done. Output: {args.output} ({size_mb:.1f} MB)")
    print(f"  Buildings: {len(buildings)}, Roads: {len(roads)}, "
          f"Parks: {len(parks)}, Water polys: {len(water_polygons)}, "
          f"Water lines: {len(water_lines)}, Landuse: {len(landuse)}")


if __name__ == "__main__":
    main()
