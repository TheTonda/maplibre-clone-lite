#!/usr/bin/env python3
"""OSM PBF → Tiled Protobuf Preprocessor

Converts OSM PBF files into a slippy-map tile pyramid of zstd-compressed
protobuf files. Applies Douglas-Peucker simplification per zoom level.

Usage:
    python preprocess.py <input.pbf> <output_dir> [--zoom 8,12,15,17]
"""
import argparse
import math
import os
import struct
import sys
from collections import defaultdict
from typing import List, Optional, Tuple

import osmium
import zstandard

# Add tools/ to sys.path for osm_data_pb2 import
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import osm_data_pb2 as pb

# ── Constants ─────────────────────────────────────────────────────────
R = 6371000.0  # Earth radius in meters
DEG_TO_RAD = math.pi / 180.0
RAD_TO_DEG = 180.0 / math.pi

ZOOM_TOLERANCE = {
    8: 500.0,
    12: 50.0,
    15: 5.0,
    17: 0.5,
}

# Feature tags that classify a way/area as building
BUILDING_TAGS = {"building", "building:part"}

# Feature tags for roads
HIGHWAY_TAGS = {
    "motorway", "trunk", "primary", "secondary", "tertiary",
    "unclassified", "residential", "service", "track", "path",
    "footway", "cycleway", "pedestrian", "living_street", "road",
}

# Feature tags for natural/landuse/leisure polygons
POLYGON_TAGS = {
    "natural": {"water", "wood", "scrub", "grassland", "heath", "wetland", "beach", "sand"},
    "landuse": {"forest", "residential", "industrial", "commercial", "farmland",
                "meadow", "orchard", "vineyard", "grass", "recreation_ground",
                "retail", "military", "quarry", "allotments", "cemetery",
                "village_green", "brownfield", "greenfield", "construction"},
    "leisure": {"park", "garden", "golf_course", "pitch", "playground",
                "sports_centre", "stadium", "track", "nature_reserve"},
    "waterway": {"riverbank", "dock", "basin"},
}


# ── Math Helpers ──────────────────────────────────────────────────────

def lat_lon_to_world_enu(lat: float, lon: float,
                         ref_lat: float, ref_lon: float) -> Tuple[float, float]:
    """Convert WGS84 lat/lon to world ENU meters from reference point."""
    x = R * math.cos(ref_lat * DEG_TO_RAD) * ((lon - ref_lon) * DEG_TO_RAD)
    z = R * (lat - ref_lat) * DEG_TO_RAD
    return x, z


def lat_lon_to_tile(lat: float, lon: float, zoom: int) -> Tuple[int, int]:
    """Convert lat/lon to tile x/y at given zoom level (slippy map)."""
    n = 1 << zoom
    lat_rad = lat * DEG_TO_RAD
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    x = max(0, min(n - 1, x))
    y = max(0, min(n - 1, y))
    return x, y


def tile_center_lat_lon(zoom: int, x: int, y: int) -> Tuple[float, float]:
    """Get the center lat/lon of a tile."""
    n = 1 << zoom
    lon = (x + 0.5) / n * 360.0 - 180.0
    lat_rad = math.atan(math.sinh(math.pi * (1.0 - 2.0 * (y + 0.5) / n)))
    lat = lat_rad * RAD_TO_DEG
    return lat, lon


def tile_bbox_world_enu(zoom: int, x: int, y: int,
                        ref_lat: float, ref_lon: float) -> Tuple[float, float, float, float]:
    """Get tile bounding box in world ENU meters."""
    lat1, lon1 = tile_center_lat_lon(zoom, x, y)
    # Approximate tile bbox from corners
    n = 1 << zoom
    lon_min = x / n * 360.0 - 180.0
    lon_max = (x + 1) / n * 360.0 - 180.0
    lat_max_rad = math.atan(math.sinh(math.pi * (1.0 - 2.0 * y / n)))
    lat_min_rad = math.atan(math.sinh(math.pi * (1.0 - 2.0 * (y + 1) / n)))
    lat_max = lat_max_rad * RAD_TO_DEG
    lat_min = lat_min_rad * RAD_TO_DEG

    x_min, z_min = lat_lon_to_world_enu(lat_min, lon_min, ref_lat, ref_lon)
    x_max, z_max = lat_lon_to_world_enu(lat_max, lon_max, ref_lat, ref_lon)
    return x_min, x_max, z_min, z_max


# ── Douglas-Peucker Simplification ────────────────────────────────────

