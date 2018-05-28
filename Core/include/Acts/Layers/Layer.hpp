// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 ACTS project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// Layer.h, Acts project
///////////////////////////////////////////////////////////////////

#pragma once

// Core module
#include <map>
#include "Acts/EventData/NeutralParameters.hpp"
#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/Surfaces/SurfaceArray.hpp"
#include "Acts/Utilities/ApproachDescriptor.hpp"
#include "Acts/Utilities/BinnedArray.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/GeometryObject.hpp"
#include "Acts/Utilities/Intersection.hpp"
#include "Acts/Volumes/AbstractVolume.hpp"

namespace Acts {

class Surface;
class SurfaceMaterial;
class MaterialProperties;
class BinUtility;
class Volume;
class VolumeBounds;
class TrackingVolume;
class DetachedTrackingVolume;
class ApproachDescriptor;

// Simple surface intersection
typedef ObjectIntersection<Surface> SurfaceIntersection;

// master typedef
class Layer;
typedef std::shared_ptr<const Layer> LayerPtr;
typedef std::shared_ptr<Layer>       MutableLayerPtr;
typedef std::pair<const Layer*, const Layer*> NextLayers;

/// @enum LayerType
///
/// For code readability, it distinguishes between different
/// type of layers, which steers the behaviour in the navigation
enum LayerType { navigation = -1, passive = 0, active = 1 };

/// @class Layer
///
/// Base Class for a Detector Layer in the Tracking Geometry
///
/// An actual implemented Detector Layer inheriting from this base
/// class has to inherit from a specific type of Surface as well.
/// In addition, a Layer can carry:
///
/// A SurfaceArray of Surfaces holding the actual detector elements or
/// subSurfaces.
/// - SurfaceMaterial for Surface-based materialUpdates
/// - a pointer to the TrackingVolume (can only be set by such)
/// - an active/passive code :
/// 0      - activ
/// 1      - passive
/// [....] - other
///
/// The search type for compatible surfaces on a layer is
///   [ the higher the number, the faster ]:
/// --------- Layer internal ------------------------------------------------
/// -1     - provide all intersection tested without boundary check
///  0     - provide all intersection tested with boundary check
///  1     - provide overlap descriptor based without boundary check
///  2     - provide overlap descriptor based with boundary check
///
/// A layer can have substructure regarding:
/// - m_ssRepresentingSurface -> always exists (1), can have material (2)
/// - m_ssSensitiveSurfaces   -> can or not exist (0,1), can have material (2)
/// - m_ssApproachSurfaces    -> can or not exist (0,1) cam have material (2)
///
class Layer : public virtual GeometryObject
{
  /// Declare the TrackingVolume as a friend, to be able to register previous,
  /// next and set the enclosing TrackingVolume
  friend class TrackingVolume;

  /// Declare the DetachedTrackingVolume as a friend to be able to register it
  friend class DetachedTrackingVolume;

public:
  /// Default Constructor - deleted
  Layer() = delete;

  /// Copy Constructor - deleted
  Layer(const Layer&) = delete;

  /// Destructor
  virtual ~Layer();

  /// Assignment operator - forbidden, layer assignment must not be ambiguous
  ///
  /// @param lay is the source layer for assignment
  Layer&
  operator=(const Layer&)
      = delete;

  /// Return the entire SurfaceArray, returns a nullptr if no SurfaceArray
  const SurfaceArray*
  surfaceArray() const;

  /// Non-const version
  SurfaceArray*
  surfaceArray();

  /// Transforms the layer into a Surface representation for extrapolation
  /// @note the layer can be hosting many surfaces, but this is the global
  /// one to which one can extrapolate
  virtual const Surface&
  surfaceRepresentation() const = 0;

  // Non-const version
  virtual Surface&
  surfaceRepresentation()
      = 0;

  /// Return the Thickness of the Layer
  /// this is by definition along the normal vector of the surfaceRepresentation
  double
  thickness() const;

  /// templated onLayer() method
  ///
  /// @param parameters are the templated (charged/neutral) on layer check
  /// @param bcheck is the boundary check directive
  ///
  /// @return boolean that indicates success of the operation
  template <typename parameters_t>
  bool
  onLayer(const parameters_t&  parameters,
          const BoundaryCheck& bcheck = true) const;

  /// geometrical isOnLayer() method
  ///
  /// @note using isOnSurface() with Layer specific tolerance
  /// @param pos is the gobal position to be checked
  /// @param bcheck is the boundary check directive
  ///
  /// @return boolean that indicates success of the operation
  virtual bool
  isOnLayer(const Vector3D& pos, const BoundaryCheck& bcheck = true) const;

