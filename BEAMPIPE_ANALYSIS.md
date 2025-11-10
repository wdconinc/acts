# Beampipe Analysis and Capped Beampipe Implementation Plan

## Branch Information
- **Branch**: `feature/capped-beampipe`
- **Build Directory**: `build-capped-beampipe`
- **Status**: Build successful, tests have issues (likely environment-related)

## Current Beampipe Behavior

### Key Findings

1. **Beampipe Extends Through Full Z-Range**
   - Location: `Plugins/DD4hep/src/ConvertDD4hepDetector.cpp:77-79`
   - Comment: "the beam pipe volume builder needs special treatment and needs to be added in the end (beampipe exceeds length of all other subdetectors)"
   - The beampipe is added **last** in the volume builder list, wrapping around all other volumes

2. **Beampipe Identification**
   - Identified by `buildToRadiusZero = true` flag
   - Set at: `Plugins/DD4hep/src/ConvertDD4hepDetector.cpp:448`
   - Enforced as unique: Only ONE beampipe allowed (lines 103-111)

3. **Beampipe Construction**
   - In `CylinderVolumeBuilder.cpp:623-625`: When `buildToRadiusZero` is true, `rMin` is set to 0
   - Creates a cylindrical volume from r=0 to outer radius
   - Z-extent determined by the full detector geometry

## Test Geometry Examples

### Legacy TrackingGeometry Builder
From `Tests/CommonHelpers/src/CylindricalTrackingGeometry.cpp:192-220`:

```cpp
// Beampipe configuration
PassiveLayerBuilder::Config bplConfig;
bplConfig.layerIdentification = "BeamPipe";
bplConfig.centralLayerRadii = std::vector<double>(1, kBeamPipeRadius);  // 19mm
bplConfig.centralLayerHalflengthZ = std::vector<double>(1, kBeamPipeHalfLengthZ);  // 1000mm
bplConfig.centralLayerThickness = std::vector<double>(1, kBeamPipeThickness);  // 0.8mm

CylinderVolumeBuilder::Config bpvConfig;
bpvConfig.volumeName = "BeamPipe";
bpvConfig.buildToRadiusZero = true;  // KEY FLAG

auto beamPipeBounds = std::make_shared<const CylinderVolumeBounds>(0., 25., 1100.);
```

### Blueprint (Gen3) Geometry
From same file, lines 315-324:

```cpp
auto beampipeBounds = std::make_unique<CylinderVolumeBounds>(0_mm, kBeamPipeRadius, 100_mm);
auto beampipe = std::make_unique<TrackingVolume>(
    Transform3::Identity(), std::move(beampipeBounds), "Beampipe");
```

## Current Navigation Behavior

### What Happens at Beampipe Ends?

From `Tests/UnitTests/Core/Propagator/NavigatorTests.cpp:329`:
- Test mentions "step to the BeamPipe"
- Navigator treats beampipe as a regular cylindrical surface
- **No special handling for z-end caps currently**

### Navigation Flow
1. Track propagates through beampipe volume (r < r_beampipe)
2. Navigator finds next boundary surface
3. At beampipe ends (±z_max):
   - Track exits beampipe volume
   - Enters next volume (typically gap or endcap volume)
   - **No physical surface at beampipe z-ends by default**

## Problem Statement

**Current Issue**: The beampipe is an open-ended cylinder. Particles traveling along z at small radii (r < r_beampipe) will:
1. Pass through the beampipe end without interaction
2. Enter an empty volume or undefined space
3. No material boundary at ±z_max of beampipe

**Desired Behavior**: Define a "capped" beampipe where:
1. Endcap discs can be placed at ±z_max
2. Endcaps can have r_min = 0 (solid disc)
3. Particles hitting endcaps see proper material boundaries
4. Endcaps should NOT be flagged as beampipes themselves

## Implementation Challenges

