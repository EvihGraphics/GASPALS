# GraphPrinter Blueprint IR Schema

## Purpose

GraphPrinter PNGs are useful for human reading, but C++ reconstruction requires a machine-readable intermediate representation.

This schema defines the normalized Blueprint IR emitted from GraphEditor text chunks and capture metadata.

## File Location

```text
Saved/Screenshots/Blueprints/_blueprint_ir/<AssetName>/<GraphName>/<ModuleId>.json
```

## Top-Level Schema

```json
{
  "schema_version": "gaspals.blueprint_ir.v1",
  "decode_status": "ok|partial|failed",
  "decode_blockers": [],
  "source": {
    "package_name": "",
    "asset_name": "",
    "asset_class": "",
    "editor_graph_name": "",
    "module_id": "",
    "png_path": "",
    "chunk_extract_path": "",
    "capture_command_hash": ""
  },
  "graph": {
    "graph_type": "event_graph|function|anim_graph|state_machine|transition_rule|linked_layer|unknown",
    "runtime_layer": "anim_update",
    "bounds": {"x": 0, "y": 0, "w": 0, "h": 0},
    "entry_points": [],
    "exit_points": []
  },
  "nodes": [],
  "pins": [],
  "edges": [],
  "variables": {"reads": [], "writes": []},
  "functions": {"calls": [], "macros": []},
  "asset_references": [],
  "comment_boxes": [],
  "stitching": {
    "stitch_group_id": "",
    "incoming_module_links": [],
    "outgoing_module_links": [],
    "neighbor_chunks": []
  }
}
```

## Node Record

```json
{
  "node_id": "",
  "node_guid": "",
  "title": "",
  "class": "",
  "node_type": "event|call_function|variable_get|variable_set|branch|sequence|anim_node|state|transition|comment|reroute|unknown",
  "position": {"x": 0, "y": 0},
  "size": {"w": 0, "h": 0},
  "comment_box_id": "",
  "pure": false,
  "latent": false,
  "thread_safe": false,
  "raw_text_span": ""
}
```

## Pin Record

```json
{
  "pin_id": "",
  "node_id": "",
  "name": "",
  "direction": "input|output",
  "pin_type": "exec|bool|float|int|vector|rotator|transform|object|struct|enum|pose|unknown",
  "default_value": "",
  "linked_to": []
}
```

## Edge Record

```json
{
  "edge_id": "",
  "from_pin": "",
  "to_pin": "",
  "edge_type": "exec|data|pose|unknown",
  "crosses_chunk_boundary": false,
  "target_module_id": ""
}
```

## Variable Access

```json
{
  "name": "",
  "type": "",
  "owner": "AnimInstance|Character|Component|Local|Unknown",
  "access_node_id": "",
  "frame_phase": "pre_update|update|post_selection|anim_graph|unknown"
}
```

## C++ Reconstruction Hints

Each IR may optionally include:

```json
{
  "cpp_hints": {
    "recommended_structs": [],
    "recommended_services": [],
    "recommended_method_signature": "",
    "migration_mode": "observe-only|mirror-only|replace-with-fallback|formal-takeover",
    "required_unit_tests": []
  }
}
```

## Decode Status Rules

```text
ok:
  GraphEditor chunk decoded and all nodes/pins/edges were extracted.

partial:
  Enough node-level evidence exists for documentation or human review, but some edges or metadata are missing.

failed:
  Do not use this module for automated C++ generation.
  Keep the raw chunk and failure reason for diagnostics.
```