  /// Return method for the approach descriptor, can be nullptr
  const ApproachDescriptor*
  approachDescriptor() const;

  /// Non-const version of the approach descriptor
  ApproachDescriptor*
  approachDescriptor();

  /// Accept layer according to the following collection directives
  ///
  /// @tparam options_t Type of the options for navigation
  ///
  /// @return a boolean whether the layer is accepted for processing
  template <typename options_t>
  bool
  resolve(const options_t& options) const
  {
    return resolve(options.resolveSensitive,
                   options.resolveMaterial,
                   options.resolvePassive);
  }

  /// Accept layer according to the following collection directives
  ///
  /// @param resolveSensitive is the prescription to find the sensitive surfaces
  /// @param resolveMaterial is the precription to find material surfaces
  /// @param resolvePassive is the prescription to find all passive surfaces
  ///
  /// @return a boolean whether the layer is accepted for processing
  virtual bool
  resolve(bool resolveSensitive,
          bool resolveMaterial,
          bool resolvePassive) const;

  /// @brief Decompose Layer into (compatible) surfaces
  ///
  /// @tparam parameters_t The Track parameters  type
  /// @tparam options_t The navigation options type
  /// @tparam corrector_t is an (optional) intersection corrector
  ///
  /// @param parameters The templated parameters for searching
  /// @param options The templated naivation options
  ///
  /// @return list of intersection of surfaces on the layer
  template <typename parameters_t,
            typename options_t,
            typename corrector_t = VoidCorrector>
  std::vector<SurfaceIntersection>
  compatibleSurfaces(const parameters_t& parameters,
                     const options_t&    options,
                     const corrector_t&  corrfnc = corrector_t()) const;

  /// Surface seen on approach
  ///
  /// @tparam parameters_t The Track parameters  type
  /// @tparam options_t The navigation options type
  /// @tparam corrector_t is an (optional) intersection corrector
  ///
  /// for layers without sub structure, this is the surfaceRepresentation
  /// for layers with sub structure, this is the approachSurface
  /// @param parameters The templated parameters for searching
  /// @param options The templated naivation options
  ///
  /// @return the Surface intersection of the approach surface
  template <typename parameters_t,
            typename options_t,
            typename corrector_t = VoidCorrector>
  const SurfaceIntersection
  surfaceOnApproach(const parameters_t& parameters,
                    const options_t&    options,
                    const corrector_t&  corrfnc = corrector_t()) const;

  // -------------- legacy method block: start
  // ------------------------------------

  /// LEGACY method for old ExtrapolationEngine
  /// --------- to be deprecated with release 0.07.00
  ///
  ///  get compatible surfaces starting from neutral parameters
  ///  returns the compatible surfaces with a straight line estimation
  ///  a future estimater is forseen
  ///
  ///  - if start/end surface are given, surfaces are provided in between
  ///    (start && end excluded)
  ///
  /// @param pars are the (charged) track parameters for the search
  /// @param pdir is the propagation direction prescription
  /// @param bcheck is the boundary check directive
  /// @param resolveSensitive is the prescription to find the sensitive surfaces
  /// @param resolveMaterial is the precription to find material surfaces
  /// @param resolvePassive is the prescription to find all passive surfaces
  /// @param searchType is the level of depth for the search
  /// @param startSurface is an (optional) start surface for the search:
  ///        excluded in return
  /// @param endSurface is an (optional) end surface for the search:
  ///        excluded in return
  /// @param ice is a (future) compatibility estimator
  ///
  /// @return the list of possible surface interactions,
  ///                  ordered according to intersection
  virtual std::vector<SurfaceIntersection>
  compatibleSurfaces(const TrackParameters& pars,
                     NavigationDirection    pdir,
                     const BoundaryCheck&   bcheck,
                     bool                   resolveSensitive,
                     bool                   resolveMaterial,
                     bool                   resolvePassive,
                     int                    searchType,
                     const Surface*         startSurface = nullptr,
                     const Surface*         endSurface   = nullptr) const;