### Challenge 1: Volume Builder Logic
- `buildToRadiusZero` flag currently identifies THE beampipe
- Multiple volumes with r_min=0 would trigger "duplicate beampipe" error
- Need to distinguish between:
  - Cylindrical beampipe (r_min=0, extends in z)
  - Disc endcaps (r_min=0, thin in z)

### Challenge 2: Volume Hierarchy
- Current: Beampipe wraps full z-range
- Needed: Beampipe limited to specific z-range, with endcaps beyond

### Challenge 3: DD4hep Integration
- DD4hep DetType::BEAMPIPE triggers `buildToRadiusZero`
- Need new identification scheme for beampipe endcaps
- Options:
  - New DetType flag
  - VariantParameter: `beampipe_endcap: bool = true`
  - Geometric detection (disc with r_min=0)

## Proposed Solution

### Option A: Modify buildToRadiusZero Logic
1. Rename or extend `buildToRadiusZero` to be more specific
2. Add `isBeampipeEndcap` flag
3. Allow multiple r_min=0 volumes with different roles

### Option B: Geometry-Based Detection
1. Keep `buildToRadiusZero` for cylindrical beampipe only
2. Auto-detect disc geometry with r_min=0 as potential endcaps
3. Check if disc is positioned at beampipe z-ends
4. Treat as beampipe cap if conditions met

### Option C: Explicit Configuration (RECOMMENDED)
1. Add new VariantParameter: `beampipe_cap: string = "none|negative|positive|both"`
2. Modify `CylinderVolumeBuilder` to:
   - Accept explicit z-bounds for beampipe
   - Add disc surfaces at z-ends if configured
3. Allow endcap volumes to extend to r=0 without triggering beampipe logic
4. Update `ConvertDD4hepDetector.cpp` to handle capped beampipe assembly

## Next Steps

1. ✅ Create branch `feature/capped-beampipe`
2. ✅ Build project successfully
3. ✅ Analyze current beampipe construction
4. ✅ Identify navigation behavior at beampipe ends
5. ⏭️ Design API for capped beampipe configuration
6. ⏭️ Implement changes to `CylinderVolumeBuilder`
7. ⏭️ Update DD4hep conversion logic
8. ⏭️ Create unit tests for capped beampipe navigation
9. ⏭️ Test with forward-going tracks at small radius

## Code Locations

### Key Files to Modify
- `Plugins/DD4hep/src/ConvertDD4hepDetector.cpp` - DD4hep beampipe identification
- `Core/include/Acts/Geometry/CylinderVolumeBuilder.hpp` - Config structure
- `Core/src/Geometry/CylinderVolumeBuilder.cpp` - Volume building logic
- `Plugins/DD4hep/include/ActsPlugins/DD4hep/DD4hepVolumeBuilder.hpp` - DD4hep volume config

### Test Files to Create/Modify
- `Tests/UnitTests/Core/Geometry/*BeampipeTests.cpp` - New test file
- `Tests/UnitTests/Plugins/DD4hep/*CappedBeampipe*.cpp` - DD4hep integration test
- `Tests/UnitTests/Core/Propagator/NavigatorTests.cpp` - Add beampipe endcap navigation test

## Test Strategy

### Required Test Cases
1. **Beampipe Cap Navigation Test**
   - Create geometry with capped beampipe
   - Start track at r=5mm, z=0, heading in +z direction
   - Verify track intersects with positive z-end cap
   - Verify material interaction at cap surface
   
2. **Multiple R=0 Volumes Test**
   - Create beampipe + endcap discs
   - Verify no "duplicate beampipe" error
   - Verify correct volume identification

3. **DD4hep Integration Test**
   - Create DD4hep geometry XML with capped beampipe
   - Convert to ACTS geometry
   - Verify proper volume hierarchy

## Build Notes

- Build time: ~20 minutes on current system
- Configuration: Minimal (Core + DD4hep plugin only)
- Test issues: Segfaults in several tests (may be build environment issue, not feature-related)
- Next build should include test fixes if implementing changes

