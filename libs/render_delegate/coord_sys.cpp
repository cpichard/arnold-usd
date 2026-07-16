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

#include "camera.h"
#include "config.h"
#include "render_param.h"
#include "utils.h"

#include <common_utils.h>
#include <constant_strings.h>

#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/sceneDelegate.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Flip the coordinate-system camera's up axis (row 1 of the camera-to-world
// matrix). This inverts camera-space Y, and because the projective spaces
// (.NDC/.screen/.raster) are derived from the camera, their V axis flips with
// it, so every named space flips consistently. Used to match Houdini/Karma's
// projection orientation when HDARNOLD_coordsys_flip_v is enabled.
void _FlipCoordSysMatrixV(AtNode* node)
{
    AtArray* matrix = AiNodeGetArray(node, str::matrix);
    if (matrix == nullptr)
        return;
    const uint32_t numKeys = AiArrayGetNumKeys(matrix);
    for (uint32_t key = 0; key < numKeys; ++key) {
        AtMatrix m = AiArrayGetMtx(matrix, key);
        m[1][0] = -m[1][0];
        m[1][1] = -m[1][1];
        m[1][2] = -m[1][2];
        m[1][3] = -m[1][3];
        AiArraySetMtx(matrix, key, m);
    }
}

} // namespace

HdArnoldCoordSys::HdArnoldCoordSys(HdArnoldRenderDelegate* renderDelegate, const SdfPath& id)
    : HdCoordSys(id), _renderDelegate(renderDelegate)
{
    // The Arnold node is created lazily in the first Sync(), once GetName() is
    // known: Arnold resolves named coordinate spaces by looking up a camera
    // node whose name matches the coordinate system name, so the node's name
    // matters and is only reliably available after the name has been synced.
}

HdArnoldCoordSys::~HdArnoldCoordSys()
{
    if (_node)
        _renderDelegate->DestroyArnoldNode(_node);
}

const HdArnoldCamera* HdArnoldCoordSys::_FindBoundCamera(HdSceneDelegate* sceneDelegate) const
{
    // When the coordinate system is populated through the scene index, the sprim
    // id identifies a coordSys prim parented under the bound prim itself. The
    // scene-delegate emulation reports it as a prim path with the property
    // separators rewritten, e.g. </grid/proj_cam/__coordSys_map_proj>, so the
    // bound camera is the *parent* path. Try both the prim path and its parent
    // and return the first that resolves to a camera. When populated through the
    // legacy delegate the id is a property path on the geometry carrying the
    // binding, where neither resolves to a camera and the caller falls back to
    // the coordinate system's own transform.
    const HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    for (const SdfPath& path : {GetId().GetPrimPath(), GetId().GetParentPath()}) {
        const HdSprim* sprim = renderIndex.GetSprim(HdPrimTypeTokens->camera, path);
        if (const auto* camera = dynamic_cast<const HdArnoldCamera*>(sprim))
            return camera;
    }
    return nullptr;
}