  /// LEGACY method for old ExtrapolationEngine
  /// --------- to be deprecated with release 0.07.00
  ///
  ///  get compatible surfaces starting from neutral parameters
  ///  returns the compatible surfaces either with straight line estimation
  ///
  ///  - if start/end surface are given, surfaces are provided in between
  ///    (start && end excluded)
  ///
  /// @param pars are the (charged) track parameters for the search
  /// @param pdir is the propagation direction prescription
  /// @param bcheck is the boundary check directive
  /// @param resolveSensitive is the prescription to find the sensitive surfaces
  /// @param resolveMaterial is the precription to find material surfaces
  /// @param resolvePassive is the prescription to find all passive surfaces
  /// @param searchType is the level of depth for the search
  /// @param startSurface is an (optional) start surface for the search:
  ///        excluded in return
  /// @param endSurface is an (optional) end surface for the search:
  ///        excluded in return
  ///
  /// @return the list of possible surface interactions,
  ///                  ordered according to intersection
  virtual std::vector<SurfaceIntersection>
  compatibleSurfaces(const NeutralParameters& pars,
                     NavigationDirection      pdir,
                     const BoundaryCheck&     bcheck,
                     bool                     resolveSensitive,
                     bool                     resolveMaterial,
                     bool                     resolvePassive,
                     int                      searchType,
                     const Surface*           startSurface = nullptr,
                     const Surface*           endSurface   = nullptr) const;

  /// LEGACY method for old ExtrapolationEngine
  /// --------- to be deprecated with release 0.07.00
  ///
  /// get compatible surfaces starting from the parameters
  ///
  /// @param pars are the (charged) track parameters for the search
  /// @param pdir is the propagation direction prescription
  /// @param bcheck is the boundary check directive
  /// @param resolveSensitive is the prescription to find the sensitive surfaces
  /// @param resolveMaterial is the prescription to find surfaces with material
  /// @param resolvePassive is the prescription to find all passive surfaces
  /// @param searchType is the level of depth for the search
  /// @param startSurface is optional for the search: excluded in return
  /// @param endSurface is optional for the search: excluded in return
  /// @param ice is a (future) compatibility estimator that could be used to
  /// modify the straight line approach
  ///
  /// @return the possible surface intersections
  template <typename parameters_t>
  std::vector<SurfaceIntersection>
  getCompatibleSurfaces(const parameters_t&  pars,
                        NavigationDirection  pdir,
                        const BoundaryCheck& bcheck,
                        bool                 resolveSensitive,
                        bool                 resolveMaterial,
                        bool                 resolvePassive,
                        int                  searchType,
                        const Surface*       startSurface = nullptr,
                        const Surface*       endSurface   = nullptr) const;

  /// LEGACY method for old ExtrapolationEngine
  /// --------- to be deprecated with release 0.07.00
  ///
  /// Surface seen on approach
  ///
  /// @tparam corrector_t is an (optional) intersection corrector
  ///
  /// for layers without sub structure, this is the surfaceRepresentation
  /// for layers with sub structure, this is the approachSurface
  /// @param gpos is the global position to start the approach from
  /// @param dir is the direction from where to attempt the approach
  /// @param pdir is the direction prescription
  /// @param bcheck is the boundary check directive
  /// @param resolveSensitive is the prescription to find the sensitive surfaces
  /// @param resolveMaterial is the precription to find material surfaces
  /// @param resolvePassive is the prescription to find all passive surfaces
  /// @param ice is a (future) compatibility estimator t
  /// the straight line approach
  virtual const SurfaceIntersection
  surfaceOnApproach(const Vector3D&      gpos,
                    const Vector3D&      dir,
                    NavigationDirection  pdir,
                    const BoundaryCheck& bcheck,
                    bool                 resolveSensitive,
                    bool                 resolveMaterial,
                    bool                 resolvePassive) const;

  /// LEGACY method for old ExtrapolationEngine
  /// --------- to be deprecated with release 0.07.00
  ///
  /// test compatible surface - checking directly for intersection & collection
  ///
  /// geometrical test compatible surface method
  /// @param cSurfaces are the retrun surface intersections
  /// @param surface is the parameter surface
  /// @todo how is this different from the start surface
  /// @param gpos is the resolved global position
  /// @param dir is themomentum direction
  /// @param pdir is the propagation direction prescription
  /// @param bcheck is the boundary check directive
  /// @param pathLimit is the maximal path length allowed to the surface
  /// @param intersectionTest is a boolean idicating if intersection is done
  /// @param startSurface is an (optional) start surface for the search:
  /// excluded in return
  /// @param endSurface is an (optional) end surface of search: excluded in
  /// return
  ///
  /// @return boolean that indicates if a compatible surface exists at all
  void
  testCompatibleSurface(std::vector<SurfaceIntersection>& cSurfaces,
                        const Surface&                    surface,
                        const Vector3D&                   gpos,
                        const Vector3D&                   dir,
                        NavigationDirection               pdir,
                        const BoundaryCheck&              bcheck,
                        double                            pathLimit) const;

