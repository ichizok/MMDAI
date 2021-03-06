/**

 Copyright (c) 2010-2014  hkrn

 All rights reserved.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
 - Neither the name of the MMDAI project team nor the names of
   its contributors may be used to endorse or promote products
   derived from this software without specific prior written
   permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

*/

#include "vpvl2/vpvl2.h"
#include "vpvl2/internal/util.h"

#include "vpvl2/asset/Model.h"
#include "vpvl2/mvd/Motion.h"
#include "vpvl2/pmx/Model.h"
#include "vpvl2/mvd/BoneKeyframe.h"
#include "vpvl2/mvd/CameraKeyframe.h"
#include "vpvl2/mvd/EffectKeyframe.h"
#include "vpvl2/mvd/LightKeyframe.h"
#include "vpvl2/mvd/ModelKeyframe.h"
#include "vpvl2/mvd/MorphKeyframe.h"
#include "vpvl2/mvd/ProjectKeyframe.h"
#include "vpvl2/vmd/BoneKeyframe.h"
#include "vpvl2/vmd/CameraKeyframe.h"
#include "vpvl2/vmd/LightKeyframe.h"
#include "vpvl2/vmd/MorphKeyframe.h"
#include "vpvl2/vmd/Motion.h"

#ifdef VPVL2_LINK_VPVL
#include "vpvl2/pmd/Model.h"
#else
#include "vpvl2/pmd2/Model.h"
#endif

namespace {

using namespace vpvl2::VPVL2_VERSION_NS;

class NullBone : public IBone {
public:
    static inline IBone *sharedReference() {
        static NullBone bone;
        return &bone;
    }

    const IString *name(IEncoding::LanguageType /* type */) const { return 0; }
    void setName(const IString * /* value */, IEncoding::LanguageType /* type */) {}
    int index() const { return -1; }
    IModel *parentModelRef() const { return 0; }
    IBone *parentBoneRef() const { return 0; }
    Transform worldTransform() const {  return Transform::getIdentity(); }
    Transform localTransform() const {  return Transform::getIdentity(); }
    void getLocalTransform(Transform &world2LocalTransform) const {
        world2LocalTransform = Transform::getIdentity();
    }
    void setLocalTransform(const Transform & /* value */) {}
    Vector3 origin() const { return kZeroV3; }
    Vector3 destinationOrigin() const { return kZeroV3; }
    Vector3 localTranslation() const { return kZeroV3; }
    Quaternion localOrientation() const { return Quaternion::getIdentity(); }
    void getEffectorBones(Array<IBone *> & /* value */) const {}
    void setLocalTranslation(const Vector3 & /* value */) {}
    void setLocalOrientation(const Quaternion & /* value */) {}
    bool isMovable() const { return false; }
    bool isRotateable() const { return false; }
    bool isVisible() const { return false; }
    bool isInteractive() const { return false; }
    bool hasInverseKinematics() const { return false; }
    bool hasFixedAxes() const { return false; }
    bool hasLocalAxes() const { return false; }
    Vector3 fixedAxis() const { return kZeroV3; }
    void getLocalAxes(Matrix3x3 & /* value */) const {}
    void setInverseKinematicsEnable(bool /* value */) {}
    bool isInverseKinematicsEnabled() const { return false; }
    bool isInherentTranslationEnabled() const { return false; }
    bool isInherentOrientationEnabled() const { return false; }
    float32 inherentCoefficient() const { return 1.0f; }
    void setInherentCoefficient(float32 /* value */) {}
    IBone *parentInherentBoneRef() const { return 0; }
    IBone *destinationOriginBoneRef() const { return 0; }

    void setParentBoneRef(IBone * /* value */) {}
    void setParentInherentBoneRef(IBone * /* value */) {}
    void setDestinationOriginBoneRef(IBone * /* value */) {}
    void setOrigin(const Vector3 & /* value */) {}
    void setDestinationOrigin(const Vector3 & /* value */) {}
    void setFixedAxis(const Vector3 & /* value */) {}
    void setAxisX(const Vector3 & /* value */) {}
    void setAxisZ(const Vector3 & /* value */) {}
    void setIndex(int /* value */) {}
    void setLayerIndex(int /* value */) {}
    void setExternalIndex(int /* value */) {}
    void setRotateable(bool /* value */) {}
    void setMovable(bool /* value */) {}
    void setVisible(bool /* value */) {}
    void setInteractive(bool /* value */) {}
    void setInherentOrientationEnable(bool /* value */) {}
    void setInherentTranslationEnable(bool /* value */) {}
    void setFixedAxisEnable(bool /* value */) {}
    void setLocalAxesEnable(bool /* value */) {}
    void setTransformAfterPhysicsEnable(bool /* value */) {}
    void setTransformedByExternalParentEnable(bool /* value */) {}

private:
    NullBone() {}
    ~NullBone() {}
};

class NullMaterial : public IMaterial {
public:
    static const Color kWhiteColor;
    static inline IMaterial *sharedReference() {
        static NullMaterial material;
        return &material;
    }

