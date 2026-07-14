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

#include "coord_sys.h"

#include "render_param.h"
#include "utils.h"

#include <constant_strings.h>

PXR_NAMESPACE_OPEN_SCOPE

HdArnoldCoordSys::HdArnoldCoordSys(HdArnoldRenderDelegate* renderDelegate, const SdfPath& id)
    : HdCoordSys(id), _renderDelegate(renderDelegate)
{
    _node = renderDelegate->CreateArnoldNode(str::matrix_multiply_vector, AtString(id.GetText()));
}

HdArnoldCoordSys::~HdArnoldCoordSys()
{
    if (_node)
        _renderDelegate->DestroyArnoldNode(_node);
}

void HdArnoldCoordSys::Sync(
    HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    // Save bits before the parent clears them.
    const HdDirtyBits bits = *dirtyBits;

    // Parent syncs the name and resets dirty bits to Clean.
    HdCoordSys::Sync(sceneDelegate, renderParam, dirtyBits);

    if (bits & DirtyTransform) {
        HdArnoldRenderParamInterrupt param(renderParam);
        param.Interrupt();
        HdArnoldSetTransform(_node, sceneDelegate, GetId());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