  // -------------- legacy method block: end
  // ------------------------------------

  /// Fast navigation to next layer
  ///
  /// @param pos is the start position for the search
  /// @param dir is the direction for the search
  ///
  /// @return the pointer to the next layer
  const Layer*
  nextLayer(const Vector3D& pos, const Vector3D& dir) const;

  /// get the confining TrackingVolume
  ///
  /// @return the pointer to the enclosing volume
  const TrackingVolume*
  trackingVolume() const;

  /// get the confining DetachedTrackingVolume
  ///
  /// @return the pointer to the detached volume
  const DetachedTrackingVolume*
  enclosingDetachedTrackingVolume() const;

  /// register Volume associated to the layer
  /// - if you want to do that by hand: should be shared or unique ptr
  ///
  /// @param avol is the provided volume
  void
  registerRepresentingVolume(const AbstractVolume* avol);

  ///  return the abstract volume that represents the layer
  ///
  /// @return the representing volume of the layer
  const AbstractVolume*
  representingVolume() const;

  /// return the LayerType
  LayerType
  layerType() const;

  /// @return a map of all contained detector elements and their corresponding
  /// identifier
  const std::map<Identifier, const DetectorElementBase*>&
  detectorElements() const;

protected:
  /// Constructor with pointer to SurfaceArray (passing ownership)
  ///
  /// @param surfaceArray is the array of sensitive surfaces
  /// @param thickness is the normal thickness of the Layer
  /// @param ad oapproach descriptor
  /// @param ltype is the layer type if active or passive
  Layer(std::unique_ptr<SurfaceArray>       surfaceArray,
        double                              thickness = 0.,
        std::unique_ptr<ApproachDescriptor> ad        = nullptr,
        LayerType                           ltype     = passive);

  ///  private method to set enclosing TrackingVolume, called by friend class
  /// only
  ///  optionally, the layer can be resized to the dimensions of the
  /// TrackingVolume
  ///  - Bounds of the Surface are resized
  ///  - MaterialProperties dimensions are resized
  ///  - SubSurface array boundaries are NOT resized
  ///
  /// @param tvol is the tracking volume the layer is confined
  void
  encloseTrackingVolume(const TrackingVolume& tvol);

  ///  private method to set the enclosed detached TV,
  /// called by friend class only
  ///
  /// @param dtvol is the detached tracking volume the layer is confined
  void
  encloseDetachedTrackingVolume(const DetachedTrackingVolume& dtvol);

  /// the previous Layer according to BinGenUtils
  NextLayers m_nextLayers;

  /// A binutility to find the next layer
  /// @TODO check if this is needed
  const BinUtility* m_nextLayerUtility;

  /// SurfaceArray on this layer Surface
  ///
  /// This array will be modified during signature and constant afterwards, but
  /// the C++ type system unfortunately cannot cleanly express this.
  ///
  std::unique_ptr<const SurfaceArray> m_surfaceArray;

  /// thickness of the Layer
  double m_layerThickness;

  /// descriptor for surface on approach
  ///
  /// The descriptor may need to be modified during geometry building, and will
  /// remain constant afterwards, but again C++ cannot currently express this.
  ///
  std::unique_ptr<const ApproachDescriptor> m_approachDescriptor;

  /// the enclosing TrackingVolume
  const TrackingVolume* m_trackingVolume;

  /// the eventual enclosing detached Tracking volume
  const DetachedTrackingVolume* m_enclosingDetachedTrackingVolume;

  /// Representing Volume
  /// can be used as approach surface sources
  const AbstractVolume* m_representingVolume;

  /// make a passive/active either way
  LayerType m_layerType;

  /// sub structure indication
  int m_ssRepresentingSurface;
  int m_ssSensitiveSurfaces;
  int m_ssApproachSurfaces;

private:
  /// Private helper method to close the geometry
  /// - it will set the layer geometry ID for a unique identification
  /// - it will also register the internal sub strucutre
  /// @param layerID is the geometry id of the volume
  ///                as calculated by the TrackingGeometry
  void
  closeGeometry(const GeometryID& layerID);

  /// map which collects all detector elements and connects them with their
  /// Identfier
  std::map<Identifier, const DetectorElementBase*> m_detectorElements;
};

/// Layers are constructedd with shared_ptr factories, hence the layer array is
/// describes as:
typedef BinnedArray<LayerPtr> LayerArray;

}  // end of namespace

#include "Acts/Layers/detail/Layer.ipp"