    IModel *parentModelRef() const { return 0; }
    const IString *name(IEncoding::LanguageType /* type */) const { return 0; }
    const IString *englishName() const { return 0; }
    const IString *userDataArea() const { return 0; }
    const IString *mainTexture() const { return 0; }
    const IString *sphereTexture() const { return 0; }
    const IString *toonTexture() const { return 0; }
    SphereTextureRenderMode sphereTextureRenderMode() const { return kNone; }
    Color ambient() const { return kZeroC; }
    Color diffuse() const { return kZeroC; }
    Color specular() const { return kZeroC; }
    Color edgeColor() const { return kZeroC; }
    Color mainTextureBlend() const { return kWhiteColor; }
    Color sphereTextureBlend() const { return kWhiteColor; }
    Color toonTextureBlend() const { return kWhiteColor; }
    IndexRange indexRange() const { return IndexRange(); }
    float32 shininess() const { return 0; }
    IVertex::EdgeSizePrecision edgeSize() const { return 1; }
    int index() const { return -1; }
    int mainTextureIndex() const { return -1; }
    int sphereTextureIndex() const { return -1; }
    int toonTextureIndex() const { return -1; }
    int sizeofIndices() const { return 0; }
    bool isSharedToonTextureUsed() const { return false; }
    bool isCullingDisabled() const { return true; }
    bool isCastingShadowEnabled() const { return false; }
    bool isCastingShadowMapEnabled() const { return false; }
    bool isShadowMapEnabled() const { return false; }
    bool isEdgeEnabled() const { return false; }
    bool isVertexColorEnabled() const { return false; }
    bool isVisible() const { return false; }

