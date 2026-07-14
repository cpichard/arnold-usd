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

/// HdArnoldCoordSys represents a USD coordinate system binding as an Arnold
/// matrix_multiply_vector shader node whose matrix tracks the prim's world
/// transform.  Shaders can connect to its output to transform positions into
/// the named coordinate space.
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
    HdArnoldRenderDelegate* _renderDelegate;
    AtNode* _node = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