def douglas_peucker(points: List[Tuple[float, float]],
                    tolerance: float) -> List[Tuple[float, float]]:
    """Simplify a polyline using Douglas-Peucker algorithm."""
    if len(points) <= 2:
        return points

    # Find point with maximum distance
    max_dist = 0.0
    max_idx = 0
    for i in range(1, len(points) - 1):
        dist = _perpendicular_distance(points[i], points[0], points[-1])
        if dist > max_dist:
            max_dist = dist
            max_idx = i

    if max_dist > tolerance:
        left = douglas_peucker(points[:max_idx + 1], tolerance)
        right = douglas_peucker(points[max_idx:], tolerance)
        return left[:-1] + right
    else:
        return [points[0], points[-1]]


def _perpendicular_distance(point: Tuple[float, float],
                            line_start: Tuple[float, float],
                            line_end: Tuple[float, float]) -> float:
    """Distance from point to line segment."""
    dx = line_end[0] - line_start[0]
    dy = line_end[1] - line_start[1]
    if dx == 0 and dy == 0:
        return math.hypot(point[0] - line_start[0], point[1] - line_start[1])
    t = ((point[0] - line_start[0]) * dx + (point[1] - line_start[1]) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    proj_x = line_start[0] + t * dx
    proj_y = line_start[1] + t * dy
    return math.hypot(point[0] - proj_x, point[1] - proj_y)


# ── OSM Handler ───────────────────────────────────────────────────────

class TilePreprocessor(osmium.SimpleHandler):
    def __init__(self, ref_lat: float, ref_lon: float,
                 zoom_levels: List[int]):
        super().__init__()
        self.ref_lat = ref_lat
        self.ref_lon = ref_lon
        self.zoom_levels = zoom_levels
        self.min_lat = 90.0
        self.max_lat = -90.0
        self.min_lon = 180.0
        self.max_lon = -180.0

        # Per-zoom accumulated features
        # Key: (z, x, y) → list of feature tuples
        self.tiles: dict = {z: defaultdict(list) for z in zoom_levels}

        # Node coordinate cache (needed for way geometry resolution)
        self.node_coords: dict = {}

    def node(self, n):
        """Cache node coordinates for way resolution."""
        if n.location.valid:
            self.node_coords[n.id] = (n.location.lat, n.location.lon)

    def way(self, w):
        if not w.nodes or len(w.nodes) < 2:
            return

        tags = {t.k: t.v for t in w.tags}
        is_building = bool(tags.keys() & BUILDING_TAGS)
        highway = tags.get("highway")
        is_road = highway in HIGHWAY_TAGS if highway else False

        if not is_building and not is_road:
            return

        # Resolve coordinates
        coords = []
        for nr in w.nodes:
            coord = self.node_coords.get(nr.ref)
            if coord:
                coords.append(coord)
            elif nr.location.valid:
                coords.append((nr.lat, nr.lon))

        if len(coords) < 2:
            return

        # Update dataset bounds
        for lat, lon in coords:
            self.min_lat = min(self.min_lat, lat)
            self.max_lat = max(self.max_lat, lat)
            self.min_lon = min(self.min_lon, lon)
            self.max_lon = max(self.max_lon, lon)

        # Process for each zoom level
        for z in self.zoom_levels:
            if is_building and z < 15:
                continue  # Skip buildings at low zoom

            tolerance = ZOOM_TOLERANCE.get(z, 1.0)
            simplified = douglas_peucker(coords, tolerance)

            if len(simplified) < 2:
                continue

            # Convert to world ENU
            world_pts = [lat_lon_to_world_enu(lat, lon, self.ref_lat, self.ref_lon)
                         for lat, lon in simplified]

            # Assign to tiles
            seen_tiles = set()
            for lat, lon in simplified:
                tx, ty = lat_lon_to_tile(lat, lon, z)
                seen_tiles.add((tx, ty))

            for tx, ty in seen_tiles:
                tc_lat, tc_lon = tile_center_lat_lon(z, tx, ty)
                tc_wx, tc_wz = lat_lon_to_world_enu(tc_lat, tc_lon,
                                                     self.ref_lat, self.ref_lon)
                # Convert to local ENU relative to tile center
                local_pts = [(wx - tc_wx, wz - tc_wz) for wx, wz in world_pts]

                if is_road:
                    road_type = highway or "road"
                    road_width = 6.0
                    width_str = tags.get("width")
                    if width_str:
                        try:
                            road_width = float(width_str)
                        except ValueError:
                            pass
                    self.tiles[z][(tx, ty)].append(
                        ("road", local_pts, w.id, road_type, road_width))
                elif is_building:
                    height_str = tags.get("height", "0")
                    try:
                        height = float(height_str)
                    except ValueError:
                        height = 0.0
                    self.tiles[z][(tx, ty)].append(
                        ("building", local_pts, w.id, "", height))

    def area(self, a):
        """Handle polygon areas (parks, water, landuse)."""
        tags = {t.k: t.v for t in a.tags}
        poly_type = self._classify_polygon(tags)
        if not poly_type:
            return

        # Get outer rings
        try:
            outer_rings = list(a.outer_rings())
        except Exception:
            return
        if not outer_rings:
            return

        for ring in outer_rings:
            coords = []
            for nr in ring:
                coord = self.node_coords.get(nr.ref)
                if coord:
                    coords.append(coord)
                elif nr.location.valid:
                    coords.append((nr.lat, nr.lon))

            if len(coords) < 3:
                continue

            # Update bounds
            for lat, lon in coords:
                self.min_lat = min(self.min_lat, lat)
                self.max_lat = max(self.max_lat, lat)
                self.min_lon = min(self.min_lon, lon)
                self.max_lon = max(self.max_lon, lon)

            for z in self.zoom_levels:
                tolerance = ZOOM_TOLERANCE.get(z, 1.0)
                simplified = douglas_peucker(coords, tolerance)
                if len(simplified) < 3:
                    continue

                world_pts = [lat_lon_to_world_enu(lat, lon, self.ref_lat, self.ref_lon)
                             for lat, lon in simplified]

                seen_tiles = set()
                for lat, lon in simplified:
                    tx, ty = lat_lon_to_tile(lat, lon, z)
                    seen_tiles.add((tx, ty))

                for tx, ty in seen_tiles:
                    tc_lat, tc_lon = tile_center_lat_lon(z, tx, ty)
                    tc_wx, tc_wz = lat_lon_to_world_enu(tc_lat, tc_lon,
                                                         self.ref_lat, self.ref_lon)
                    local_pts = [(wx - tc_wx, wz - tc_wz) for wx, wz in world_pts]
                    self.tiles[z][(tx, ty)].append(
                        ("polygon", local_pts, 0, poly_type, 0.0))

    def _classify_polygon(self, tags: dict) -> Optional[str]:
        """Classify a polygon as water, park, or landuse."""
        # Water first
        if tags.get("natural") == "water":
            return "water"
        if tags.get("waterway") in ("riverbank", "dock", "basin"):
            return "water"
        if tags.get("water"):
            return "water"

        # Parks
        if tags.get("leisure") == "park":
            return "park"
        if tags.get("leisure") == "garden":
            return "park"
        if tags.get("leisure") == "nature_reserve":
            return "park"
        if tags.get("boundary") == "national_park":
            return "park"

        # Generic landuse
        for key in ("landuse", "natural", "leisure"):
            if key in tags:
                return "landuse"

        return None


# ── Tile Writing ──────────────────────────────────────────────────────

def write_tiles(tiles: dict, output_dir: str, zoom_levels: List[int],
                ref_lat: float, ref_lon: float):
    """Serialize and compress tiles to output directory."""

    total_tiles = 0

    for z in zoom_levels:
        zoom_tiles = tiles.get(z, {})
        for (tx, ty), features in zoom_tiles.items():
            tc_lat, tc_lon = tile_center_lat_lon(z, tx, ty)

            tile = pb.Tile()
            tile.zoom = z
            tile.tile_x = tx
            tile.tile_y = ty
            tile.center_lat = tc_lat
            tile.center_lon = tc_lon
            tile.simplify_tolerance = ZOOM_TOLERANCE.get(z, 1.0)

            n_buildings = 0
            n_roads = 0
            n_polygons = 0

            for feat in features:
                ftype = feat[0]
                pts = feat[1]

                if ftype == "building":
                    n_buildings += 1
                    b = tile.buildings.add()
                    b.id = feat[2]
                    b.height_m = feat[4]
                    for px, pz in pts:
                        pt = b.footprint.add()
                        pt.x = px
                        pt.z = pz

                elif ftype == "road":
                    n_roads += 1
                    r = tile.roads.add()
                    r.id = feat[2]
                    r.type = feat[3]
                    r.width_m = feat[4]
                    for px, pz in pts:
                        pt = r.line.add()
                        pt.x = px
                        pt.z = pz

                elif ftype == "polygon":
                    n_polygons += 1
                    p = tile.polygons.add()
                    p.type = feat[3]
                    for px, pz in pts:
                        pt = p.polygon.add()
                        pt.x = px
                        pt.z = pz

            tile.building_count = n_buildings
            tile.road_count = n_roads
            tile.polygon_count = n_polygons

            # Serialize and compress
            raw = tile.SerializeToString()
            cctx = zstandard.ZstdCompressor(level=3)
            compressed = cctx.compress(raw)

            # Write to output_dir/z/x/y.bin
            tile_dir = os.path.join(output_dir, str(z), str(tx))
            os.makedirs(tile_dir, exist_ok=True)
            tile_path = os.path.join(tile_dir, f"{ty}.bin")
            with open(tile_path, "wb") as f:
                f.write(compressed)

            total_tiles += 1

    return total_tiles


def write_metadata(output_dir: str, name: str,
                   min_lat: float, max_lat: float,
                   min_lon: float, max_lon: float,
                   ref_lat: float, ref_lon: float,
                   zoom_levels: List[int], total_tiles: int):
    """Write zstd-compressed DatasetMetadata protobuf."""
    meta = pb.DatasetMetadata()
    meta.name = name
    meta.min_lat = min_lat
    meta.max_lat = max_lat
    meta.min_lon = min_lon
    meta.max_lon = max_lon
    meta.ref_lat = ref_lat
    meta.ref_lon = ref_lon
    meta.zoom_levels.extend(zoom_levels)
    meta.total_tiles = total_tiles

    raw = meta.SerializeToString()
    cctx = zstandard.ZstdCompressor(level=3)
    compressed = cctx.compress(raw)

    meta_path = os.path.join(output_dir, "metadata.bin")
    with open(meta_path, "wb") as f:
        f.write(compressed)


# ── Main ──────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Process OSM PBF into zstd-compressed protobuf tile pyramid")
    parser.add_argument("input", help="Input OSM PBF file")
    parser.add_argument("output_dir", help="Output directory for tile pyramid")
    parser.add_argument("--zoom", default="8,12,15,17",
                        help="Comma-separated zoom levels (default: 8,12,15,17)")
    parser.add_argument("--ref-lat", type=float, default=None,
                        help="ENU reference latitude (default: dataset center)")
    parser.add_argument("--ref-lon", type=float, default=None,
                        help="ENU reference longitude (default: dataset center)")
    args = parser.parse_args()

    zoom_levels = [int(z.strip()) for z in args.zoom.split(",")]

    # Read PBF header for bounds (so we can compute ref point)
    print(f"Reading PBF header from {args.input}...")
    reader = osmium.io.Reader(args.input)
    header = reader.header()
    bbox = header.box()
    reader.close()

    min_lat = bbox.bottom_left.lat
    min_lon = bbox.bottom_left.lon
    max_lat = bbox.top_right.lat
    max_lon = bbox.top_right.lon

    ref_lat = args.ref_lat if args.ref_lat is not None else (min_lat + max_lat) / 2.0
    ref_lon = args.ref_lon if args.ref_lon is not None else (min_lon + max_lon) / 2.0

    print(f"Bounds: lat [{min_lat:.4f}, {max_lat:.4f}], lon [{min_lon:.4f}, {max_lon:.4f}]")
    print(f"Reference point: ({ref_lat:.6f}, {ref_lon:.6f})")
    print(f"Zoom levels: {zoom_levels}")

    # Process
    print("Processing features...")
    processor = TilePreprocessor(ref_lat, ref_lon, zoom_levels)
    processor.apply_file(args.input, locations=True, idx="flex_mem")

    # Use the bounds from the PBF header (more reliable than feature bounds)
    print(f"\nWriting tiles to {args.output_dir}...")
    total = write_tiles(processor.tiles, args.output_dir, zoom_levels,
                        ref_lat, ref_lon)

    # Write metadata
    dataset_name = os.path.basename(args.output_dir)
    write_metadata(args.output_dir, dataset_name,
                   min_lat, max_lat, min_lon, max_lon,
                   ref_lat, ref_lon, zoom_levels, total)

    print(f"Done! {total} tiles written to {args.output_dir}")
    print(f"Metadata: {args.output_dir}/metadata.bin")


if __name__ == "__main__":
    main()