void HdArnoldCoordSys::Sync(
    HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    // Save bits before the parent clears them.
    const HdDirtyBits bits = *dirtyBits;

    // Parent syncs the name and resets dirty bits to Clean.
    HdCoordSys::Sync(sceneDelegate, renderParam, dirtyBits);

    HdArnoldRenderParamInterrupt param(renderParam);

    // Lazily create the camera node now that the coordinate system name is
    // known. Arnold's OSL render services resolve "<name>.camera/.NDC/.screen/
    // .raster" by looking up a camera node named <name>, so the node must carry
    // that name rather than the sprim's full SdfPath.
    if (_node == nullptr) {
        const TfToken name = GetName();
        if (name.IsEmpty())
            return;
        param.Interrupt();
        _node = _renderDelegate->CreateArnoldNode(str::persp_camera, AtString(name.GetText()));
    }

    // Cameras sync before coordSys sprims (see _SupportedSprimTypes), so when the
    // coordinate system resolves to a camera its Arnold node is already configured.
    const HdArnoldCamera* boundCamera = _FindBoundCamera(sceneDelegate);
    AtNode* src = boundCamera ? boundCamera->GetCamera() : nullptr;

    if (src != nullptr) {
        // The coordinate system is bound to a camera we can resolve. Mirror that
        // camera node directly: copy its world matrix (which already folds in the
        // parent procedural matrix) and, for the projective spaces, its frustum.
        //
        // Copying the matrix rather than reading the coordSys prim's own transform
        // via GetId() is deliberate: the scene-index population places the coordSys
        // prim under the camera prim (</cam.__coordSys:name>), so its transform gets
        // the camera transform applied a second time when flattened, whereas the
        // legacy scene delegate reports it directly. Mirroring the resolved camera
        // node is correct for both paths.
        param.Interrupt();
        if (AtArray* matrix = AiNodeGetArray(src, str::matrix)) {
            AiNodeSetArray(_node, str::matrix, AiArrayCopy(matrix));
            if (HdArnoldConfig::GetInstance().coordsys_flip_v)
                _FlipCoordSysMatrixV(_node);
        }
        // We create a persp_camera, so we can only mirror the frustum of a
        // perspective source. Orthographic projection cameras would need an
        // ortho_camera node and are left as a follow-up.
        if (AiNodeIs(src, str::persp_camera)) {
            AiNodeSetFlt(_node, str::fov, AiNodeGetFlt(src, str::fov));
            AtVector2 windowMin = AiNodeGetVec2(src, str::screen_window_min);
            AtVector2 windowMax = AiNodeGetVec2(src, str::screen_window_max);
            // Arnold derives a camera's vertical field of view from the *render*
            // frame aspect ratio (xres/yres * pixel_aspect), not from the camera's
            // own aperture (see AiWorldToScreenMatrix). For a projection camera that
            // must match the bound camera - as Karma does - we override the vertical
            // screen window so the projector's own aperture ratio is used, cancelling
            // the render aspect: yHalf = frameAspect * (vAperture / hAperture). When
            // the render aspect already equals the aperture aspect this is a no-op.
            const float hAperture = boundCamera->GetHorizontalAperture();
            const float vAperture = boundCamera->GetVerticalAperture();
            if (hAperture > AI_EPSILON && vAperture > AI_EPSILON) {
                const AtNode* options = AiUniverseGetOptions(_renderDelegate->GetUniverse());
                const int yres = AiNodeGetInt(options, str::yres);
                const float frameAspect = (yres != 0)
                    ? (static_cast<float>(AiNodeGetInt(options, str::xres)) / static_cast<float>(yres)) *
                        AiNodeGetFlt(options, str::pixel_aspect_ratio)
                    : 1.0f;
                const float yCenter = 0.5f * (windowMin.y + windowMax.y);
                const float yHalf = frameAspect * (vAperture / hAperture);
                windowMin.y = yCenter - yHalf;
                windowMax.y = yCenter + yHalf;
            }
            AiNodeSetVec2(_node, str::screen_window_min, windowMin.x, windowMin.y);
            AiNodeSetVec2(_node, str::screen_window_max, windowMax.x, windowMax.y);
            AiNodeSetFlt(_node, str::near_clip, AiNodeGetFlt(src, str::near_clip));
            AiNodeSetFlt(_node, str::far_clip, AiNodeGetFlt(src, str::far_clip));
        }
    } else if (bits & DirtyTransform) {
        // No resolvable camera (e.g. the legacy scene delegate exposes the
        // coordinate system on the geometry prim rather than the camera). Fall
        // back to the coordinate system's own world transform.
        param.Interrupt();
        HdArnoldSetTransform(_node, sceneDelegate, GetId());
        // Arnold does not apply the parent procedural matrix to cameras, so we
        // fold it in here, matching HdArnoldCamera.
        ArnoldUsdApplyParentMatrix(_node, _renderDelegate->GetProceduralParent());
        if (HdArnoldConfig::GetInstance().coordsys_flip_v)
            _FlipCoordSysMatrixV(_node);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
