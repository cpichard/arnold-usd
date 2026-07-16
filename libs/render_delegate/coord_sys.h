// Copyright 2024 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "api.h"

#include <ai.h>

#include <pxr/pxr.h>
#include <pxr/imaging/hd/coordSys.h>

#include "render_delegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdArnoldCamera;

/// HdArnoldCoordSys represents a USD coordinate system binding as an Arnold
/// camera node whose name matches the coordinate system name (GetName()) and
/// whose transform tracks the bound prim's world transform.
///
/// Arnold's OSL render services resolve named coordinate spaces such as
/// "map_proj.camera", "map_proj.NDC", "map_proj.screen" and "map_proj.raster"
/// by looking up a *camera* node named after the space (the portion before the
/// suffix) via AiNodeLookUpByName. Representing the coordinate system as a
/// camera named after GetName() is therefore what makes those lookups resolve.
///
/// When the coordinate system is bound to a camera prim (the common case, e.g.
/// a projection camera) we also mirror that camera's frustum (fov, screen
/// window, clipping) so that the projective ".NDC"/".screen"/".raster" spaces
/// match the bound camera.
class HdArnoldCoordSys : public HdCoordSys {
public:
    HDARNOLD_API
    HdArnoldCoordSys(HdArnoldRenderDelegate* renderDelegate, const SdfPath& id);

    HDARNOLD_API
    ~HdArnoldCoordSys() override;

    HDARNOLD_API
    void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

    AtNode* GetArnoldNode() const { return _node; }

    /// The camera node dedicated to the ".NDC" space, or nullptr when the NDC
    /// correction is disabled. Arnold's NDC convention is Y-opposite to its
    /// screen/raster, so (when HDARNOLD_coordsys_flip_ndc_v is enabled) the ".NDC"
    /// space is resolved through this separately-flipped camera while
    /// ".camera"/".screen"/".raster" keep using GetArnoldNode(). Callers rewrite
    /// the ".NDC"-suffixed material "space" inputs to this node's name.
    AtNode* GetArnoldNdcNode() const { return _ndcNode; }

private:
    /// Returns the HdArnoldCamera the coordinate system is bound to, or nullptr
    /// when the bound prim is not a camera (or cannot be resolved).
    const HdArnoldCamera* _FindBoundCamera(HdSceneDelegate* sceneDelegate) const;

    /// Mirror the resolved bound camera (matrix + frustum) into dst, optionally
    /// flipping the V axis, and register it for per-render aspect correction.
    void _MirrorCamera(AtNode* dst, AtNode* src, const HdArnoldCamera* boundCamera, bool flipV);

    /// Fall back to the coordinate system's own world transform (no frustum),
    /// optionally flipping the V axis. Used when no bound camera resolves.
    void _MirrorTransform(AtNode* dst, HdSceneDelegate* sceneDelegate, bool flipV);

    HdArnoldRenderDelegate* _renderDelegate;
    AtNode* _node = nullptr;
    AtNode* _ndcNode = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
