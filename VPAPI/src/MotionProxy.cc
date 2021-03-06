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

#include "BoneKeyframeRefObject.h"
#include "BoneMotionTrack.h"
#include "BoneRefObject.h"
#include "CameraKeyframeRefObject.h"
#include "CameraMotionTrack.h"
#include "CameraRefObject.h"
#include "LightKeyframeRefObject.h"
#include "LightMotionTrack.h"
#include "LightRefObject.h"
#include "ModelProxy.h"
#include "MorphKeyframeRefObject.h"
#include "MorphMotionTrack.h"
#include "MorphRefObject.h"
#include "MotionProxy.h"
#include "ProjectProxy.h"
#include "Util.h"

#include <cmath>
#include <QApplication>
#include <QtCore>
#include <QUndoStack>

#include <vpvl2/vpvl2.h>
#include <vpvl2/extensions/qt/String.h>

using namespace vpvl2;
using namespace vpvl2::extensions::qt;

namespace {

class BaseKeyframeCommand : public QUndoCommand {
public:
    BaseKeyframeCommand(const QList<BaseKeyframeRefObject *> &keyframeRefs, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_keyframeRefs(keyframeRefs)
    {
        Q_ASSERT(!m_keyframeRefs.isEmpty());
        foreach (BaseKeyframeRefObject *keyframe, keyframeRefs) {
            m_trackRefs.insert(keyframe->parentTrack());
        }
    }
    BaseKeyframeCommand(BaseKeyframeRefObject *keyframeRef, QUndoCommand *parent)
        : QUndoCommand(parent)
    {
        m_keyframeRefs.append(keyframeRef);
    }
    ~BaseKeyframeCommand() {
    }

protected:
    void addKeyframe() {
        bool doUpdate = m_trackRefs.isEmpty();
        foreach (BaseKeyframeRefObject *keyframeRef, m_keyframeRefs) {
            BaseMotionTrack *track = keyframeRef->parentTrack();
            track->addKeyframe(keyframeRef, doUpdate);
        }
    }
    void removeKeyframe() {
        bool doUpdate = m_trackRefs.isEmpty();
        foreach (BaseKeyframeRefObject *keyframeRef, m_keyframeRefs) {
            BaseMotionTrack *track = keyframeRef->parentTrack();
            track->removeKeyframe(keyframeRef, doUpdate);
        }
    }
    void refreshTrack() {
        if (!m_trackRefs.isEmpty()) {
            foreach (BaseMotionTrack *track, m_trackRefs) {
                track->refresh();
            }
        }
    }

    QList<BaseKeyframeRefObject *> m_keyframeRefs;
    QSet<BaseMotionTrack *> m_trackRefs;
};

class AddKeyframeCommand : public BaseKeyframeCommand {
public:
    AddKeyframeCommand(const QList<BaseKeyframeRefObject *> &keyframeRefs, QUndoCommand *parent)
        : BaseKeyframeCommand(keyframeRefs, parent)
    {
        setText(QApplication::tr("Register Keyframe(s)"));
    }
    AddKeyframeCommand(BaseKeyframeRefObject *keyframeRef, QUndoCommand *parent)
        : BaseKeyframeCommand(keyframeRef, parent)
    {
        setText(QApplication::tr("Register Keyframe(s)"));
    }
    ~AddKeyframeCommand() {
    }

    virtual void undo() {
        removeKeyframe();
        refreshTrack();
    }
    virtual void redo() {
        addKeyframe();
        refreshTrack();
    }
};

class RemoveKeyframeCommand : public BaseKeyframeCommand {
public:
    RemoveKeyframeCommand(const QList<BaseKeyframeRefObject *> &keyframeRefs, QUndoCommand *parent)
        : BaseKeyframeCommand(keyframeRefs, parent)
    {
        setText(QApplication::tr("Remove Keyframe(s)"));
    }
    RemoveKeyframeCommand(BaseKeyframeRefObject *keyframeRef, QUndoCommand *parent)
        : BaseKeyframeCommand(keyframeRef, parent)
    {
        setText(QApplication::tr("Remove Keyframe(s)"));
    }
    ~RemoveKeyframeCommand() {
    }

    virtual void undo() {
        addKeyframe();
        refreshTrack();
    }
    virtual void redo() {
        removeKeyframe();
        refreshTrack();
    }
};

class MergeKeyframeCommand : public QUndoCommand {
public:
    typedef QPair<BaseKeyframeRefObject *, BaseKeyframeRefObject *> Pair;

    MergeKeyframeCommand(const QList<Pair> &keyframes,
                         const quint64 &newTimeIndex,
                         const quint64 &oldTimeIndex,
                         QUndoCommand *parent)
        : QUndoCommand(parent),
          m_keyframes(keyframes),
          m_newTimeIndex(newTimeIndex),
          m_oldTimeIndex(oldTimeIndex)
    {
        setText(QApplication::tr("Merge Keyframe(s)"));
    }
    ~MergeKeyframeCommand() {
    }

