# Runtime Insertion Points

## Observed Host Architecture

- `GASPALS` is a UE 5.7 project with a content-heavy `GASPALS` plugin, not an existing C++ locomotion runtime module.
- The formal locomotion spine is blueprint-driven: the character blueprint is the state/input aggregator and the animation blueprint is the final motion-decision surface.
- Because of that shape, the safest Shadow Mode strategy is an additive observer plugin that attaches to the host character at runtime and writes logs to `Saved/Logs`, without replacing animation graphs or pose search assets.

## Primary Observation Surfaces

### 1. Character Input and State Aggregation

Use `CBP_SandboxCharacter` as the primary host-side observation point.

Why:

- It owns the translation from Enhanced Input actions into locomotion-friendly state.
- It already carries the state that Shadow Mode needs to mirror: desired gait, aim state, traversal request inputs, ragdoll state, and pose-search continuation state.

What to read there:

- input intent
- desired gait / walk / sprint / strafe state
- control rotation
- traversal request and check structs
- current movement state and velocity

### 2. Final Animation Decision Surface

Use `ABP_SandboxCharacter` as the final animation-observation surface.

Why:

- It is where Motion Matching, Pose Search history collection, linked layers, blend stacks, root-bone offsets, orientation warping, foot placement, and Leg IK converge.
- Replacing or wrapping this graph in Phase 0-2 would violate the Shadow Mode rule.

What to read there:

- active AnimInstance class
- active montage, if any
- current overlay-related state passed into linked layers
- pose-search database family and animation-state transitions

## Secondary Observation Surfaces

### Player Controller Boundary

Use `PC_Sandbox` only for:

- possession changes
- character switching / observer rebinding

Do not use `PC_Sandbox` as the main locomotion sampling surface.

### Traversal Boundary

Use traversal structs/components as the environment interaction boundary:

- traversal trace component
- traversal check inputs/results
- chooser inputs/outputs

This gives a clean read-only view of environment-triggered motion changes without touching montage logic.

### Pose Search Assets

Observe but do not mutate:

- pose search database chooser tables
- dense/sparse database assets
- motion-matching tag vocabulary

### Overlay / ALS Layering

Observe but do not mutate:

- overlay base selection
- overlay pose selection
- linked animation layer interfaces
- hand-held prop state

The current overlay system is a stable boundary and should remain untouched during Shadow Mode bootstrap.

## Recommended `GASPALSShadow` Attachment Model

### Runtime shape

- `UGASPALSShadowWorldSubsystem`
  - owns the active log session
  - writes `session.json`, `frames.jsonl`, and `events.jsonl` under `Saved/Logs/GASPALSShadow/<timestamp>/`
- `UGASPALSShadowObserverComponent`
  - attaches to the currently controlled sandbox character
  - snapshots owner movement, controller rotation, anim instance identity, active montage, and tagged metadata
  - exposes named metadata and extension-channel hooks so blueprint-side overlay/traversal/chooser state can be added later without rewriting the plugin API

### Rebinding behavior

- bind observer to the possessed pawn
- on character swap, rebind to the newly controlled pawn
- keep sampling read-only; no movement commands, no animation output writes

## Explicit No-Touch Boundaries for Phase 0-2

- no AnimBP main-graph replacement
- no new linked-layer insertion into the formal locomotion path
- no pose search database replacement
- no overlay selection override
- no traversal chooser override
- no formal locomotion output takeover

## Deliverable Outcome for This Phase

Shadow Mode now has a concrete insertion design:

- host observation starts at the sandbox character
- final animation observation happens at the sandbox AnimBP boundary
- logging is isolated under the new `GASPALSShadow` plugin
- future bridge code can add blueprint-fed overlay/traversal fields without destabilizing the existing runtime
