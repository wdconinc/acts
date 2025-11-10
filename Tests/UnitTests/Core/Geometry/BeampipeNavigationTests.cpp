// This file is part of the ACTS project.
//
// Copyright (C) 2016 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <boost/test/unit_test.hpp>

#include "Acts/Definitions/Algebra.hpp"
#include "Acts/Definitions/Units.hpp"
#include "Acts/Geometry/CylinderVolumeBuilder.hpp"
#include "Acts/Geometry/CylinderVolumeHelper.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Geometry/LayerArrayCreator.hpp"
#include "Acts/Geometry/PassiveLayerBuilder.hpp"
#include "Acts/Geometry/TrackingGeometry.hpp"
#include "Acts/Geometry/TrackingGeometryBuilder.hpp"
#include "Acts/Geometry/TrackingVolumeArrayCreator.hpp"
#include "Acts/Material/HomogeneousSurfaceMaterial.hpp"
#include "Acts/Material/Material.hpp"
#include "Acts/Material/MaterialSlab.hpp"
#include "Acts/Propagator/Navigator.hpp"
#include "Acts/Utilities/Logger.hpp"

#include <memory>

using namespace Acts;
using namespace Acts::UnitLiterals;

namespace ActsTests {

BOOST_AUTO_TEST_SUITE(BeampipeNavigationSuite)

/// Test to verify current behavior when tracks propagate through beampipe ends
BOOST_AUTO_TEST_CASE(BeampipeEndNavigationTest) {
  GeometryContext gctx;

  // Create a simple beampipe geometry
  const double beamPipeRadius = 20_mm;
  const double beamPipeHalfZ = 100_mm;
  const double beamPipeThickness = 1_mm;

  // Material for beampipe
  Material beryllium = Material::fromMolarDensity(9.012, 4, 1.848_g / 1_cm3);
  MaterialSlab beamPipeMaterial(beryllium, beamPipeThickness);

  // Build the beampipe layer
  PassiveLayerBuilder::Config bplConfig;
  bplConfig.layerIdentification = "BeamPipe";
  bplConfig.centralLayerRadii = {beamPipeRadius};
  bplConfig.centralLayerHalflengthZ = {beamPipeHalfZ};
  bplConfig.centralLayerThickness = {beamPipeThickness};
  bplConfig.centralLayerMaterial = {
      std::make_shared<const HomogeneousSurfaceMaterial>(beamPipeMaterial)};
  
  auto beamPipeBuilder = std::make_shared<const PassiveLayerBuilder>(
      bplConfig, getDefaultLogger("BeamPipeLayerBuilder", Logging::INFO));

  // Create cylindervolumehelper
  LayerArrayCreator::Config lacConfig;
  auto layerArrayCreator = std::make_shared<const LayerArrayCreator>(
      lacConfig, getDefaultLogger("LayerArrayCreator", Logging::INFO));

  TrackingVolumeArrayCreator::Config tvacConfig;
  auto trackingVolumeArrayCreator =
      std::make_shared<const TrackingVolumeArrayCreator>(
          tvacConfig, getDefaultLogger("TrackingVolumeArrayCreator", Logging::INFO));

  CylinderVolumeHelper::Config cvhConfig;
  cvhConfig.layerArrayCreator = layerArrayCreator;
  cvhConfig.trackingVolumeArrayCreator = trackingVolumeArrayCreator;
  auto cylinderVolumeHelper = std::make_shared<const CylinderVolumeHelper>(
      cvhConfig, getDefaultLogger("CylinderVolumeHelper", Logging::INFO));

  // Build the beampipe volume
  CylinderVolumeBuilder::Config bpvConfig;
  bpvConfig.trackingVolumeHelper = cylinderVolumeHelper;
  bpvConfig.volumeName = "BeamPipe";
  bpvConfig.layerBuilder = beamPipeBuilder;
  bpvConfig.layerEnvelopeR = {1_mm, 1_mm};
  bpvConfig.buildToRadiusZero = true;  // This makes it a beampipe
  
  auto beamPipeVolumeBuilder = std::make_shared<const CylinderVolumeBuilder>(
      bpvConfig, getDefaultLogger("BeamPipeVolumeBuilder", Logging::INFO));

  // Create the bounds for the beampipe volume
  auto beamPipeBounds = std::make_shared<const CylinderVolumeBounds>(
      0., beamPipeRadius + 5_mm, beamPipeHalfZ + 10_mm);

  // Build the tracking volume
  auto beamPipeVolume = beamPipeVolumeBuilder->trackingVolume(
      gctx, nullptr, beamPipeBounds);

  BOOST_CHECK_NE(beamPipeVolume, nullptr);

  // Build tracking geometry
  TrackingGeometryBuilder::Config tgbConfig;
  tgbConfig.trackingVolumeHelper = cylinderVolumeHelper;
  tgbConfig.trackingVolumeBuilders = {
      [beamPipeVolumeBuilder](const GeometryContext& vgctx,
                              const std::shared_ptr<const TrackingVolume>& inner,
                              const std::shared_ptr<const VolumeBounds>&) {
        return beamPipeVolumeBuilder->trackingVolume(vgctx, inner);
      }};

  auto trackingGeometryBuilder =
      std::make_shared<const TrackingGeometryBuilder>(tgbConfig,
                                                      getDefaultLogger("TrackingGeometryBuilder", Logging::INFO));
  
  auto trackingGeometry = trackingGeometryBuilder->trackingGeometry(gctx);
  BOOST_CHECK_NE(trackingGeometry, nullptr);

  // Now test navigation behavior
  // Create a track starting at r=5mm, z=0, heading in +z direction
  Vector3 position(5_mm, 0., 0.);
  Vector3 direction(0., 0., 1.);  // Moving in +z direction

  Navigator::Config navConfig;
  navConfig.trackingGeometry = trackingGeometry;
  Navigator navigator(navConfig);

  Navigator::State navState;
  navigator.initialize(navState, position, direction);

  // Get current volume
  auto currentVolume = navState.currentVolume;
  BOOST_CHECK_NE(currentVolume, nullptr);
  
  // Log the current state
  std::cout << "Starting position: " << position.transpose() << std::endl;
  std::cout << "Starting direction: " << direction.transpose() << std::endl;
  std::cout << "Current volume: " << currentVolume->volumeName() << std::endl;
  std::cout << "Current volume bounds: r=[" 
            << dynamic_cast<const CylinderVolumeBounds&>(currentVolume->volumeBounds()).get(CylinderVolumeBounds::eMinR) 
            << ", "
            << dynamic_cast<const CylinderVolumeBounds&>(currentVolume->volumeBounds()).get(CylinderVolumeBounds::eMaxR)
            << "], z=[-"
            << dynamic_cast<const CylinderVolumeBounds&>(currentVolume->volumeBounds()).get(CylinderVolumeBounds::eHalfLengthZ)
            << ", "
            << dynamic_cast<const CylinderVolumeBounds&>(currentVolume->volumeBounds()).get(CylinderVolumeBounds::eHalfLengthZ)
            << "]" << std::endl;

  // Propagate towards +z end
  // The track should reach z = beamPipeHalfZ + 10mm (the volume boundary)
  // Question: Is there a surface at the z-end of the beampipe, or does it just exit?
  
  auto target = navigator.nextTarget(navState, position, direction);
  
  if (!target.isNone()) {
    std::cout << "Next target found" << std::endl;
    std::cout << "Target is boundary: " << target.isBoundary() << std::endl;
    std::cout << "Target is surface: " << target.isSurface() << std::endl;
    
    // Get intersection
    auto intersection = target.surface().intersect(gctx, position, direction).closestForward();
    std::cout << "Intersection path length: " << intersection.pathLength() << std::endl;
    std::cout << "Intersection position: " << intersection.position().transpose() << std::endl;
  } else {
    std::cout << "No target found - this indicates navigation issue" << std::endl;
  }

  // Key question: When track reaches z > beamPipeHalfZ, what happens?
  // - Does it hit a disc surface?
  // - Does it enter empty space?
  // - Does it leave the tracking geometry?
  
  BOOST_TEST_MESSAGE("Test demonstrates current beampipe end navigation behavior");
  BOOST_TEST_MESSAGE("A capped beampipe would provide a disc surface at the z-end");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace ActsTests
