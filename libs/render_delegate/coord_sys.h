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

private:
    /// Returns the HdArnoldCamera the coordinate system is bound to, or nullptr
    /// when the bound prim is not a camera (or cannot be resolved).
    const HdArnoldCamera* _FindBoundCamera(HdSceneDelegate* sceneDelegate) const;

    HdArnoldRenderDelegate* _renderDelegate;
    AtNode* _node = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
