#!/usr/bin/env python3
"""
tests/test_python_extractor.py — Unit tests for extract_geometry.py

Tests the helper functions in the Python extraction tool without
requiring a real OSM PBF file.
"""

import sys
import os
import unittest

# Add tools to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))

# Import the module functions directly
import extract_geometry


class TestParseHeight(unittest.TestCase):
    """Tests for parse_height function."""

    def test_direct_height(self):
        """height tag should be used directly."""
        tags = {"height": "15"}
        result = extract_geometry.parse_height(tags)
        self.assertEqual(result, 15.0)

    def test_height_levels(self):
        """building:levels should multiply by 3."""
        tags = {"building:levels": "5"}
        result = extract_geometry.parse_height(tags)
        self.assertEqual(result, 15.0)

    def test_default_commercial(self):
        """Commercial buildings without height should default to 8m."""
        tags = {"building": "commercial"}
        result = extract_geometry.parse_height(tags)
        self.assertEqual(result, 8.0)

    def test_default_residential(self):
        """Residential buildings without height should default to 6m."""
        tags = {"building": "residential"}
        result = extract_geometry.parse_height(tags)
        self.assertEqual(result, 6.0)

    def test_out_of_range(self):
        """Height > 500 should be rejected."""
        tags = {"height": "600"}
        result = extract_geometry.parse_height(tags)
        self.assertIsNone(result)

    def test_invalid_string(self):
        """Non-numeric height should be rejected."""
        tags = {"height": "tall"}
        result = extract_geometry.parse_height(tags)
        self.assertIsNone(result)


class TestGetBuildingColor(unittest.TestCase):
    """Tests for get_building_color function."""

    def test_tall_building(self):
        """Buildings >= 15m should get dark color."""
        color = extract_geometry.get_building_color("commercial", 20)
        self.assertEqual(color, [0.45, 0.45, 0.50])

    def test_commercial_color(self):
        """Commercial buildings should get specific color."""
        color = extract_geometry.get_building_color("commercial", 10)
        self.assertEqual(color, [0.55, 0.50, 0.48])

    def test_residential_color(self):
        """Residential buildings should get specific color."""
        color = extract_geometry.get_building_color("residential", 5)
        self.assertEqual(color, [0.60, 0.48, 0.40])

    def test_default_color(self):
        """Unknown type should get default color."""
        color = extract_geometry.get_building_color("unknown", 5)
        self.assertEqual(color, [0.60, 0.58, 0.55])


class TestGetRoadColor(unittest.TestCase):
    """Tests for get_road_color function."""

    def test_motorway(self):
        color = extract_geometry.get_road_color("motorway")
        self.assertEqual(color, [0.85, 0.75, 0.30])

    def test_residential(self):
        color = extract_geometry.get_road_color("residential")
        self.assertEqual(color, [0.55, 0.50, 0.40])

    def test_unknown(self):
        color = extract_geometry.get_road_color("unknown_highway")
        self.assertEqual(color, [0.55, 0.50, 0.40])


class TestGetRoadWidth(unittest.TestCase):
    """Tests for get_road_width function."""

    def test_motorway(self):
        self.assertEqual(extract_geometry.get_road_width("motorway"), 4.0)

    def test_residential(self):
        self.assertEqual(extract_geometry.get_road_width("residential"), 1.5)

    def test_unknown(self):
        self.assertEqual(extract_geometry.get_road_width("unknown"), 1.0)


class TestSimplifyRing(unittest.TestCase):
    """Tests for simplify_ring (Douglas-Peucker) function."""

    def test_short_ring_unchanged(self):
        """Rings with <= 2 points should be unchanged."""
        ring = [(0, 0), (1, 1)]
        result = extract_geometry.simplify_ring(ring)
        self.assertEqual(result, ring)

    def test_preserves_endpoints(self):
        """Simplified ring should preserve first and last points."""
        ring = [(0, 0), (1, 1), (2, 2), (3, 3), (4, 4)]
        result = extract_geometry.simplify_ring(ring, tolerance=0.0)
        self.assertEqual(result[0], (0, 0))
        self.assertEqual(result[-1], (4, 4))


class TestPostprocess(unittest.TestCase):
    """Tests for postprocess function."""

    def test_buildings_sorted_by_height(self):
        """Buildings should be sorted by height descending."""
        buildings = [
            {"height": 5.0},
            {"height": 20.0},
            {"height": 10.0},
        ]
        buildings, _, _, _, _, _ = extract_geometry.postprocess(
            buildings, [], [], [], [], [], 100
        )
        heights = [b["height"] for b in buildings]
        self.assertEqual(heights, [20.0, 10.0, 5.0])

    def test_buildings_capped(self):
        """Buildings should be capped at max_buildings."""
        buildings = [{"height": float(i)} for i in range(100)]
        buildings, _, _, _, _, _ = extract_geometry.postprocess(
            buildings, [], [], [], [], [], 10
        )
        self.assertEqual(len(buildings), 10)

    def test_roads_sorted_by_priority(self):
        """Roads should be sorted by highway priority."""
        roads = [
            {"type": "residential"},
            {"type": "motorway"},
            {"type": "primary"},
        ]
        _, roads, _, _, _, _ = extract_geometry.postprocess(
            [], roads, [], [], [], [], 100
        )
        types = [r["type"] for r in roads]
        self.assertEqual(types[0], "motorway")
        self.assertEqual(types[1], "primary")


if __name__ == '__main__':
    unittest.main()