    virtual void undo() {
        foreach (const Pair &pair, m_keyframes) {
            Q_ASSERT(pair.first);
            BaseKeyframeRefObject *destinationKeyframe = pair.first;
            destinationKeyframe->setTimeIndex(m_oldTimeIndex);
            destinationKeyframe->parentTrack()->replaceTimeIndex(m_oldTimeIndex, m_newTimeIndex);
            if (BaseKeyframeRefObject *sourceKeyframe = pair.second) {
                sourceKeyframe->parentTrack()->addKeyframe(sourceKeyframe, true);
            }
        }
    }
    virtual void redo() {
        foreach (const Pair &pair, m_keyframes) {
            Q_ASSERT(pair.first);
            if (BaseKeyframeRefObject *sourceKeyframe = pair.second) {
                sourceKeyframe->parentTrack()->removeKeyframe(sourceKeyframe, true);
            }
            BaseKeyframeRefObject *destionationKeyframe = pair.first;
            destionationKeyframe->setTimeIndex(m_newTimeIndex);
            destionationKeyframe->parentTrack()->replaceTimeIndex(m_newTimeIndex, m_oldTimeIndex);
        }
    }

private:
    const QList<Pair> m_keyframes;
    const quint64 m_newTimeIndex;
    const quint64 m_oldTimeIndex;
};

class UpdateBoneKeyframeCommand : public QUndoCommand {
public:
    UpdateBoneKeyframeCommand(const QList<BoneKeyframeRefObject *> &keyframeRefs, MotionProxy *motionProxy, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_motionProxyRef(motionProxy)
    {
        initialize(keyframeRefs, motionProxy);
    }
    UpdateBoneKeyframeCommand(BoneKeyframeRefObject *keyframeRef, MotionProxy *motionProxy, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_motionProxyRef(motionProxy)
    {
        QList<BoneKeyframeRefObject *> keyframeRefs;
        keyframeRefs.append(keyframeRef);
        initialize(keyframeRefs, motionProxy);
    }
    ~UpdateBoneKeyframeCommand() {
        QHashIterator<BoneKeyframeRefObject *, BoneKeyframeRefObject *> it(m_new2oldKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            BoneKeyframeRefObject *newKeyframe = it.key();
            if (!newKeyframe->parentTrack()->contains(newKeyframe)) {
                delete newKeyframe;
            }
        }
        m_motionProxyRef = 0;
    }

    virtual void undo() {
        QHashIterator<BoneKeyframeRefObject *, BoneKeyframeRefObject *> it(m_old2newKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            BoneKeyframeRefObject *keyframeRef = it.key();
            BoneMotionTrack *track = qobject_cast<BoneMotionTrack *>(keyframeRef->parentTrack());
            Q_ASSERT(track);
            track->replace(keyframeRef, m_old2newKeyframeRefs.value(keyframeRef), false);
            setLocalTransform(keyframeRef);
        }
        refreshTracks();
    }
    virtual void redo() {
        QHashIterator<BoneKeyframeRefObject *, BoneKeyframeRefObject *> it(m_new2oldKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            BoneKeyframeRefObject *newKeyframe = it.key();
            BoneMotionTrack *track = qobject_cast<BoneMotionTrack *>(newKeyframe->parentTrack());
            Q_ASSERT(track);
            track->replace(newKeyframe, m_new2oldKeyframeRefs.value(newKeyframe), false);
            setLocalTransform(newKeyframe);
        }
        refreshTracks();
    }

private:
    void initialize(const QList<BoneKeyframeRefObject *> &keyframeRefs, const MotionProxy *motionProxyRef) {
        Q_ASSERT(motionProxyRef);
        const ModelProxy *modelProxyRef = motionProxyRef->parentModel();
        QScopedPointer<BoneKeyframeRefObject> newKeyframe;
        foreach (BoneKeyframeRefObject *keyframeRef, keyframeRefs) {
            BoneMotionTrack *track = qobject_cast<BoneMotionTrack *>(keyframeRef->parentTrack());
            newKeyframe.reset(new BoneKeyframeRefObject(track, keyframeRef->data()->clone()));
            IBoneKeyframe *newKeyframeRef = newKeyframe->data();
            BoneRefObject *boneRef = modelProxyRef->findBoneByName(track->name());
            newKeyframeRef->setLocalTranslation(boneRef->rawLocalTranslation());
            newKeyframeRef->setLocalOrientation(boneRef->rawLocalOrientation());
            m_tracks.insert(keyframeRef->parentTrack());
            m_old2newKeyframeRefs.insert(keyframeRef, newKeyframe.data());
            m_new2oldKeyframeRefs.insert(newKeyframe.take(), keyframeRef);
        }
        setText(QApplication::tr("Update Bone Keyframe(s)"));
    }
    void refreshTracks() {
        foreach (BaseMotionTrack *track, m_tracks) {
            track->sort();
        }
        m_motionProxyRef->data()->update(IKeyframe::kBoneKeyframe);
    }
    void setLocalTransform(const BoneKeyframeRefObject *boneKeyframeRef) {
        if (BoneRefObject *boneRef = m_motionProxyRef->parentModel()->findBoneByName(boneKeyframeRef->name())) {
            const IBoneKeyframe *keyframeRef = boneKeyframeRef->data();
            boneRef->setRawLocalTranslation(keyframeRef->localTranslation());
            boneRef->setRawLocalOrientation(keyframeRef->localOrientation());
        }
    }

    QHash<BoneKeyframeRefObject *, BoneKeyframeRefObject *> m_old2newKeyframeRefs;
    QHash<BoneKeyframeRefObject *, BoneKeyframeRefObject *> m_new2oldKeyframeRefs;
    QSet<BaseMotionTrack *> m_tracks;
    MotionProxy *m_motionProxyRef;
};

class UpdateCameraKeyframeCommand : public QUndoCommand {
public:
    UpdateCameraKeyframeCommand(CameraKeyframeRefObject *keyframeRef, CameraRefObject *cameraRef, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_newKeyframe(new CameraKeyframeRefObject(qobject_cast<CameraMotionTrack *>(keyframeRef->parentTrack()), keyframeRef->data()->clone())),
          m_keyframeRef(keyframeRef),
          m_cameraRef(cameraRef)
    {
        Q_ASSERT(m_keyframeRef);
        Q_ASSERT(m_cameraRef);
        ICameraKeyframe *newKeyframeRef = m_newKeyframe->data();
        newKeyframeRef->setAngle(Util::toVector3(m_cameraRef->angle()));
        newKeyframeRef->setDistance(m_cameraRef->distance());
        newKeyframeRef->setFov(m_cameraRef->fov());
        newKeyframeRef->setLookAt(Util::toVector3(m_cameraRef->lookAt()));
        setText(QApplication::tr("Update Camera Keyframe"));
    }
    ~UpdateCameraKeyframeCommand() {
        if(m_keyframeRef->parentTrack()->contains(m_newKeyframe.data())) {
            m_newKeyframe.take();
        }
        m_keyframeRef = 0;
        m_cameraRef = 0;
    }

    virtual void undo() {
        CameraMotionTrack *track = qobject_cast<CameraMotionTrack *>(m_keyframeRef->parentTrack());
        Q_ASSERT(track);
        track->replace(m_keyframeRef, m_newKeyframe.data(), true);
        setCameraParameters(m_keyframeRef);
    }
    virtual void redo() {
        CameraMotionTrack *track = qobject_cast<CameraMotionTrack *>(m_keyframeRef->parentTrack());
        Q_ASSERT(track);
        track->replace(m_newKeyframe.data(), m_keyframeRef, true);
        setCameraParameters(m_newKeyframe.data());
    }

private:
    void setCameraParameters(const CameraKeyframeRefObject *cameraKeyframe) {
        if (m_cameraRef) {
            const ICameraKeyframe *keyframeRef = cameraKeyframe->data();
            m_cameraRef->setAngle(Util::fromVector3(keyframeRef->angle()));
            m_cameraRef->setDistance(keyframeRef->distance());
            m_cameraRef->setFov(keyframeRef->fov());
            m_cameraRef->setLookAt(Util::fromVector3(keyframeRef->lookAt()));
        }
    }

    QScopedPointer<CameraKeyframeRefObject> m_newKeyframe;
    CameraKeyframeRefObject *m_keyframeRef;
    CameraRefObject *m_cameraRef;
};

class UpdateLightKeyframeCommand : public QUndoCommand {
public:
    UpdateLightKeyframeCommand(LightKeyframeRefObject *keyframeRef, LightRefObject *lightRef, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_newKeyframe(new LightKeyframeRefObject(qobject_cast<LightMotionTrack *>(keyframeRef->parentTrack()), keyframeRef->data()->clone())),
          m_keyframeRef(keyframeRef),
          m_lightRef(lightRef)
    {
        Q_ASSERT(m_keyframeRef);
        Q_ASSERT(m_lightRef);
        ILightKeyframe *newKeyframeRef = m_newKeyframe->data();
        newKeyframeRef->setColor(Util::toColorRGB(m_lightRef->color()));
        newKeyframeRef->setDirection(Util::toVector3(m_lightRef->direction()));
        setText(QApplication::tr("Update Light Keyframe"));
    }
    ~UpdateLightKeyframeCommand() {
        if(m_keyframeRef->parentTrack()->contains(m_newKeyframe.data())) {
            m_newKeyframe.take();
        }
        m_keyframeRef = 0;
        m_lightRef = 0;
    }

    virtual void undo() {
        LightMotionTrack *track = qobject_cast<LightMotionTrack *>(m_keyframeRef->parentTrack());
        Q_ASSERT(track);
        track->replace(m_keyframeRef, m_newKeyframe.data(), true);
        setLightParameters(m_keyframeRef);
    }
    virtual void redo() {
        LightMotionTrack *track = qobject_cast<LightMotionTrack *>(m_keyframeRef->parentTrack());
        Q_ASSERT(track);
        track->replace(m_newKeyframe.data(), m_keyframeRef, true);
        setLightParameters(m_newKeyframe.data());
    }

private:
    void setLightParameters(const LightKeyframeRefObject *lightKeyframe) {
        if (m_lightRef) {
            const ILightKeyframe *keyframeRef = lightKeyframe->data();
            m_lightRef->setColor(Util::fromColorRGB(keyframeRef->color()));
            m_lightRef->setDirection(Util::fromVector3(keyframeRef->direction()));
        }
    }

    QScopedPointer<LightKeyframeRefObject> m_newKeyframe;
    LightKeyframeRefObject *m_keyframeRef;
    LightRefObject *m_lightRef;
};

class UpdateMorphKeyframeCommand : public QUndoCommand {
public:
    UpdateMorphKeyframeCommand(const QList<MorphKeyframeRefObject *> &keyframeRefs, MotionProxy *motionProxy, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_motionProxyRef(motionProxy)
    {
        initialize(keyframeRefs, motionProxy);
    }
    UpdateMorphKeyframeCommand(MorphKeyframeRefObject *keyframeRef, MotionProxy *motionProxy, QUndoCommand *parent)
        : QUndoCommand(parent),
          m_motionProxyRef(motionProxy)
    {
        QList<MorphKeyframeRefObject *> keyframeRefs;
        keyframeRefs.append(keyframeRef);
        initialize(keyframeRefs, motionProxy);
    }
    ~UpdateMorphKeyframeCommand() {
        QHashIterator<MorphKeyframeRefObject *, MorphKeyframeRefObject *> it(m_new2oldKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            MorphKeyframeRefObject *newKeyframe = it.key();
            if (!newKeyframe->parentTrack()->contains(newKeyframe)) {
                delete newKeyframe;
            }
        }
        m_motionProxyRef = 0;
    }

    virtual void undo() {
        QHashIterator<MorphKeyframeRefObject *, MorphKeyframeRefObject *> it(m_old2newKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            MorphKeyframeRefObject *keyframeRef = it.key();
            MorphMotionTrack *track = qobject_cast<MorphMotionTrack *>(keyframeRef->parentTrack());
            Q_ASSERT(track);
            track->replace(keyframeRef, m_old2newKeyframeRefs.value(keyframeRef), false);
            setWeight(keyframeRef);
        }
        refreshTracks();
    }
    virtual void redo() {
        QHashIterator<MorphKeyframeRefObject *, MorphKeyframeRefObject *> it(m_new2oldKeyframeRefs);
        while (it.hasNext()) {
            it.next();
            MorphKeyframeRefObject *newKeyframe = it.key();
            MorphMotionTrack *track = qobject_cast<MorphMotionTrack *>(newKeyframe->parentTrack());
            Q_ASSERT(track);
            track->replace(newKeyframe, m_new2oldKeyframeRefs.value(newKeyframe), false);
            setWeight(newKeyframe);
        }
        refreshTracks();
    }

private:
    void initialize(const QList<MorphKeyframeRefObject *> &keyframeRefs, const MotionProxy *motionProxyRef) {
        Q_ASSERT(motionProxyRef);
        const ModelProxy *modelProxyRef = motionProxyRef->parentModel();
        QScopedPointer<MorphKeyframeRefObject> newKeyframe;
        foreach (MorphKeyframeRefObject *keyframeRef, keyframeRefs) {
            MorphMotionTrack *track = qobject_cast<MorphMotionTrack *>(keyframeRef->parentTrack());
            newKeyframe.reset(new MorphKeyframeRefObject(track, keyframeRef->data()->clone()));
            IMorphKeyframe *newKeyframeRef = newKeyframe->data();
            MorphRefObject *morphRef = modelProxyRef->findMorphByName(track->name());
            newKeyframeRef->setWeight(morphRef->weight());
            m_tracks.insert(keyframeRef->parentTrack());
            m_old2newKeyframeRefs.insert(keyframeRef, newKeyframe.data());
            m_new2oldKeyframeRefs.insert(newKeyframe.take(), keyframeRef);
        }
        setText(QApplication::tr("Update Morph Keyframe(s)"));
    }
    void refreshTracks() {
        foreach (BaseMotionTrack *track, m_tracks) {
            track->sort();
        }
        m_motionProxyRef->data()->update(IKeyframe::kMorphKeyframe);
    }
    void setWeight(const MorphKeyframeRefObject *morphKeyframeRef) {
        if (MorphRefObject *morphRef = m_motionProxyRef->parentModel()->findMorphByName(morphKeyframeRef->name())) {
            const IMorphKeyframe *keyframeRef = morphKeyframeRef->data();
            morphRef->setWeight(keyframeRef->weight());
        }
    }

    QHash<MorphKeyframeRefObject *, MorphKeyframeRefObject *> m_old2newKeyframeRefs;
    QHash<MorphKeyframeRefObject *, MorphKeyframeRefObject *> m_new2oldKeyframeRefs;
    QSet<BaseMotionTrack *> m_tracks;
    MotionProxy *m_motionProxyRef;
};

class UpdateBoneKeyframeInterpolationCommand : public QUndoCommand {
public:
    UpdateBoneKeyframeInterpolationCommand(BoneKeyframeRefObject *keyframeRef,
                                           MotionProxy *motionProxyRef,
                                           const QVector4D &value,
                                           int type,
                                           QUndoCommand *parent)
        : QUndoCommand(parent),
          m_keyframeRef(keyframeRef),
          m_motionProxyRef(motionProxyRef),
          m_newValue(value.x(), value.y(), value.z(), value.w()),
          m_type(static_cast<IBoneKeyframe::InterpolationType>(type))
    {
        Q_ASSERT(m_keyframeRef && m_motionProxyRef);
        m_keyframeRef->data()->getInterpolationParameter(m_type, m_oldValue);
        setText(QApplication::tr("Update Bone Keyframe Interpolation Parameters"));
    }
    ~UpdateBoneKeyframeInterpolationCommand() {
        m_keyframeRef = 0;
        m_motionProxyRef = 0;
    }

    virtual void undo() {
        m_keyframeRef->data()->setInterpolationParameter(m_type, m_oldValue);
    }
    virtual void redo() {
        m_keyframeRef->data()->setInterpolationParameter(m_type, m_newValue);
    }

private:
    BoneKeyframeRefObject *m_keyframeRef;
    MotionProxy *m_motionProxyRef;
    const QuadWord m_newValue;
    const IBoneKeyframe::InterpolationType m_type;
    QuadWord m_oldValue;
};

class UpdateCameraKeyframeInterpolationCommand : public QUndoCommand {
public:
    UpdateCameraKeyframeInterpolationCommand(CameraKeyframeRefObject *keyframeRef,
                                             MotionProxy *motionProxyRef,
                                             const QVector4D &value,
                                             int type,
                                             QUndoCommand *parent)
        : QUndoCommand(parent),
          m_keyframeRef(keyframeRef),
          m_motionProxyRef(motionProxyRef),
          m_newValue(value.x(), value.y(), value.z(), value.w()),
          m_type(static_cast<ICameraKeyframe::InterpolationType>(type))
    {
        Q_ASSERT(m_keyframeRef && m_motionProxyRef);
        m_keyframeRef->data()->getInterpolationParameter(m_type, m_oldValue);
        setText(QApplication::tr("Update Camera Keyframe Interpolation Parameters"));
    }
    ~UpdateCameraKeyframeInterpolationCommand() {
        m_keyframeRef = 0;
        m_motionProxyRef = 0;
    }

    virtual void undo() {
        m_keyframeRef->data()->setInterpolationParameter(m_type, m_oldValue);
    }
    virtual void redo() {
        m_keyframeRef->data()->setInterpolationParameter(m_type, m_newValue);
    }

private:
    CameraKeyframeRefObject *m_keyframeRef;
    MotionProxy *m_motionProxyRef;
    const QuadWord m_newValue;
    const ICameraKeyframe::InterpolationType m_type;
    QuadWord m_oldValue;
};

class PasteKeyframesCommand : public QUndoCommand {
public:
    typedef QPair<QObject *, quint64> ProceedSetPair;
    typedef QSet<ProceedSetPair> ProceedSet;

    PasteKeyframesCommand(MotionProxy *motionProxy,
                          const QList<BaseKeyframeRefObject *> &copiedKeyframes,
                          const IKeyframe::TimeIndex &offsetTimeIndex,
                          QList<BaseKeyframeRefObject *> *selectedKeyframes,
                          QUndoCommand *parent)
        : QUndoCommand(parent),
          m_copiedKeyframeRefs(copiedKeyframes),
          m_offsetTimeIndex(offsetTimeIndex),
          m_motionProxy(motionProxy),
          m_selectedKeyframeRefs(selectedKeyframes)
    {
        setText(QApplication::tr("Paster Keyframe(s)"));
    }
    ~PasteKeyframesCommand() {
    }

    virtual void undo() {
        foreach (BaseKeyframeRefObject *keyframe, m_createdKeyframes) {
            BaseMotionTrack *track = keyframe->parentTrack();
            track->removeKeyframe(keyframe, false);
        }
        m_motionProxy->refresh();
        qDeleteAll(m_createdKeyframes);
        m_createdKeyframes.clear();
        m_selectedKeyframeRefs->clear();
        m_selectedKeyframeRefs->append(m_copiedKeyframeRefs);
    }
    virtual void redo() {
        ProceedSet proceeded;
        foreach (BaseKeyframeRefObject *keyframe, m_copiedKeyframeRefs) {
            const qreal &timeIndex = keyframe->timeIndex() + m_offsetTimeIndex;
            BaseMotionTrack *track = keyframe->parentTrack();
            if (BaseKeyframeRefObject *clonedKeyframe = track->copy(keyframe, timeIndex, false)) {
                handleKeyframe(clonedKeyframe, proceeded);
                m_createdKeyframes.append(clonedKeyframe);
            }
        }
        m_motionProxy->refresh();
    }
    virtual void handleKeyframe(BaseKeyframeRefObject *value, ProceedSet &proceeded) {
        Q_UNUSED(value);
        Q_UNUSED(proceeded);
    }

protected:
    const QList<BaseKeyframeRefObject *> m_copiedKeyframeRefs;
    const QList<BaseKeyframeRefObject *> m_previousSelectedKeyframeRefs;
    const IKeyframe::TimeIndex m_offsetTimeIndex;
    MotionProxy *m_motionProxy;
    QList<BaseKeyframeRefObject *> *m_selectedKeyframeRefs;
    QList<BaseKeyframeRefObject *> m_createdKeyframes;
};

class InversedPasteKeyframesCommand : public PasteKeyframesCommand {
public:
    InversedPasteKeyframesCommand(MotionProxy *motionProxy,
                                  const QList<BaseKeyframeRefObject *> &copiedKeyframes,
                                  const IKeyframe::TimeIndex &offsetTimeIndex,
                                  QList<BaseKeyframeRefObject *> *selectedKeyframes,
                                  QUndoCommand *parent)
        : PasteKeyframesCommand(motionProxy, copiedKeyframes, offsetTimeIndex, selectedKeyframes, parent)
    {
        setText(QApplication::tr("Paster Keyframe(s) with Inversed"));
    }

    void handleKeyframe(BaseKeyframeRefObject *value, ProceedSet &proceeded) {
        ModelProxy *modelProxy = m_motionProxy->parentModel();
        if (BoneKeyframeRefObject *keyframe = qobject_cast<BoneKeyframeRefObject *>(value)) {
            const ProjectProxy *projectProxy = m_motionProxy->parentProject();
            const IEncoding *encodingRef = projectProxy->encodingInstanceRef();
            const QString &leftStringLiteral = Util::toQString(encodingRef->stringConstant(IEncoding::kLeft));
            const QString &rightStringLiteral = Util::toQString(encodingRef->stringConstant(IEncoding::kRight));
            const QString &name = keyframe->name();
            const quint64 &timeIndex = keyframe->timeIndex();
            bool isRight = name.startsWith(rightStringLiteral), isLeft = name.startsWith(leftStringLiteral);
            BoneRefObject *boneRef = modelProxy->findBoneByName(name);
            if (!proceeded.contains(ProceedSetPair(boneRef, timeIndex)) && (isRight || isLeft)) {
                QString replaced = name;
                if (isRight) {
                    replaced.replace(rightStringLiteral, leftStringLiteral);
                    proceeded.insert(ProceedSetPair(modelProxy->findBoneByName(replaced), timeIndex));
                }
                else if (isLeft) {
                    replaced.replace(leftStringLiteral, rightStringLiteral);
                    proceeded.insert(ProceedSetPair(modelProxy->findBoneByName(replaced), timeIndex));
                }
                const QVector3D &v = keyframe->localTranslation();
                keyframe->setLocalTranslation(QVector3D(-v.x(), v.y(), v.z()));
                const QQuaternion &q = keyframe->localOrientation();
                keyframe->setLocalOrientation(QQuaternion(q.x(), -q.y(), -q.z(), q.scalar()));
            }
        }
    }
};

class CutKeyframesCommand : public BaseKeyframeCommand {
public:
    CutKeyframesCommand(QList<BaseKeyframeRefObject *> *keyframesRefs, QUndoCommand *parent)
        : BaseKeyframeCommand(*keyframesRefs, parent),
          m_copiedKeyframesRef(keyframesRefs),
          m_cutKeyframeRefs(*keyframesRefs)
    {
        setText(QApplication::tr("Cut Keyframe(s)"));
    }
    ~CutKeyframesCommand() {
    }

    virtual void undo() {
        addKeyframe();
        *m_copiedKeyframesRef = m_cutKeyframeRefs;
        refreshTrack();
    }
    virtual void redo() {
        removeKeyframe();
        m_copiedKeyframesRef->clear();
        refreshTrack();
    }

private:
    QList<BaseKeyframeRefObject *> *m_copiedKeyframesRef;
    QList<BaseKeyframeRefObject *> m_cutKeyframeRefs;
};

template<typename TTrack, typename TKeyframeRefObject, typename TKeyframe>
class BaseAllKeyframesCommand : public QUndoCommand {
public:
    BaseAllKeyframesCommand(QUndoCommand *parent)
        : QUndoCommand(parent)
    {
    }
    virtual ~BaseAllKeyframesCommand() {
        qDeleteAll(m_keyframes);
        m_keyframes.clear();
    }

    void saveAllKeyframes(const QHash<QString, TTrack *> &trackBundle) {
        QHashIterator<QString, TTrack *> it(trackBundle);
        while (it.hasNext()) {
            it.next();
            const TTrack *track = it.value();
            foreach (BaseKeyframeRefObject *item, track->keyframes()) {
                TKeyframeRefObject *keyframe = qobject_cast<TKeyframeRefObject *>(item);
                Q_ASSERT(keyframe);
                m_keyframes.insert(keyframe, keyframe->data()->clone());
            }
        }
    }
    void handleUndo(const QHash<QString, TTrack *> &trackBundle) {
        QHashIterator<QString, TTrack *> it(trackBundle);
        QByteArray bytes;
        while (it.hasNext()) {
            it.next();
            const TTrack *track = it.value();
            foreach (BaseKeyframeRefObject *item, track->keyframes()) {
                TKeyframeRefObject *keyframe = qobject_cast<TKeyframeRefObject *>(item);
                Q_ASSERT(keyframe);
                if (const TKeyframe *k = m_keyframes.value(keyframe)) {
                    bytes.resize(k->estimateSize());
                    k->write(reinterpret_cast<uint8 *>(bytes.data()));
                    keyframe->data()->read(reinterpret_cast<const uint8 *>(bytes.constData()));
                }
            }
        }
    }
    void handleRedo(const QHash<QString, TTrack *> &trackBundle) {
        QHashIterator<QString, TTrack *> it(trackBundle);
        while (it.hasNext()) {
            it.next();
            const TTrack *track = it.value();
            foreach (BaseKeyframeRefObject *item, track->keyframes()) {
                TKeyframeRefObject *keyframe = qobject_cast<TKeyframeRefObject *>(item);
                Q_ASSERT(keyframe);
                //keyframe->setLocalTranslation(keyframe->localTranslation() + m_translation);
            }
        }
    }

protected:
    virtual void handleKeyframe(TKeyframeRefObject *value) = 0;

    QHash<const TKeyframeRefObject *, TKeyframe *> m_keyframes;
};

class TranslateAllBoneKeyframesCommand : public BaseAllKeyframesCommand<BoneMotionTrack, BoneKeyframeRefObject, vpvl2::IBoneKeyframe> {
public:
    TranslateAllBoneKeyframesCommand(MotionProxy *motionProxyRef,
                                     const QVector3D &value,
                                     QUndoCommand *parent)
        : BaseAllKeyframesCommand<BoneMotionTrack, BoneKeyframeRefObject, vpvl2::IBoneKeyframe>(parent),
          m_motionProxyRef(motionProxyRef),
          m_translation(value)
    {
        saveAllKeyframes(m_motionProxyRef->boneMotionTrackBundle());
        setText(QApplication::tr("Scale Bone Keyframes"));
    }
    ~TranslateAllBoneKeyframesCommand() {
    }

    virtual void undo() {
        handleUndo(m_motionProxyRef->boneMotionTrackBundle());
    }
    virtual void redo() {
        handleRedo(m_motionProxyRef->boneMotionTrackBundle());
    }

private:
    void handleKeyframe(BoneKeyframeRefObject *value) {
        value->setLocalTranslation(value->localTranslation() + m_translation);
    }

    MotionProxy *m_motionProxyRef;
    const QVector3D m_translation;
};

class ScaleAllBoneKeyframesCommand : public BaseAllKeyframesCommand<BoneMotionTrack, BoneKeyframeRefObject, vpvl2::IBoneKeyframe> {
public:
    ScaleAllBoneKeyframesCommand(MotionProxy *motionProxyRef,
                                 const QVector3D &translationScaleFactor,
                                 const QVector3D &orientationScaleFactor,
                                 QUndoCommand *parent)
        : BaseAllKeyframesCommand<BoneMotionTrack, BoneKeyframeRefObject, vpvl2::IBoneKeyframe>(parent),
          m_motionProxyRef(motionProxyRef),
          m_translationScaleFactor(translationScaleFactor),
          m_orientationScaleFactor(orientationScaleFactor)
    {
        saveAllKeyframes(m_motionProxyRef->boneMotionTrackBundle());
        setText(QApplication::tr("Scale Bone Keyframes"));
    }
    ~ScaleAllBoneKeyframesCommand() {
    }

    virtual void undo() {
        handleUndo(m_motionProxyRef->boneMotionTrackBundle());
    }
    virtual void redo() {
        handleRedo(m_motionProxyRef->boneMotionTrackBundle());
    }

private:
    void handleKeyframe(BoneKeyframeRefObject *value) {
        value->setLocalTranslation(value->localTranslation() * m_translationScaleFactor);
        value->setLocalEulerOrientation(value->localEulerOrientation() * m_orientationScaleFactor);
    }

    MotionProxy *m_motionProxyRef;
    const QVector3D m_translationScaleFactor;
    const QVector3D m_orientationScaleFactor;
};

class ScaleAllMorphKeyframesCommand : public BaseAllKeyframesCommand<MorphMotionTrack, MorphKeyframeRefObject, vpvl2::IMorphKeyframe> {
public:
    ScaleAllMorphKeyframesCommand(MotionProxy *motionProxyRef,
                                  const qreal &scaleFactor,
                                  QUndoCommand *parent)
        : BaseAllKeyframesCommand<MorphMotionTrack, MorphKeyframeRefObject, vpvl2::IMorphKeyframe>(parent),
          m_motionProxyRef(motionProxyRef),
          m_scaleFactor(scaleFactor)
    {
        saveAllKeyframes(m_motionProxyRef->morphMotionTrackBundle());
        setText(QApplication::tr("Scale Morph Keyframes"));
    }
    ~ScaleAllMorphKeyframesCommand() {
    }

    virtual void undo() {
        handleUndo(m_motionProxyRef->morphMotionTrackBundle());
    }
    virtual void redo() {
        handleRedo(m_motionProxyRef->morphMotionTrackBundle());
    }

private:
    void handleKeyframe(MorphKeyframeRefObject *value) {
        value->setWeight(value->weight() * m_scaleFactor);
    }

    MotionProxy *m_motionProxyRef;
    const qreal m_scaleFactor;
};

}

MotionProxy::MotionProxy(ProjectProxy *projectRef,
                         IMotion *motion,
                         const QUuid &uuid,
                         const QUrl &fileUrl,
                         QUndoStack *undoStackRef)
    : QObject(projectRef),
      m_projectRef(projectRef),
      m_cameraMotionTrackRef(0),
      m_lightMotionTrackRef(0),
      m_motion(motion),
      m_undoStackRef(undoStackRef),
      m_uuid(uuid),
      m_fileUrl(fileUrl),
      m_dirty(false)
{
    Q_ASSERT(m_projectRef);
    Q_ASSERT(m_undoStackRef);
    Q_ASSERT(!m_motion.isNull());
    Q_ASSERT(!m_uuid.isNull());
    connect(this, &MotionProxy::durationTimeIndexChanged, projectRef, &ProjectProxy::durationTimeIndexChanged);
    VPVL2_VLOG(1, "The motion " << uuid.toString().toStdString() << " is added");
}

MotionProxy::~MotionProxy()
{
    VPVL2_VLOG(1, "The motion " << uuid().toString().toStdString() << " will be deleted");
    qDeleteAll(m_boneMotionTrackBundle);
    m_boneMotionTrackBundle.clear();
    qDeleteAll(m_morphMotionTrackBundle);
    m_morphMotionTrackBundle.clear();
    m_cameraMotionTrackRef = 0;
    m_lightMotionTrackRef = 0;
    m_undoStackRef = 0;
    m_projectRef = 0;
    m_dirty = false;
}

void MotionProxy::assignModel(ModelProxy *modelProxy, const Factory *factoryRef)
{
    Q_ASSERT(modelProxy);
    vpvl2::IMotion *motionRef = m_motion.data();
    int numLoadedKeyframes = 0;
    int numBoneKeyframes = motionRef->countKeyframes(IKeyframe::kBoneKeyframe);
    int numMorphKeyframes = motionRef->countKeyframes(IKeyframe::kMorphKeyframe);
    int numEstimatedTotalKeyframes = numBoneKeyframes + numMorphKeyframes;
    emit motionWillLoad(numEstimatedTotalKeyframes);
    loadBoneTrackBundle(motionRef, numBoneKeyframes, numEstimatedTotalKeyframes, numLoadedKeyframes);
    loadMorphTrackBundle(motionRef, numMorphKeyframes, numEstimatedTotalKeyframes, numLoadedKeyframes);
    emit motionDidLoad(numLoadedKeyframes, numEstimatedTotalKeyframes);
    QScopedPointer<IBoneKeyframe> boneKeyframe;
    foreach (const BoneRefObject *bone, modelProxy->allBoneRefs()) {
        if (!findBoneMotionTrack(bone)) {
            boneKeyframe.reset(factoryRef->createBoneKeyframe(motionRef));
            boneKeyframe->setDefaultInterpolationParameter();
            boneKeyframe->setTimeIndex(0);
            boneKeyframe->setLocalOrientation(bone->rawLocalOrientation());
            boneKeyframe->setLocalTranslation(bone->rawLocalTranslation());
            boneKeyframe->setName(bone->data()->name(IEncoding::kDefaultLanguage));
            BoneMotionTrack *track = addBoneTrack(bone->name());
            track->addKeyframe(track->convertBoneKeyframe(boneKeyframe.take()), false);
        }
    }
    QScopedPointer<IMorphKeyframe> morphKeyframe;
    foreach (const MorphRefObject *morph, modelProxy->allMorphRefs()) {
        if (!findMorphMotionTrack(morph)) {
            morphKeyframe.reset(factoryRef->createMorphKeyframe(motionRef));
            morphKeyframe->setTimeIndex(0);
            morphKeyframe->setWeight(morph->weight());
            morphKeyframe->setName(morph->data()->name(IEncoding::kDefaultLanguage));
            MorphMotionTrack *track = addMorphTrack(morph->name());
            track->addKeyframe(track->convertMorphKeyframe(morphKeyframe.take()), false);
        }
    }
    refresh();
}

void MotionProxy::setCameraMotionTrack(CameraMotionTrack *track, const Factory *factoryRef)
{
    Q_ASSERT(track && factoryRef);
    IMotion *motionRef = data();
    const int nkeyframes = motionRef->countKeyframes(track->type());
    for (int i = 0; i < nkeyframes; i++) {
        ICameraKeyframe *keyframe = motionRef->findCameraKeyframeRefAt(i);
        track->add(track->convertCameraKeyframe(keyframe), false);
    }
    if (!track->findKeyframeByTimeIndex(0)) {
        QScopedPointer<ICamera> cameraRef(m_projectRef->projectInstanceRef()->createCamera());
        QScopedPointer<ICameraKeyframe> keyframe(factoryRef->createCameraKeyframe(data()));
        keyframe->setDefaultInterpolationParameter();
        keyframe->setAngle(cameraRef->angle());
        keyframe->setDistance(cameraRef->distance());
        keyframe->setFov(cameraRef->fov());
        keyframe->setLookAt(cameraRef->lookAt());
        track->addKeyframe(track->convertCameraKeyframe(keyframe.take()), false);
    }
    track->refresh();
    bindTrackSignals(track);
    m_cameraMotionTrackRef = track;
}

void MotionProxy::setLightMotionTrack(LightMotionTrack *track, const Factory *factoryRef)
{
    Q_ASSERT(track && factoryRef);
    IMotion *motionRef = data();
    const int nkeyframes = motionRef->countKeyframes(track->type());
    for (int i = 0; i < nkeyframes; i++) {
        ILightKeyframe *keyframe = motionRef->findLightKeyframeRefAt(i);
        track->add(track->convertLightKeyframe(keyframe), false);
    }
    if (!track->findKeyframeByTimeIndex(0)) {
        QScopedPointer<ILight> lightRef(m_projectRef->projectInstanceRef()->createLight());
        QScopedPointer<ILightKeyframe> keyframe(factoryRef->createLightKeyframe(data()));
        keyframe->setColor(lightRef->color());
        keyframe->setDirection(lightRef->direction());
        track->addKeyframe(track->convertLightKeyframe(keyframe.take()), true);
    }
    track->refresh();
    bindTrackSignals(track);
    m_lightMotionTrackRef = track;
}

void MotionProxy::selectKeyframes(const QList<BaseKeyframeRefObject *> &value)
{
    m_currentKeyframeRefs = value;
}

bool MotionProxy::save(const QUrl &fileUrl)
{
    bool result = false;
    if (fileUrl.isEmpty() || !fileUrl.isValid()) {
        /* do nothing if url is empty or invalid */
        VPVL2_VLOG(2, "fileUrl is empty or invalid: url=" << fileUrl.toString().toStdString());
    }
    else if (fileUrl.fileName().endsWith(".json")) {
        result = saveJson(fileUrl);
    }
    else {
        QByteArray bytes;
        bytes.resize(m_motion->estimateSize());
        m_motion->save(reinterpret_cast<uint8_t *>(bytes.data()));
        QSaveFile saveFile(fileUrl.toLocalFile());
        if (saveFile.open(QFile::WriteOnly | QFile::Unbuffered)) {
            saveFile.write(bytes);
            result = saveFile.commit();
        }
    }
    return result;
}

bool MotionProxy::saveJson(const QUrl &fileUrl) const
{
    QFile file(fileUrl.toLocalFile());
    if (file.open(QFile::WriteOnly)) {
        QJsonDocument document(toJson().toObject());
        file.write(document.toJson());
        file.close();
        return true;
    }
    else {
        qWarning() << file.errorString();
        return false;
    }
}

QJsonValue MotionProxy::toJson() const
{
    QJsonObject v, boneTracks, morphTracks;
    v.insert("uuid", uuid().toString());
    QHashIterator<QString, BoneMotionTrack *> it(m_boneMotionTrackBundle);
    while (it.hasNext()) {
        it.next();
        boneTracks.insert(it.key(), it.value()->toJson());
    }
    v.insert("boneTracks", boneTracks);
    QHashIterator<QString, MorphMotionTrack *> it2(m_morphMotionTrackBundle);
    while (it2.hasNext()) {
        it2.next();
        morphTracks.insert(it2.key(), it2.value()->toJson());
    }
    v.insert("morphTracks", morphTracks);
    if (m_cameraMotionTrackRef) {
        v.insert("cameraTrack", m_cameraMotionTrackRef->toJson());
    }
    if (m_lightMotionTrackRef) {
        v.insert("lightTrack", m_lightMotionTrackRef->toJson());
    }
    if (ModelProxy *parentModelRef = parentModel()) {
        QJsonObject m;
        m.insert("name", parentModelRef->name());
        m.insert("uuid", parentModelRef->uuid().toString());
        v.insert("parentModel", m);
    }
    return v;
}

qreal MotionProxy::differenceTimeIndex(qreal value) const
{
    return m_motion->durationTimeIndex() - value;
}

qreal MotionProxy::differenceDuration(qreal value) const
{
    return differenceTimeIndex(value) * Scene::defaultFPS();
}

BoneMotionTrack *MotionProxy::findBoneMotionTrack(const BoneRefObject *value) const
{
    return value ? findBoneMotionTrack(value->name()) : 0;
}

BoneMotionTrack *MotionProxy::findBoneMotionTrack(const QString &name) const
{
    return m_boneMotionTrackBundle.value(name);
}

MorphMotionTrack *MotionProxy::findMorphMotionTrack(const MorphRefObject *value) const
{
    return value ? findMorphMotionTrack(value->name()) : 0;
}

MorphMotionTrack *MotionProxy::findMorphMotionTrack(const QString &name) const
{
    return m_morphMotionTrackBundle.value(name);
}

BaseKeyframeRefObject *MotionProxy::resolveKeyframeAt(const quint64 &timeIndex, QObject *opaque) const
{
    if (const BoneRefObject *boneRef = qobject_cast<const BoneRefObject *>(opaque)) {
        const BoneMotionTrack *track = findBoneMotionTrack(boneRef);
        return track ? track->findKeyframeByTimeIndex(timeIndex) : 0;
    }
    else if (const CameraRefObject *cameraRef = qobject_cast<const CameraRefObject *>(opaque)) {
        const CameraMotionTrack *track = cameraRef->track();
        return track->findKeyframeByTimeIndex(timeIndex);
    }
    else if (const LightRefObject *lightRef = qobject_cast<const LightRefObject *>(opaque)) {
        const LightMotionTrack *track = lightRef->track();
        return track->findKeyframeByTimeIndex(timeIndex);
    }
    else if (const MorphRefObject *morphRef = qobject_cast<const MorphRefObject *>(opaque)) {
        const MorphMotionTrack *track = findMorphMotionTrack(morphRef);
        return track ? track->findKeyframeByTimeIndex(timeIndex) : 0;
    }
    return 0;
}

void MotionProxy::applyParentModel()
{
    if (ModelProxy *modelProxy = m_projectRef->resolveModelProxy(m_motion->parentModelRef())) {
        m_motion->setParentModelRef(modelProxy->data());
    }
}

void MotionProxy::addKeyframe(QObject *opaque, const quint64 &timeIndex, QUndoCommand *parent)
{
    BaseKeyframeRefObject *keyframeRef = 0;
    if (const BoneRefObject *bone = qobject_cast<const BoneRefObject *>(opaque)) {
        keyframeRef = addBoneKeyframe(bone);
        if (keyframeRef) {
            int length = findBoneMotionTrack(bone->name())->length();
            VPVL2_VLOG(2, "insert type=BONE timeIndex=" << timeIndex << " name=" << bone->name().toStdString() << " length=" << length);
        }
    }
    else if (const CameraRefObject *camera = qobject_cast<const CameraRefObject *>(opaque)) {
        keyframeRef = addCameraKeyframe(camera);
        VPVL2_VLOG(2, "insert type=CAMERA timeIndex=" << timeIndex << " length=" << camera->track()->length());
    }
    else if (const LightRefObject *light = qobject_cast<const LightRefObject *>(opaque)) {
        keyframeRef = addLightKeyframe(light);
        VPVL2_VLOG(2, "insert type=LIGHT timeIndex=" << timeIndex << " length=" << light->track()->length());
    }
    else if (const MorphRefObject *morph = qobject_cast<const MorphRefObject *>(opaque)) {
        keyframeRef = addMorphKeyframe(morph);
        if (keyframeRef) {
            int length = findMorphMotionTrack(morph->name())->length();
            VPVL2_VLOG(2, "insert type=MORPH timeIndex=" << timeIndex << " name=" << morph->name().toStdString() << " length=" << length);
        }
    }
    if (keyframeRef) {
        QScopedPointer<QUndoCommand> command(new AddKeyframeCommand(keyframeRef, parent));
        keyframeRef->setTimeIndex(timeIndex);
        pushUndoCommand(command, parent);
    }
}

void MotionProxy::updateKeyframe(QObject *opaque, const quint64 &timeIndex, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command;
    if (BoneKeyframeRefObject *boneKeyframeRef = qobject_cast<BoneKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateBoneKeyframeCommand(boneKeyframeRef, this, parent));
        VPVL2_VLOG(2, "update type=BONE timeIndex=" << timeIndex << " name=" << boneKeyframeRef->name().toStdString());
    }
    else if (CameraKeyframeRefObject *cameraKeyframeRef = qobject_cast<CameraKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateCameraKeyframeCommand(cameraKeyframeRef, m_projectRef->camera(), parent));
        VPVL2_VLOG(2, "update type=CAMERA timeIndex=" << timeIndex);
    }
    else if (LightKeyframeRefObject *lightKeyframeRef = qobject_cast<LightKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateLightKeyframeCommand(lightKeyframeRef, m_projectRef->light(), parent));
        VPVL2_VLOG(2, "update type=LIGHT timeIndex=" << timeIndex);
    }
    else if (MorphKeyframeRefObject *morphKeyframeRef = qobject_cast<MorphKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateMorphKeyframeCommand(morphKeyframeRef, this, parent));
        VPVL2_VLOG(2, "update type=MORPH timeIndex=" << timeIndex << " name=" << morphKeyframeRef->name().toStdString());
    }
    else if (const BoneRefObject *boneRef = qobject_cast<BoneRefObject *>(opaque)) {
        command.reset(updateOrAddKeyframeFromBone(boneRef, timeIndex, parent));
    }
    else if (CameraRefObject *cameraRef = qobject_cast<CameraRefObject *>(opaque)) {
        command.reset(updateOrAddKeyframeFromCamera(cameraRef, timeIndex, parent));
    }
    else if (LightRefObject *lightRef = qobject_cast<LightRefObject *>(opaque)) {
        command.reset(updateOrAddKeyframeFromLight(lightRef, timeIndex, parent));
    }
    else if (const MorphRefObject *morphRef = qobject_cast<MorphRefObject *>(opaque)) {
        command.reset(updateOrAddKeyframeFromMorph(morphRef, timeIndex, parent));
    }
    pushUndoCommand(command, parent);
}

void MotionProxy::updateKeyframeInterpolation(QObject *opaque, const QVector4D &value, int type, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command;
    if (BoneKeyframeRefObject *boneKeyframeRef = qobject_cast<BoneKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateBoneKeyframeInterpolationCommand(boneKeyframeRef, this, value, type, parent));
        VPVL2_VLOG(2, "updateInterpolation type=BONE timeIndex=" << boneKeyframeRef->timeIndex());
    }
    else if (CameraKeyframeRefObject *cameraKeyframeRef = qobject_cast<CameraKeyframeRefObject *>(opaque)) {
        command.reset(new UpdateCameraKeyframeInterpolationCommand(cameraKeyframeRef, this, value, type, parent));
        VPVL2_VLOG(2, "updateInterpolation type=CAMERA timeIndex=" << cameraKeyframeRef->timeIndex());
    }
}

void MotionProxy::removeKeyframe(QObject *opaque, QUndoCommand *parent)
{
    if (BaseKeyframeRefObject *keyframeRef = qobject_cast<BaseKeyframeRefObject *>(opaque)) {
        QScopedPointer<QUndoCommand> command(new RemoveKeyframeCommand(keyframeRef, parent));
        pushUndoCommand(command, parent);
    }
}

void MotionProxy::removeAllSelectedKeyframes(QUndoCommand *parent)
{
    QList<BaseKeyframeRefObject *> keyframes;
    foreach (BaseKeyframeRefObject *keyframe, m_currentKeyframeRefs) {
        if (keyframe->timeIndex() > 0) {
            keyframes.append(keyframe);
        }
    }
    removeKeyframes(keyframes, parent);
}

void MotionProxy::removeAllKeyframesAt(const quint64 &timeIndex, QUndoCommand *parent)
{
    QList<quint64> timeIndices;
    timeIndices.append(timeIndex);
    removeAllKeyframesIn(timeIndices, parent);
}

void MotionProxy::removeAllKeyframesIn(const QList<quint64> &timeIndices, QUndoCommand *parent)
{
    QList<BaseKeyframeRefObject *> keyframes;
    foreach (const quint64 &timeIndex, timeIndices) {
        if (timeIndex > 0) {
            QHashIterator<QString, BoneMotionTrack *> it(m_boneMotionTrackBundle);
            while (it.hasNext()) {
                it.next();
                if (BaseKeyframeRefObject *keyframe = it.value()->findKeyframeByTimeIndex(timeIndex)) {
                    keyframes.append(keyframe);
                }
            }
            QHashIterator<QString, MorphMotionTrack *> it2(m_morphMotionTrackBundle);
            while (it2.hasNext()) {
                it2.next();
                if (BaseKeyframeRefObject *keyframe = it2.value()->findKeyframeByTimeIndex(timeIndex)) {
                    keyframes.append(keyframe);
                }
            }
            if (m_cameraMotionTrackRef) {
                if (CameraKeyframeRefObject *keyframe = qobject_cast<CameraKeyframeRefObject *>(m_cameraMotionTrackRef->findKeyframeByTimeIndex(timeIndex))) {
                    keyframes.append(keyframe);
                }
            }
            if (m_lightMotionTrackRef) {
                if (LightKeyframeRefObject *keyframe = qobject_cast<LightKeyframeRefObject *>(m_lightMotionTrackRef->findKeyframeByTimeIndex(timeIndex))) {
                    keyframes.append(keyframe);
                }
            }
        }
    }
    removeKeyframes(keyframes, parent);
}

void MotionProxy::copyKeyframes()
{
    m_copiedKeyframeRefs = m_currentKeyframeRefs;
    emit canPasteChanged();
}

void MotionProxy::pasteKeyframes(const quint64 &timeIndex, bool inversed, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command;
    if (inversed) {
        command.reset(new InversedPasteKeyframesCommand(this, m_copiedKeyframeRefs, timeIndex, &m_currentKeyframeRefs, parent));
    }
    else {
        command.reset(new PasteKeyframesCommand(this, m_copiedKeyframeRefs, timeIndex, &m_currentKeyframeRefs, parent));
    }
    pushUndoCommand(command, parent);
    VPVL2_VLOG(2, "paste timeIndex=" << timeIndex << " inversed=" << inversed);
}

void MotionProxy::cutKeyframes(QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command(new CutKeyframesCommand(&m_copiedKeyframeRefs, parent));
    pushUndoCommand(command, parent);
}

IMotion *MotionProxy::data() const
{
    return m_motion.data();
}

ProjectProxy *MotionProxy::parentProject() const
{
    return m_projectRef;
}

ModelProxy *MotionProxy::parentModel() const
{
    return m_projectRef->resolveModelProxy(m_motion->parentModelRef());
}

QUndoStack *MotionProxy::undoStack() const
{
    return m_undoStackRef;
}

QUuid MotionProxy::uuid() const
{
    return m_uuid;
}

QUrl MotionProxy::fileUrl() const
{
    return m_fileUrl;
}

qreal MotionProxy::durationTimeIndex() const
{
    return differenceTimeIndex(0);
}

qreal MotionProxy::duration() const
{
    return differenceDuration(0);
}

QQmlListProperty<BaseKeyframeRefObject> MotionProxy::currentKeyframes()
{
    return QQmlListProperty<BaseKeyframeRefObject>(this, m_currentKeyframeRefs);
}

bool MotionProxy::canPaste() const
{
    return m_copiedKeyframeRefs.size() > 0;
}

bool MotionProxy::isDirty() const
{
    return m_dirty;
}

void MotionProxy::setDirty(bool value)
{
    if (isDirty() != value) {
        m_dirty = value;
        emit dirtyChanged();
        if (value) {
            m_projectRef->setDirty(value);
        }
    }
}

const MotionProxy::BoneMotionTrackBundle &MotionProxy::boneMotionTrackBundle() const
{
    return m_boneMotionTrackBundle;
}

const MotionProxy::MorphMotionTrackBundle &MotionProxy::morphMotionTrackBundle() const
{
    return m_morphMotionTrackBundle;
}

void MotionProxy::mergeKeyframes(const QList<QObject *> &keyframes, const quint64 &newTimeIndex, const quint64 &oldTimeIndex, QUndoCommand *parent)
{
    QList<MergeKeyframeCommand::Pair> keyframeRefs;
    foreach (QObject *item, keyframes) {
        BaseKeyframeRefObject *keyframe = qobject_cast<BaseKeyframeRefObject *>(item);
        Q_ASSERT(keyframe);
        BaseMotionTrack *track = keyframe->parentTrack();
        if (BaseKeyframeRefObject *src = track->findKeyframeByTimeIndex(newTimeIndex)) {
            keyframeRefs.append(MergeKeyframeCommand::Pair(keyframe, src));
        }
        else {
            keyframeRefs.append(MergeKeyframeCommand::Pair(keyframe, 0));
        }
    }
    if (!keyframeRefs.isEmpty()) {
        QScopedPointer<QUndoCommand> command(new MergeKeyframeCommand(keyframeRefs, newTimeIndex, oldTimeIndex, parent));
        pushUndoCommand(command, parent);
        VPVL2_VLOG(2, "merge newTimeIndex=" << newTimeIndex << " oldTimeIndex=" << oldTimeIndex << " length=" << keyframeRefs.size());
    }
}

void MotionProxy::translateAllBoneKeyframes(const QVector3D &value, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command(new TranslateAllBoneKeyframesCommand(this, value, parent));
    pushUndoCommand(command, parent);
}

void MotionProxy::scaleAllBoneKeyframes(const QVector3D &translationScaleFactor, const QVector3D &orientationScaleFactor, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command(new ScaleAllBoneKeyframesCommand(this, translationScaleFactor, orientationScaleFactor, parent));
    pushUndoCommand(command, parent);
}

void MotionProxy::scaleAllMorphKeyframes(const qreal &scaleFactor, QUndoCommand *parent)
{
    QScopedPointer<QUndoCommand> command(new ScaleAllMorphKeyframesCommand(this, scaleFactor, parent));
    pushUndoCommand(command, parent);
}

void MotionProxy::refresh()
{
    QHashIterator<QString, BoneMotionTrack *> it(m_boneMotionTrackBundle);
    while (it.hasNext()) {
        it.next();
        it.value()->sort();
    }
    m_motion->update(IKeyframe::kBoneKeyframe);
    QHashIterator<QString, MorphMotionTrack *> it2(m_morphMotionTrackBundle);
    while (it2.hasNext()) {
        it2.next();
        it2.value()->sort();
    }
    m_motion->update(IKeyframe::kMorphKeyframe);
    if (m_cameraMotionTrackRef) {
        m_cameraMotionTrackRef->refresh();
        m_motion->update(IKeyframe::kCameraKeyframe);
    }
    if (m_lightMotionTrackRef) {
        m_lightMotionTrackRef->refresh();
        m_motion->update(IKeyframe::kLightKeyframe);
    }
    emit durationTimeIndexChanged();
}

BoneKeyframeRefObject *MotionProxy::addBoneKeyframe(const BoneRefObject *value) const
{
    BoneKeyframeRefObject *keyframe = 0;
    if (BoneMotionTrack *track = findBoneMotionTrack(value->name())) {
        Factory *factoryRef = m_projectRef->factoryInstanceRef();
        QScopedPointer<IBoneKeyframe> keyframePtr(factoryRef->createBoneKeyframe(m_motion.data()));
        keyframePtr->setDefaultInterpolationParameter();
        keyframePtr->setName(value->data()->name(IEncoding::kDefaultLanguage));
        keyframe = track->convertBoneKeyframe(keyframePtr.take());
        keyframe->setLocalTranslation(value->localTranslation());
        keyframe->setLocalOrientation(value->localOrientation());
    }
    return keyframe;
}

CameraKeyframeRefObject *MotionProxy::addCameraKeyframe(const CameraRefObject *value) const
{
    CameraKeyframeRefObject *keyframe = 0;
    Factory *factoryRef = m_projectRef->factoryInstanceRef();
    QScopedPointer<ICameraKeyframe> keyframePtr(factoryRef->createCameraKeyframe(m_motion.data()));
    ICamera *cameraRef = value->data();
    keyframePtr->setDefaultInterpolationParameter();
    keyframePtr->setAngle(cameraRef->angle());
    keyframePtr->setDistance(cameraRef->distance());
    keyframePtr->setFov(cameraRef->fov());
    keyframePtr->setLookAt(cameraRef->lookAt());
    keyframe = value->track()->convertCameraKeyframe(keyframePtr.take());
    return keyframe;
}

LightKeyframeRefObject *MotionProxy::addLightKeyframe(const LightRefObject *value) const
{
    LightKeyframeRefObject *keyframe = 0;
    Factory *factoryRef = m_projectRef->factoryInstanceRef();
    QScopedPointer<ILightKeyframe> keyframePtr(factoryRef->createLightKeyframe(m_motion.data()));
    ILight *lightRef = value->data();
    keyframePtr->setColor(lightRef->color());
    keyframePtr->setDirection(lightRef->direction());
    keyframe = value->track()->convertLightKeyframe(keyframePtr.take());
    return keyframe;
}

MorphKeyframeRefObject *MotionProxy::addMorphKeyframe(const MorphRefObject *value) const
{
    MorphKeyframeRefObject *keyframe = 0;
    if (MorphMotionTrack *track = findMorphMotionTrack(value->name())) {
        Factory *factoryRef = m_projectRef->factoryInstanceRef();
        QScopedPointer<IMorphKeyframe> keyframePtr(factoryRef->createMorphKeyframe(m_motion.data()));
        keyframePtr->setName(value->data()->name(IEncoding::kDefaultLanguage));
        keyframe = track->convertMorphKeyframe(keyframePtr.take());
        keyframe->setWeight(value->weight());
    }
    return keyframe;
}

QUndoCommand *MotionProxy::updateOrAddKeyframeFromBone(const BoneRefObject *boneRef, const quint64 &timeIndex, QUndoCommand *parent)
{
    const QString &name = boneRef->name();
    if (BoneMotionTrack *track = findBoneMotionTrack(name)) {
        if (BaseKeyframeRefObject *boneKeyframeRef = track->findKeyframeByTimeIndex(timeIndex)) {
            updateKeyframe(boneKeyframeRef, timeIndex, parent);
        }
        else {
            Factory *factoryRef = m_projectRef->factoryInstanceRef();
            QScopedPointer<IBoneKeyframe> newKeyframe(factoryRef->createBoneKeyframe(m_motion.data()));
            newKeyframe->setDefaultInterpolationParameter();
            newKeyframe->setName(boneRef->data()->name(IEncoding::kDefaultLanguage));
            newKeyframe->setTimeIndex(timeIndex);
            BoneKeyframeRefObject *newKeyframe2 = track->convertBoneKeyframe(newKeyframe.take());
            QScopedPointer<QUndoCommand> parentCommand(new QUndoCommand(parent));
            new AddKeyframeCommand(newKeyframe2, parentCommand.data());
            new UpdateBoneKeyframeCommand(newKeyframe2, this, parentCommand.data());
            VPVL2_VLOG(2, "insert+update type=BONE timeIndex=" << timeIndex << " name=" << track->name().toStdString() << " length=" << track->length());
            return parentCommand.take();
        }
    }
    return 0;
}

QUndoCommand *MotionProxy::updateOrAddKeyframeFromCamera(CameraRefObject *cameraRef, const quint64 &timeIndex, QUndoCommand *parent)
{
    CameraMotionTrack *track = cameraRef->track();
    if (CameraKeyframeRefObject *cameraKeyframeRef = qobject_cast<CameraKeyframeRefObject *>(track->findKeyframeByTimeIndex(timeIndex))) {
        updateKeyframe(cameraKeyframeRef, timeIndex, parent);
    }
    else {
        Factory *factoryRef = m_projectRef->factoryInstanceRef();
        QScopedPointer<ICameraKeyframe> newKeyframe(factoryRef->createCameraKeyframe(m_motion.data()));
        newKeyframe->setDefaultInterpolationParameter();
        newKeyframe->setTimeIndex(timeIndex);
        CameraKeyframeRefObject *newKeyframe2 = track->convertCameraKeyframe(newKeyframe.take());
        QScopedPointer<QUndoCommand> parentCommand(new QUndoCommand(parent));
        new AddKeyframeCommand(newKeyframe2, parentCommand.data());
        new UpdateCameraKeyframeCommand(newKeyframe2, cameraRef, parentCommand.data());
        VPVL2_VLOG(2, "insert+update type=CAMERA timeIndex=" << timeIndex << " length=" << track->length());
        return parentCommand.take();
    }
    return 0;
}

QUndoCommand *MotionProxy::updateOrAddKeyframeFromLight(LightRefObject *lightRef, const quint64 &timeIndex, QUndoCommand *parent)
{
    LightMotionTrack *track = lightRef->track();
    if (LightKeyframeRefObject *cameraKeyframeRef = qobject_cast<LightKeyframeRefObject *>(track->findKeyframeByTimeIndex(timeIndex))) {
        updateKeyframe(cameraKeyframeRef, timeIndex, parent);
    }
    else {
        Factory *factoryRef = m_projectRef->factoryInstanceRef();
        QScopedPointer<ILightKeyframe> newKeyframe(factoryRef->createLightKeyframe(m_motion.data()));
        newKeyframe->setTimeIndex(timeIndex);
        LightKeyframeRefObject *newKeyframe2 = track->convertLightKeyframe(newKeyframe.take());
        QScopedPointer<QUndoCommand> parentCommand(new QUndoCommand(parent));
        new AddKeyframeCommand(newKeyframe2, parentCommand.data());
        new UpdateLightKeyframeCommand(newKeyframe2, lightRef, parentCommand.data());
        VPVL2_VLOG(2, "insert+update type=LIGHT timeIndex=" << timeIndex << " length=" << track->length());
        return parentCommand.take();
    }
    return 0;
}

QUndoCommand *MotionProxy::updateOrAddKeyframeFromMorph(const MorphRefObject *morphRef, const quint64 &timeIndex, QUndoCommand *parent)
{
    const QString &name = morphRef->name();
    if (MorphMotionTrack *track = findMorphMotionTrack(name)) {
        if (MorphKeyframeRefObject *morphKeyframeRef = qobject_cast<MorphKeyframeRefObject *>(track->findKeyframeByTimeIndex(timeIndex))) {
            updateKeyframe(morphKeyframeRef, timeIndex, parent);
        }
        else {
            Factory *factoryRef = m_projectRef->factoryInstanceRef();
            QScopedPointer<IMorphKeyframe> newKeyframe(factoryRef->createMorphKeyframe(m_motion.data()));
            newKeyframe->setName(morphRef->data()->name(IEncoding::kDefaultLanguage));
            newKeyframe->setTimeIndex(timeIndex);
            MorphKeyframeRefObject *newKeyframe2 = track->convertMorphKeyframe(newKeyframe.take());
            QScopedPointer<QUndoCommand> parentCommand(new QUndoCommand(parent));
            new AddKeyframeCommand(newKeyframe2, parentCommand.data());
            new UpdateMorphKeyframeCommand(newKeyframe2, this, parentCommand.data());
            VPVL2_VLOG(2, "insert+update type=MORPH timeIndex=" << timeIndex << " name=" << track->name().toStdString() << " length=" << track->length());
            return parentCommand.take();
        }
    }
    return 0;
}

void MotionProxy::loadBoneTrackBundle(IMotion *motionRef,
                                      int numBoneKeyframes,
                                      int numEstimatedKeyframes,
                                      int &numLoadedKeyframes)
{
    for (int i = 0; i < numBoneKeyframes; i++) {
        IBoneKeyframe *keyframe = motionRef->findBoneKeyframeRefAt(i);
        const QString &key = Util::toQString(keyframe->name());
        BoneMotionTrack *track = 0;
        if (m_boneMotionTrackBundle.contains(key)) {
            track = m_boneMotionTrackBundle.value(key);
        }
        else {
            track = addBoneTrack(key);
        }
        Q_ASSERT(track);
        track->add(track->convertBoneKeyframe(keyframe), false);
        emit motionBeLoading(numLoadedKeyframes++, numEstimatedKeyframes);
    }
}

void MotionProxy::loadMorphTrackBundle(IMotion *motionRef, int numMorphKeyframes, int numEstimatedKeyframes, int &numLoadedKeyframes)
{
    for (int i = 0; i < numMorphKeyframes; i++) {
        IMorphKeyframe *keyframe = motionRef->findMorphKeyframeRefAt(i);
        const QString &key = Util::toQString(keyframe->name());
        MorphMotionTrack *track = 0;
        if (m_morphMotionTrackBundle.contains(key)) {
            track = m_morphMotionTrackBundle.value(key);
        }
        else {
            track = addMorphTrack(key);
        }
        Q_ASSERT(track);
        track->add(track->convertMorphKeyframe(keyframe), false);
        emit motionBeLoading(numLoadedKeyframes++, numEstimatedKeyframes);
    }
}

void MotionProxy::removeKeyframes(const QList<BaseKeyframeRefObject *> &keyframes, QUndoCommand *parent)
{
    if (!keyframes.isEmpty()) {
        QScopedPointer<QUndoCommand> command(new RemoveKeyframeCommand(keyframes, parent));
        pushUndoCommand(command, parent);
        VPVL2_VLOG(2, "remove length=" << keyframes.size());
    }
}

BoneMotionTrack *MotionProxy::addBoneTrack(const QString &key)
{
    BoneMotionTrack *track = new BoneMotionTrack(this, key);
    bindTrackSignals(track);
    m_boneMotionTrackBundle.insert(key, track);
    return track;
}

MorphMotionTrack *MotionProxy::addMorphTrack(const QString &key)
{
    MorphMotionTrack *track = new MorphMotionTrack(this, key);
    bindTrackSignals(track);
    m_morphMotionTrackBundle.insert(key, track);
    return track;
}

void MotionProxy::bindTrackSignals(BaseMotionTrack *track)
{
    connect(track, &BaseMotionTrack::keyframeDidAdd, this, &MotionProxy::keyframeDidAdd);
    connect(track, &BaseMotionTrack::keyframeDidRemove, this, &MotionProxy::keyframeDidRemove);
    connect(track, &BaseMotionTrack::keyframeDidSwap, this, &MotionProxy::keyframeDidReplace);
    connect(track, &BaseMotionTrack::timeIndexDidChange, this, &MotionProxy::timeIndexDidChange);
}

void MotionProxy::pushUndoCommand(QScopedPointer<QUndoCommand> &command, QUndoCommand *parent)
{
    if (command) {
        if (!parent) {
            m_undoStackRef->push(command.data());
            setDirty(true);
        }
        command.take(); /* move reference to QUndoStack or parent QUndoCommand */
    }
}
