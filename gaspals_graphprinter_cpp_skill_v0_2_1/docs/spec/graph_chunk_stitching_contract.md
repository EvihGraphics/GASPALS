# Graph Chunk Stitching Contract

## Purpose

Large Blueprints are split into small GraphPrinter modules. Splitting is useful only if the system can recover cross-chunk relationships.

This contract defines how split chunks are stitched into a global graph view.

## Required Output

```text
Saved/Screenshots/Blueprints/_stitch/<AssetName>/<GraphName>/stitch_graph.json
Saved/Screenshots/Blueprints/_stitch/<AssetName>/<GraphName>/stitch_report.md
```

## Stitch Graph Schema

```json
{
  "schema_version": "gaspals.graph_stitch.v1",
  "asset_name": "",
  "package_name": "",
  "editor_graph_name": "",
  "modules": [],
  "links": [],
  "unresolved_links": [],
  "stitch_confidence": "high|medium|low|failed"
}
```

## Module Record

```json
{
  "module_id": "",
  "module_title": "",
  "png_path": "",
  "blueprint_ir_path": "",
  "split_reason": "comment_box|graph_island|state_machine|function_boundary|pixel_limit|node_limit|fallback",
  "bounds_in_graph_space": {"x": 0, "y": 0, "w": 0, "h": 0},
  "overlap_margin": 0,
  "comment_box_labels": [],
  "entry_node_ids": [],
  "exit_node_ids": []
}
```

## Link Record

```json
{
  "link_id": "",
  "from_module_id": "",
  "to_module_id": "",
  "link_type": "exec_continuation|data_dependency|pose_link|variable_read_write|reroute_continuation|state_transition|unknown",
  "from_node_id": "",
  "to_node_id": "",
  "from_pin": "",
  "to_pin": "",
  "confidence": "high|medium|low"
}
```

## Split Quality Rules

Prefer splitting by:

```text
1. Comment boxes with meaningful labels.
2. Function/event graph boundaries.
3. State machine and transition rule boundaries.
4. Linked layer or AnimGraph subgraph boundaries.
5. Graph islands.
6. Pixel/node fallback limits.
```

Do not split in a way that loses critical exec/pose/data links without creating unresolved link records.

## Acceptance

```text
[ ] Every OK PNG belongs to exactly one module.
[ ] Every module belongs to one stitch group.
[ ] Cross-chunk links are recorded or explicitly unresolved.
[ ] Unresolved links are reported.
[ ] Agent can navigate from a C++ task to all relevant chunks.
```
