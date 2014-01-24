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

#include "VertexRefObject.h"
#include "BoneRefObject.h"
#include "ModelProxy.h"
#include "Util.h"

#include <vpvl2/vpvl2.h>
#include <vpvl2/extensions/qt/String.h>

using namespace vpvl2;

VertexRefObject::VertexRefObject(ModelProxy *parentModelRef,
                                 vpvl2::IVertex *vertexRef,
                                 const QUuid &uuid)
    : m_parentModelRef(parentModelRef),
      m_vertexRef(vertexRef),
      m_uuid(uuid)
{
    Q_ASSERT(m_parentModelRef);
    Q_ASSERT(m_vertexRef);
    Q_ASSERT(!m_uuid.isNull());
}

VertexRefObject::~VertexRefObject()
{
    m_parentModelRef = 0;
    m_vertexRef = 0;
}

QVector4D VertexRefObject::originUV(int index)
{
    return Util::fromVector4(m_vertexRef->originUV(index));
}

void VertexRefObject::setOriginUV(int index, const QVector4D &value)
{
    const QVector4D &oldValue = originUV(index);
    if (!qFuzzyCompare(oldValue, value)) {
        m_vertexRef->setOriginUV(index, Util::toVector4(value));
        m_parentModelRef->setDirty(true);
        emit originUVDidChange(index, value, oldValue);
    }
}

QVector4D VertexRefObject::morphUV(int index)
{
    return Util::fromVector4(m_vertexRef->morphUV(index));
}

void VertexRefObject::setMorphUV(int index, const QVector4D &value)
{
    const QVector4D &oldValue = morphUV(index);
    if (!qFuzzyCompare(oldValue, value)) {
        m_vertexRef->setMorphUV(index, Util::toVector4(value));
        m_parentModelRef->setDirty(true);
        emit morphUVDidChange(index, value, oldValue);
    }
}

BoneRefObject *VertexRefObject::bone(int index)
{
    return m_parentModelRef->resolveBoneRef(m_vertexRef->boneRef(index));
}

void VertexRefObject::setBone(int index, BoneRefObject *value)
{
    BoneRefObject *oldValue = bone(index);
    if (value && oldValue != value) {
        m_vertexRef->setBoneRef(index, value->data());
        m_parentModelRef->setDirty(true);
        emit boneDidChange(index, value, oldValue);
    }
}

qreal VertexRefObject::weight(int index)
{
    return m_vertexRef->weight(index);
}

void VertexRefObject::setWeight(int index, const qreal &value)
{
    qreal oldValue = weight(index);
    if (value && !qFuzzyCompare(oldValue, value)) {
        m_vertexRef->setWeight(index, value);
        m_parentModelRef->setDirty(true);
        emit weightDidChange(index, value, oldValue);
    }
}


IVertex *VertexRefObject::data() const
{
    Q_ASSERT(m_vertexRef);
    return m_vertexRef;
}

ModelProxy *VertexRefObject::parentModel() const
{
    Q_ASSERT(m_parentModelRef);
    return m_parentModelRef;
}

QUuid VertexRefObject::uuid() const
{
    return m_uuid;
}

QVector3D VertexRefObject::origin() const
{
    return Util::fromVector3(m_vertexRef->origin());
}

void VertexRefObject::setOrigin(const QVector3D &value)
{
    if (!qFuzzyCompare(origin(), value)) {
        m_vertexRef->setOrigin(Util::toVector3(value));
        m_parentModelRef->setDirty(true);
        emit originChanged();
    }
}

QVector3D VertexRefObject::normal() const
{
    return Util::fromVector3(m_vertexRef->normal());
}

void VertexRefObject::setNormal(const QVector3D &value)
{
    if (!qFuzzyCompare(normal(), value)) {
        m_vertexRef->setNormal(Util::toVector3(value));
        m_parentModelRef->setDirty(true);
        emit normalChanged();
    }
}

QVector3D VertexRefObject::textureCoord() const
{
    return Util::fromVector3(m_vertexRef->textureCoord());
}

void VertexRefObject::setTextureCoord(const QVector3D &value)
{
    if (!qFuzzyCompare(textureCoord(), value)) {
        m_vertexRef->setTextureCoord(Util::toVector3(value));
        m_parentModelRef->setDirty(true);
        emit textureCoordChanged();
    }
}

qreal VertexRefObject::edgeSize() const
{
    return m_vertexRef->edgeSize();
}

void VertexRefObject::setEdgeSize(const qreal &value)
{
    if (!qFuzzyCompare(edgeSize(), value)) {
        m_vertexRef->setEdgeSize(value);
        m_parentModelRef->setDirty(true);
        emit edgeSizeChanged();
    }
}

VertexRefObject::Type VertexRefObject::type() const
{
    return static_cast<VertexRefObject::Type>(m_vertexRef->type());
}

void VertexRefObject::setType(const Type &value)
{
    if (type() != value) {
        m_vertexRef->setType(static_cast<IVertex::Type>(value));
        m_parentModelRef->setDirty(true);
        emit typeChanged();
    }
}