    void setName(const IString * /* value */, IEncoding::LanguageType /* type */) {}
    void setUserDataArea(const IString * /* value */) {}
    void setMainTexture(const IString * /* value */) {}
    void setSphereTexture(const IString * /* value */) {}
    void setToonTexture(const IString * /* value */) {}
    void setSphereTextureRenderMode(SphereTextureRenderMode /* value */) {}
    void setAmbient(const Color & /* value */) {}
    void setDiffuse(const Color & /* value */) {}
    void setSpecular(const Color & /* value */) {}
    void setEdgeColor(const Color & /* value */) {}
    void setIndexRange(const IndexRange & /* value */) {}
    void setShininess(float32 /* value */) {}
    void setEdgeSize(const IVertex::EdgeSizePrecision & /* value */) {}
    void setMainTextureIndex(int /* value */) {}
    void setSphereTextureIndex(int /* value */) {}
    void setToonTextureIndex(int /* value */) {}
    void setIndices(int /* value */) {}
    void setSharedToonTextureUsed(bool /* value */) {}
    void setCullingDisabled(bool /* value */) {}
    void setCastingShadowEnabled(bool /* value */) {}
    void setCastingShadowMapEnabled(bool /* value */) {}
    void setShadowMapEnabled(bool /* value */) {}
    void setEdgeEnabled(bool /* value */) {}
    void setVertexColorEnabled(bool /* value */) {}
    void setFlags(int /* value */) {}
    void setVisible(bool /* value */) {}

private:
    NullMaterial() {}
    ~NullMaterial() {}
};
const Color NullMaterial::kWhiteColor = Color(1, 1, 1, 1);

}

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{

struct Factory::PrivateContext
{
    PrivateContext(IEncoding *encodingRef, IProgressReporter *progressReporter)
        : encodingRef(encodingRef),
          progressReporterRef(progressReporter),
          motionPtr(0),
          mvdPtr(0),
          mvdBoneKeyframe(0),
          mvdCameraKeyframe(0),
          mvdLightKeyframe(0),
          mvdMorphKeyframe(0),
          vmdPtr(0),
          vmdBoneKeyframe(0),
          vmdCameraKeyframe(0),
          vmdLightKeyframe(0),
          vmdMorphKeyframe(0)
    {
    }
    ~PrivateContext() {
        internal::deleteObject(motionPtr);
        internal::deleteObject(mvdPtr);
        internal::deleteObject(mvdBoneKeyframe);
        internal::deleteObject(mvdCameraKeyframe);
        internal::deleteObject(mvdLightKeyframe);
        internal::deleteObject(mvdMorphKeyframe);
        internal::deleteObject(vmdPtr);
        internal::deleteObject(vmdBoneKeyframe);
        internal::deleteObject(vmdCameraKeyframe);
        internal::deleteObject(vmdLightKeyframe);
        internal::deleteObject(vmdMorphKeyframe);
    }

    mvd::Motion *createMVDFromVMD(vmd::Motion *source) const {
        mvd::Motion *motion = mvdPtr = new mvd::Motion(source->parentModelRef(), encodingRef);
        const int nBoneKeyframes = source->countKeyframes(IKeyframe::kBoneKeyframe);
        QuadWord value;
        Array<IKeyframe *> boneKeyframes, cameraKeyframes, lightKeyframes, morphKeyframes;
        boneKeyframes.reserve(nBoneKeyframes);
        for (int i = 0; i < nBoneKeyframes; i++) {
            mvd::BoneKeyframe *keyframeTo = mvdBoneKeyframe = new mvd::BoneKeyframe(motion);
            const IBoneKeyframe *keyframeFrom = source->findBoneKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setName(keyframeFrom->name());
            keyframeTo->setLocalTranslation(keyframeFrom->localTranslation());
            keyframeTo->setLocalOrientation(keyframeFrom->localOrientation());
            keyframeTo->setDefaultInterpolationParameter();
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionX, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionX, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionY, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionY, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionZ, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionZ, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBoneRotation, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBoneRotation, value);
            boneKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(boneKeyframes, IKeyframe::kBoneKeyframe);
        const int nCameraKeyframes = source->countKeyframes(IKeyframe::kCameraKeyframe);
        cameraKeyframes.resize(nCameraKeyframes);
        for (int i = 0; i < nCameraKeyframes; i++) {
            mvd::CameraKeyframe *keyframeTo = mvdCameraKeyframe = new mvd::CameraKeyframe(motion);
            const ICameraKeyframe *keyframeFrom = source->findCameraKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setLookAt(keyframeFrom->lookAt());
            keyframeTo->setAngle(keyframeFrom->angle());
            keyframeTo->setFov(keyframeFrom->fov());
            keyframeTo->setDistance(keyframeFrom->distance());
            keyframeTo->setPerspective(keyframeFrom->isPerspective());
            keyframeTo->setDefaultInterpolationParameter();
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraLookAtX, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraLookAtX, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraAngle, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraAngle, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraFov, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraFov, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraDistance, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraDistance, value);
            cameraKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(cameraKeyframes, IKeyframe::kCameraKeyframe);
        const int nLightKeyframes = source->countKeyframes(IKeyframe::kLightKeyframe);
        lightKeyframes.reserve(nLightKeyframes);
        for (int i = 0; i < nLightKeyframes; i++) {
            mvd::LightKeyframe *keyframeTo = mvdLightKeyframe = new mvd::LightKeyframe(motion);
            const ILightKeyframe *keyframeFrom = source->findLightKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setColor(keyframeFrom->color());
            keyframeTo->setDirection(keyframeFrom->direction());
            keyframeTo->setEnable(true);
            lightKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(lightKeyframes, IKeyframe::kLightKeyframe);
        const int nMorphKeyframes = source->countKeyframes(IKeyframe::kMorphKeyframe);
        morphKeyframes.reserve(nMorphKeyframes);
        for (int i = 0; i < nMorphKeyframes; i++) {
            mvd::MorphKeyframe *keyframeTo = mvdMorphKeyframe = new mvd::MorphKeyframe(motion);
            const IMorphKeyframe *keyframeFrom = source->findMorphKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setName(keyframeFrom->name());
            keyframeTo->setWeight(keyframeFrom->weight());
            keyframeTo->setDefaultInterpolationParameter();
            morphKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(morphKeyframes, IKeyframe::kMorphKeyframe);
        mvdBoneKeyframe = 0;
        mvdCameraKeyframe = 0;
        mvdLightKeyframe = 0;
        mvdMorphKeyframe = 0;
        mvdPtr = 0;
        return motion;
    }
    vmd::Motion *createVMDFromMVD(mvd::Motion *source) const {
        vmd::Motion *motion = vmdPtr = new vmd::Motion(source->parentModelRef(), encodingRef);
        const int nBoneKeyframes = source->countKeyframes(IKeyframe::kBoneKeyframe);
        QuadWord value;
        Array<IKeyframe *> boneKeyframes, cameraKeyframes, lightKeyframes, morphKeyframes;
        boneKeyframes.reserve(nBoneKeyframes);
        for (int i = 0; i < nBoneKeyframes; i++) {
            vmd::BoneKeyframe *keyframeTo = vmdBoneKeyframe = new vmd::BoneKeyframe(encodingRef);
            const IBoneKeyframe *keyframeFrom = source->findBoneKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setName(keyframeFrom->name());
            keyframeTo->setLocalTranslation(keyframeFrom->localTranslation());
            keyframeTo->setLocalOrientation(keyframeFrom->localOrientation());
            keyframeTo->setDefaultInterpolationParameter();
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionX, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionX, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionY, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionY, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBonePositionZ, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBonePositionZ, value);
            keyframeFrom->getInterpolationParameter(IBoneKeyframe::kBoneRotation, value);
            keyframeTo->setInterpolationParameter(IBoneKeyframe::kBoneRotation, value);
            boneKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(boneKeyframes, IKeyframe::kBoneKeyframe);
        const int nCameraKeyframes = source->countKeyframes(IKeyframe::kCameraKeyframe);
        cameraKeyframes.reserve(nCameraKeyframes);
        for (int i = 0; i < nCameraKeyframes; i++) {
            vmd::CameraKeyframe *keyframeTo = vmdCameraKeyframe = new vmd::CameraKeyframe();
            const ICameraKeyframe *keyframeFrom = source->findCameraKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setLookAt(keyframeFrom->lookAt());
            keyframeTo->setAngle(keyframeFrom->angle());
            keyframeTo->setFov(keyframeFrom->fov());
            keyframeTo->setDistance(keyframeFrom->distance());
            keyframeTo->setPerspective(keyframeFrom->isPerspective());
            keyframeTo->setDefaultInterpolationParameter();
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraLookAtX, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraLookAtX, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraLookAtY, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraLookAtZ, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraAngle, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraAngle, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraFov, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraFov, value);
            keyframeFrom->getInterpolationParameter(ICameraKeyframe::kCameraDistance, value);
            keyframeTo->setInterpolationParameter(ICameraKeyframe::kCameraDistance, value);
            cameraKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(cameraKeyframes, IKeyframe::kCameraKeyframe);
        const int nLightKeyframes = source->countKeyframes(IKeyframe::kLightKeyframe);
        lightKeyframes.reserve(nLightKeyframes);
        for (int i = 0; i < nLightKeyframes; i++) {
            vmd::LightKeyframe *keyframeTo = vmdLightKeyframe = new vmd::LightKeyframe();
            const ILightKeyframe *keyframeFrom = source->findLightKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setColor(keyframeFrom->color());
            keyframeTo->setDirection(keyframeFrom->direction());
            lightKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(lightKeyframes, IKeyframe::kLightKeyframe);
        /* TODO: interpolation */
        const int nMorphKeyframes = source->countKeyframes(IKeyframe::kMorphKeyframe);
        morphKeyframes.reserve(nMorphKeyframes);
        for (int i = 0; i < nMorphKeyframes; i++) {
            vmd::MorphKeyframe *keyframeTo = vmdMorphKeyframe = new vmd::MorphKeyframe(encodingRef);
            const IMorphKeyframe *keyframeFrom = source->findMorphKeyframeRefAt(i);
            keyframeTo->setTimeIndex(keyframeFrom->timeIndex());
            keyframeTo->setName(keyframeFrom->name());
            keyframeTo->setWeight(keyframeFrom->weight());
            morphKeyframes.append(keyframeTo);
        }
        motion->setAllKeyframes(morphKeyframes, IKeyframe::kMorphKeyframe);
        vmdBoneKeyframe = 0;
        vmdCameraKeyframe = 0;
        vmdLightKeyframe = 0;
        vmdMorphKeyframe = 0;
        vmdPtr = 0;
        return motion;
    }

    IEncoding *encodingRef;
    IProgressReporter *progressReporterRef;
    IMotion *motionPtr;
    mutable mvd::Motion *mvdPtr;
    mutable mvd::BoneKeyframe *mvdBoneKeyframe;
    mutable mvd::CameraKeyframe *mvdCameraKeyframe;
    mutable mvd::LightKeyframe *mvdLightKeyframe;
    mutable mvd::MorphKeyframe *mvdMorphKeyframe;
    mutable vmd::Motion *vmdPtr;
    mutable vmd::BoneKeyframe *vmdBoneKeyframe;
    mutable vmd::CameraKeyframe *vmdCameraKeyframe;
    mutable vmd::LightKeyframe *vmdLightKeyframe;
    mutable vmd::MorphKeyframe *vmdMorphKeyframe;
};

IModel::Type Factory::findModelType(const uint8 *data, vsize size)
{
    if (size >= 4 && internal::memcmp(data, "PMX ", 4) == 0) {
        return IModel::kPMXModel;
    }
    else if (size >= 3 && internal::memcmp(data, "Pmd", 3) == 0) {
        return IModel::kPMDModel;
    }
    else {
        return IModel::kAssetModel;
    }
}

IMotion::FormatType Factory::findMotionType(const uint8 *data, vsize size)
{
    if (size >= sizeof(vmd::Motion::kSignature) &&
            internal::memcmp(data, vmd::Motion::kSignature, sizeof(vmd::Motion::kSignature) - 1) == 0) {
        return IMotion::kVMDFormat;
    }
    else if (size >= sizeof(mvd::Motion::kSignature) &&
             internal::memcmp(data, mvd::Motion::kSignature, sizeof(mvd::Motion::kSignature) - 1) == 0) {
        return IMotion::kMVDFormat;
    }
    else {
        return IMotion::kUnknownFormat;
    }
}

IBone *Factory::sharedNullBoneRef()
{
    return NullBone::sharedReference();
}

IMaterial *Factory::sharedNullMaterialRef()
{
    return NullMaterial::sharedReference();
}

Factory::Factory(IEncoding *encoding, IProgressReporter *progressReporter)
    : m_context(new PrivateContext(encoding, progressReporter))
{
}

Factory::~Factory()
{
    internal::deleteObject(m_context);
}

IModel *Factory::newModel(IModel::Type type) const
{
    IModel *modelRef = 0;
    switch (type) {
    case IModel::kAssetModel:
        modelRef = new asset::Model(m_context->encodingRef);
        break;
    case IModel::kPMDModel:
#ifdef VPVL2_LINK_VPVL
        modelRef = new pmd::Model(m_context->encoding);
#else
        modelRef = new pmd2::Model(m_context->encodingRef);
#endif
        break;
    case IModel::kPMXModel:
        modelRef = new pmx::Model(m_context->encodingRef);
        break;
    default:
        break;
    }
    if (m_context->progressReporterRef) {
        modelRef->setProgressReporterRef(m_context->progressReporterRef);
    }
    return modelRef;
}

IModel *Factory::createModel(const uint8 *data, vsize size, bool &ok) const
{
    IModel *model = newModel(findModelType(data, size));
    ok = model ? model->load(data, size) : false;
    return model;
}

IMotion *Factory::newMotion(IMotion::FormatType type, IModel *modelRef) const
{
    switch (type) {
    case IMotion::kVMDFormat:
        return new vmd::Motion(modelRef, m_context->encodingRef);
    case IMotion::kMVDFormat:
        return new mvd::Motion(modelRef, m_context->encodingRef);
    default:
        return 0;
    }
}

IMotion *Factory::createMotion(const uint8 *data, vsize size, IModel *model, bool &ok) const
{
    IMotion *motion = newMotion(findMotionType(data, size), model);
    ok = motion ? motion->load(data, size) : false;
    return motion;
}

IBoneKeyframe *Factory::createBoneKeyframe(IMotion *motion) const
{
    return motion ? motion->createBoneKeyframe() : 0;
}

ICameraKeyframe *Factory::createCameraKeyframe(IMotion *motion) const
{
    return motion ? motion->createCameraKeyframe() : 0;
}

IEffectKeyframe *Factory::createEffectKeyframe(IMotion *motion) const
{
    return motion ? motion->createEffectKeyframe() : 0;
}

ILightKeyframe *Factory::createLightKeyframe(IMotion *motion) const
{
    return motion ? motion->createLightKeyframe() : 0;
}

IModelKeyframe *Factory::createModelKeyframe(IMotion *motion) const
{
    return motion ? motion->createModelKeyframe() : 0;
}

IMorphKeyframe *Factory::createMorphKeyframe(IMotion *motion) const
{
    return motion ? motion->createMorphKeyframe() : 0;
}

IProjectKeyframe *Factory::createProjectKeyframe(IMotion *motion) const
{
    return motion ? motion->createProjectKeyframe() : 0;
}

IMotion *Factory::convertMotion(IMotion *source, IMotion::FormatType destType) const
{
    if (source) {
        IMotion::FormatType sourceType = source->type();
        if (sourceType == destType) {
            return source->clone();
        }
        else if (sourceType == IMotion::kVMDFormat && destType == IMotion::kMVDFormat) {
            return m_context->createMVDFromVMD(static_cast<vmd::Motion *>(source));
        }
        else if (sourceType == IMotion::kMVDFormat && destType == IMotion::kVMDFormat) {
            return m_context->createVMDFromMVD(static_cast<mvd::Motion *>(source));
        }
    }
    return 0;
}

} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */
