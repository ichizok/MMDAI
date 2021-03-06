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
#include "vpvl2/internal/ModelHelper.h"

#include "vpvl2/pmx/Bone.h"
#include "vpvl2/pmx/Joint.h"
#include "vpvl2/pmx/Label.h"
#include "vpvl2/pmx/Material.h"
#include "vpvl2/pmx/Model.h"
#include "vpvl2/pmx/Morph.h"
#include "vpvl2/pmx/RigidBody.h"
#include "vpvl2/pmx/SoftBody.h"
#include "vpvl2/pmx/Vertex.h"
#include "vpvl2/internal/ParallelProcessors.h"

#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace {

using namespace vpvl2::VPVL2_VERSION_NS;

#pragma pack(push, 1)

struct Header
{
    uint8 signature[4];
    float32 version;
};

struct Flags
{
    uint8 codec;
    uint8 additionalUVSize;
    uint8 vertexIndexSize;
    uint8 textureIndexSize;
    uint8 materialIndexSize;
    uint8 boneIndexSize;
    uint8 morphIndexSize;
    uint8 rigidBodyIndexSize;
    void clamp() {
        btClamp(codec, uint8(0), uint8(1));
        btClamp(additionalUVSize, uint8(0), uint8(4));
        btClamp(vertexIndexSize, uint8(1), uint8(4));
        btClamp(textureIndexSize, uint8(1), uint8(4));
        btClamp(materialIndexSize, uint8(1), uint8(4));
        btClamp(boneIndexSize, uint8(1), uint8(4));
        btClamp(morphIndexSize, uint8(1), uint8(4));
        btClamp(rigidBodyIndexSize, uint8(1), uint8(4));
    }
    void copy(pmx::Model::DataInfo &info) {
        info.codec = codec == 1 ? IString::kUTF8 : IString::kUTF16;
        info.additionalUVSize = additionalUVSize;
        info.vertexIndexSize = vertexIndexSize;
        info.textureIndexSize = textureIndexSize;
        info.materialIndexSize = materialIndexSize;
        info.boneIndexSize = boneIndexSize;
        info.morphIndexSize = morphIndexSize;
        info.rigidBodyIndexSize = rigidBodyIndexSize;
    }
    static int estimateSize(int value) {
        if (value < 128) {
            return 1;
        }
        else if (value < 32768) {
            return 2;
        }
        else {
            return 4;
        }
    }
};

#pragma pack(pop)

struct DefaultStaticVertexBuffer : public IModel::StaticVertexBuffer {
    typedef btAlignedObjectArray<int32> BoneIndices;
    typedef Array<BoneIndices> MeshBoneIndices;
    typedef Hash<HashInt, int> BoneIndexHash;

    struct Unit {
        Unit() {}
        static Scalar resolveRelativeBoneIndex(const IVertex *vertexRef, int offset, const BoneIndexHash *boneIndexHashes) {
            if (const IBone *boneRef = vertexRef->boneRef(offset)) {
                const int boneIndex = boneRef->index();
#if 0
                if (const int *boneIndexPtr = boneIndexHashes->find(boneIndex)) {
                    const int relativeBoneIndex = *boneIndexPtr;
                    return Scalar(relativeBoneIndex);
                }
#else
                (void) boneIndexHashes;
                return Scalar(boneIndex);
#endif
            }
            return -1;
        }
        void update(const IVertex *vertexRef, const BoneIndexHash *boneIndexHashes) {
            for (int i = 0; i < pmx::Vertex::kMaxBones; i++) {
                boneIndices[i] = resolveRelativeBoneIndex(vertexRef, i, boneIndexHashes);
                boneWeights[i] = Scalar(vertexRef->weight(i));
            }
            texcoord = vertexRef->textureCoord();
        }
        Vector3 texcoord;
        Vector4 boneIndices;
        Vector4 boneWeights;
    };
    static const Unit kIdent;

    struct Predication {
        inline bool operator()(const int left, const int right) const {
            return left < right;
        }
    };
    static void addBoneIndices(const IVertex *vertexRef, const int nindices, BoneIndices &indices) {
        VPVL2_DCHECK(indices.size() > 0);
        Predication predication;
        int size = indices.size();
        for (int i = 0; i < nindices; i++) {
            int index = vertexRef->boneRef(i)->index();
            if (indices.findBinarySearch(index) == size) {
                indices.push_back(index);
                indices.quickSort(predication);
                size = indices.size();
            }
        }
    }
    static void addBoneIndices(const IVertex *vertexRef, BoneIndices &boneIndices) {
        switch (vertexRef->type()) {
        case IVertex::kBdef1:
            addBoneIndices(vertexRef, 1, boneIndices);
            break;
        case IVertex::kBdef2:
        case IVertex::kSdef:
            addBoneIndices(vertexRef, 2, boneIndices);
            break;
        case IVertex::kBdef4:
        case IVertex::kQdef:
            addBoneIndices(vertexRef, 4, boneIndices);
            break;
        case IVertex::kMaxType:
        default:
            break;
        }
    }
    static void updateBoneIndexHashes(const pmx::Model *modelRef, PointerArray<BoneIndexHash> &boneIndexHashes) {
        const Array<pmx::Material *> &materialRefs = modelRef->materials();
        const Array<pmx::Vertex *> &verticeRefs = modelRef->vertices();
        const Array<int> &indices = modelRef->indices();
        BoneIndices boneIndices;
        boneIndexHashes.releaseAll();
        const int nmaterials = materialRefs.count();
        for (int i = 0, offset = 0; i < nmaterials; i++) {
            const IMaterial *materialRef = materialRefs[i];
            const int nindices = materialRef->indexRange().count;
            boneIndices.clear();
            boneIndices.push_back(-1);
            for (int j = 0; j < nindices; j++) {
                const int vertexIndex = indices[offset + j];
                const IVertex *vertexRef = verticeRefs[vertexIndex];
                addBoneIndices(vertexRef, boneIndices);
            }
            const int numBoneIndices = boneIndices.size();
            BoneIndexHash *boneIndexHash = boneIndexHashes.append(new BoneIndexHash());
            for (int j = 0; j < numBoneIndices; j++) {
                const int boneIndex = boneIndices[j];
                boneIndexHash->insert(boneIndex, j);
            }
            offset += nindices;
        }
    }

    DefaultStaticVertexBuffer(const pmx::Model *model)
        : modelRef(model)
    {
    }
    ~DefaultStaticVertexBuffer() {
        boneIndexHashes.releaseAll();
        modelRef = 0;
    }

    vsize size() const {
        return strideSize() * modelRef->vertices().count();
    }
    vsize strideOffset(StrideType type) const {
        static const uint8 *base = reinterpret_cast<const uint8 *>(&kIdent.texcoord);
        switch (type) {
        case kBoneIndexStride:
            return reinterpret_cast<const uint8 *>(&kIdent.boneIndices) - base;
        case kBoneWeightStride:
            return reinterpret_cast<const uint8 *>(&kIdent.boneWeights) - base;
        case kTextureCoordStride:
            return reinterpret_cast<const uint8 *>(&kIdent.texcoord) - base;
        case kVertexStride:
        case kNormalStride:
        case kMorphDeltaStride:
        case kEdgeSizeStride:
        case kEdgeVertexStride:
        case kUVA1Stride:
        case kUVA2Stride:
        case kUVA3Stride:
        case kUVA4Stride:
        case kVertexIndexStride:
        case kIndexStride:
        default:
            return 0;
        }
    }
    vsize strideSize() const {
        return sizeof(kIdent);
    }
    void update(void *address) const {
        const Array<pmx::Material *> &materials = modelRef->materials();
        const Array<int> &indices = modelRef->indices();
        const Array<pmx::Vertex *> &vertices = modelRef->vertices();
        const int nmaterials = materials.count();
        Unit *unitPtr = static_cast<Unit *>(address);
        updateBoneIndexHashes(modelRef, boneIndexHashes);
        for (int i = 0, offset = 0; i < nmaterials; i++) {
            const IMaterial *materialRef = materials[i];
            const BoneIndexHash *boneIndexHashRef = boneIndexHashes[i];
            const int nindices = materialRef->indexRange().count;
            for (int j = 0; j < nindices; j++) {
                const int vertexIndex = indices[offset + j];
                const IVertex *vertex = vertices[vertexIndex];
                unitPtr[vertexIndex].update(vertex, boneIndexHashRef);
            }
            offset += nindices;
        }
        boneIndexHashes.releaseAll();
    }
    const void *ident() const {
        return &kIdent;
    }

    const pmx::Model *modelRef;
    mutable PointerArray<BoneIndexHash> boneIndexHashes;
};
const DefaultStaticVertexBuffer::Unit DefaultStaticVertexBuffer::kIdent = DefaultStaticVertexBuffer::Unit();

struct DefaultDynamicVertexBuffer : public IModel::DynamicVertexBuffer {
    struct Unit {
        Unit() {}
        void initialize(const IVertex *vertex) {
            position = edge = vertex->origin();
            position[3] = edge[3] = Scalar(vertex->type());
            normal = vertex->normal();
            normal[3] = Scalar(vertex->edgeSize());
            setUVA(vertex);
        }
        void setPosition(const IVertex *vertex) {
            position = edge = vertex->origin() + vertex->delta();
            position[3] = edge[3] = Scalar(vertex->type());
        }
        void performTransform(const IVertex *vertex, const IVertex::EdgeSizePrecision &materialEdgeSize, Vector3 &p) {
            Vector3 n;
            const IVertex::EdgeSizePrecision &edgeSize = vertex->edgeSize() * materialEdgeSize;
            vertex->performSkinning(p, n);
            position = p;
            normal = n;
            edge = p + n * Scalar(edgeSize);
            setUVA(vertex);
        }
        inline void setUVA(const IVertex *vertex) {
            uva1 = vertex->uv(0);
            uva2 = vertex->uv(1);
            uva3 = vertex->uv(2);
            uva4 = vertex->uv(3);
        }
        Vector3 position;
        Vector3 normal;
        Vector3 edge;
        Vector4 uva1;
        Vector4 uva2;
        Vector4 uva3;
        Vector4 uva4;
    };
    static const Unit kIdent;

    DefaultDynamicVertexBuffer(const pmx::Model *model, const IModel::IndexBuffer *indexBuffer)
        : modelRef(model),
          indexBufferRef(indexBuffer),
          enableParallelUpdate(false)
    {
    }
    ~DefaultDynamicVertexBuffer() {
        modelRef = 0;
        indexBufferRef = 0;
        enableParallelUpdate = false;
    }

    vsize size() const {
        return strideSize() * modelRef->vertices().count();
    }
    vsize strideOffset(StrideType type) const {
        static const uint8 *base = reinterpret_cast<const uint8 *>(&kIdent.position);
        switch (type) {
        case kVertexStride:
            return reinterpret_cast<const uint8 *>(&kIdent.position) - base;
        case kNormalStride:
            return reinterpret_cast<const uint8 *>(&kIdent.normal) - base;
        case kEdgeVertexStride:
            return reinterpret_cast<const uint8 *>(&kIdent.edge) - base;
        case kEdgeSizeStride:
            return reinterpret_cast<const uint8 *>(&kIdent.normal[3]) - base;
        case kUVA1Stride:
            return reinterpret_cast<const uint8 *>(&kIdent.uva1) - base;
        case kUVA2Stride:
            return reinterpret_cast<const uint8 *>(&kIdent.uva2) - base;
        case kUVA3Stride:
            return reinterpret_cast<const uint8 *>(&kIdent.uva3) - base;
        case kUVA4Stride:
            return reinterpret_cast<const uint8 *>(&kIdent.uva4) - base;
        case kBoneIndexStride:
        case kBoneWeightStride:
        case kTextureCoordStride:
        case kIndexStride:
        case kVertexIndexStride:
        case kMorphDeltaStride:
        default:
            return 0;
        }
    }
    vsize strideSize() const {
        return sizeof(kIdent);
    }
    const void *ident() const {
        return &kIdent;
    }
    void setupBindPose(void *address) const {
        const Array<pmx::Vertex *> &verticeRefs = modelRef->vertices();
        internal::ParallelBindPoseVertexProcessor<pmx::Model, pmx::Vertex, Unit> processor(&verticeRefs, address);
        processor.execute(enableParallelUpdate);
    }
    void update(void *address) const {
        const Array<pmx::Vertex *> &vertices = modelRef->vertices();
        internal::ParallelVertexMorphProcessor<pmx::Model, pmx::Vertex, Unit> processor(&vertices, address);
        processor.execute(enableParallelUpdate);
    }
    void performTransform(void *address, const Vector3 &cameraPosition) const {
        const Array<pmx::Vertex *> &verticeRefs = modelRef->vertices();
        Unit *bufferPtr = static_cast<Unit *>(address);
        internal::ParallelSkinningVertexProcessor<pmx::Model, pmx::Vertex, Unit> processor(modelRef, &verticeRefs, cameraPosition, bufferPtr);
        processor.execute(enableParallelUpdate);
    }
    void computeAabb(const void *address, Array<Vector3> &values) const {
        const Array<pmx::Material *> &materials = modelRef->materials();
        internal::ParallelComputeAabbProcessor<pmx::Material, Unit> processor(&materials, &values, address);
        processor.execute(enableParallelUpdate);
    }
    void setParallelUpdateEnable(bool value) {
        enableParallelUpdate = value;
    }

    const pmx::Model *modelRef;
    const IModel::IndexBuffer *indexBufferRef;
    bool enableParallelUpdate;
};
const DefaultDynamicVertexBuffer::Unit DefaultDynamicVertexBuffer::kIdent = DefaultDynamicVertexBuffer::Unit();

struct DefaultIndexBuffer : public IModel::IndexBuffer {
    static const int kIdent = 0;
    DefaultIndexBuffer(const Array<int> &indices, const int nvertices)
        : indexType(kIndex32),
          indices32Ptr(0),
          nindices(indices.count())
    {
        if (nindices < 65536) {
            indexType = kIndex16;
            indices16Ptr = new uint16[nindices];
        }
        else if (nindices < 256) {
            indexType = kIndex8;
            indices8Ptr = new uint8[nindices];
        }
        else {
            indices32Ptr = new int[nindices];
        }
        for (int i = 0; i < nindices; i++) {
            const int index = indices[i];
            if (index >= 0 && index < nvertices) {
                setIndexAt(i, index);
            }
            else {
                setIndexAt(i, 0);
            }
        }
#ifdef VPVL2_COORDINATE_OPENGL
        switch (indexType) {
        case kIndex32:
            internal::ModelHelper::swapIndices(indices32Ptr, nindices);
            break;
        case kIndex16:
            internal::ModelHelper::swapIndices(indices16Ptr, nindices);
            break;
        case kIndex8:
            internal::ModelHelper::swapIndices(indices8Ptr, nindices);
            break;
        case kMaxIndexType:
        default:
            break;
        }
#endif
    }
    ~DefaultIndexBuffer() {
        switch (indexType) {
        case kIndex32:
            internal::deleteObjectArray(indices32Ptr);
            break;
        case kIndex16:
            internal::deleteObjectArray(indices16Ptr);
            break;
        case kIndex8:
            internal::deleteObjectArray(indices8Ptr);
            break;
        case kMaxIndexType:
        default:
            break;
        }
    }

    const void *bytes() const {
        switch (indexType) {
        case kIndex32:
            return indices32Ptr;
        case kIndex16:
            return indices16Ptr;
        case kIndex8:
            return indices8Ptr;
        case kMaxIndexType:
        default:
            return 0;
        }
    }
    vsize size() const {
        return strideSize() * nindices;
    }
    vsize strideOffset(StrideType /* type */) const {
        return 0;
    }
    vsize strideSize() const {
        switch (indexType) {
        case kIndex32:
            return sizeof(int32);
        case kIndex16:
            return sizeof(uint16);
        case kIndex8:
            return sizeof(uint8);
        case kMaxIndexType:
        default:
            return 0;
        }
    }
    const void *ident() const {
        return &kIdent;
    }
    int indexAt(int index) const {
        switch (indexType) {
        case kIndex32:
            return indices32Ptr[index];
        case kIndex16:
            return indices16Ptr[index];
        case kIndex8:
            return indices8Ptr[index];
        case kMaxIndexType:
        default:
            return 0;
        }
    }
    Type type() const {
        return indexType;
    }

    void setIndexAt(int i, int value) {
        switch (indexType) {
        case kIndex32:
            indices32Ptr[i] = value;
            break;
        case kIndex16:
            indices16Ptr[i] = uint16(value);
            break;
        case kIndex8:
            indices8Ptr[i] = uint8(value);
            break;
        case kMaxIndexType:
        default:
            break;
        }
    }
    IModel::IndexBuffer::Type indexType;
    union {
        int32  *indices32Ptr;
        uint16 *indices16Ptr;
        uint8  *indices8Ptr;
    };
    int nindices;
};
const int DefaultIndexBuffer::kIdent;

struct DefaultMatrixBuffer : public IModel::MatrixBuffer {
    typedef btAlignedObjectArray<Transform> MeshLocalTransforms;
    typedef PointerArray<float32> MeshMatrices;
    struct SkinningMeshes {
        DefaultStaticVertexBuffer::MeshBoneIndices bones;
        MeshMatrices matrices;
        ~SkinningMeshes() { matrices.releaseArrayAll(); }
    };

    DefaultMatrixBuffer(const pmx::Model *model,
                        const DefaultIndexBuffer *indexBuffer,
                        DefaultDynamicVertexBuffer *dynamicBuffer)
        : modelRef(model),
          indexBufferRef(indexBuffer),
          dynamicBufferRef(dynamicBuffer)
    {
        initialize();
    }
    ~DefaultMatrixBuffer() {
        modelRef = 0;
        indexBufferRef = 0;
        dynamicBufferRef = 0;
    }

    void updateBoneLocalTransforms() {
        const Array<pmx::Bone *> &boneRefs = modelRef->bones();
        const Array<pmx::Material *> &materialRefs = modelRef->materials();
        const Transform &staticBoneLocalTransform = Factory::sharedNullBoneRef()->localTransform();
        const int nmaterials = materialRefs.count();
        for (int i = 0; i < nmaterials; i++) {
            const DefaultStaticVertexBuffer::BoneIndices &boneIndices = meshes.bones[i];
            const int numBoneIndices = boneIndices.size();
            float32 *matrices = meshes.matrices[i];
            staticBoneLocalTransform.getOpenGLMatrix(matrices);
            for (int j = 1; j < numBoneIndices; j++) {
                const int boneIndex = boneIndices[j];
                const Transform &localBoneTransform = boneRefs[boneIndex]->localTransform();
                localBoneTransform.getOpenGLMatrix(&matrices[j * 16]);
            }
        }
    }

    void update(void * /* address */) {
        updateBoneLocalTransforms();
    }
    const float32 *bytes(int materialIndex) const {
        int nmatrices = meshes.matrices.count();
        return internal::checkBound(materialIndex, 0, nmatrices) ? meshes.matrices[materialIndex] : 0;
    }
    vsize size(int materialIndex) const {
        int nbones = meshes.bones.count();
        return internal::checkBound(materialIndex, 0, nbones) ? meshes.bones[materialIndex].size() : 0;
    }

    void initialize() {
        const Array<pmx::Material *> &materialRefs = modelRef->materials();
        const Array<pmx::Vertex *> &verticeRefs = modelRef->vertices();
        const Array<int> &indices = modelRef->indices();
        const int nmaterials = materialRefs.count();
        DefaultStaticVertexBuffer::BoneIndices boneIndices;
        meshes.bones.reserve(nmaterials);
        meshes.matrices.reserve(nmaterials);
        int offset = 0;
        for (int i = 0; i < nmaterials; i++) {
            const IMaterial *materialRef = materialRefs[i];
            const int nindices = materialRef->indexRange().count;
            boneIndices.clear();
            boneIndices.push_back(-1);
            for (int j = 0; j < nindices; j++) {
                const int vertexIndex = indices[offset + j];
                const IVertex *vertexRef = verticeRefs[vertexIndex];
                DefaultStaticVertexBuffer::addBoneIndices(vertexRef, boneIndices);
            }
            const vsize size = boneIndices.size() * 16;
            meshes.matrices.append(new float32[size]);
            meshes.bones.append(boneIndices);
            offset += nindices;
        }
        updateBoneLocalTransforms();
    }

    const pmx::Model *modelRef;
    const DefaultIndexBuffer *indexBufferRef;
    DefaultDynamicVertexBuffer *dynamicBufferRef;
    SkinningMeshes meshes;
};

}

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{
namespace pmx
{

struct Model::PrivateContext {
    PrivateContext(IEncoding *encoding, Model *self)
        : encodingRef(encoding),
          selfRef(self),
          parentSceneRef(0),
          parentModelRef(0),
          parentBoneRef(0),
          progressReporterRef(0),
          namePtr(0),
          englishNamePtr(0),
          commentPtr(0),
          englishCommentPtr(0),
          codec(IString::kUTF8),
          edgeColor(kZeroV3),
          aabbMax(kZeroV3),
          aabbMin(kZeroV3),
          position(kZeroV3),
          rotation(Quaternion::getIdentity()),
          opacity(1),
          scaleFactor(1),
          edgeWidth(0),
          visible(false),
          enablePhysics(false)
    {
        internal::zerofill(&dataInfo, sizeof(dataInfo));
        dataInfo.encoding = encodingRef;
        dataInfo.version = 2.0f;
    }
    ~PrivateContext() {
    }

    void release() {
        /* release objects order by reference dependency (no or child dependency first) */
        vertices.releaseAll();
        materials.releaseAll();
        textures.releaseAll();
        labels.releaseAll();
        morphs.releaseAll();
        joints.releaseAll();
        rigidBodies.releaseAll();
        bones.releaseAll();
        internal::zerofill(&dataInfo, sizeof(dataInfo));
        dataInfo.encoding = encodingRef;
        dataInfo.version = 2.0f;
        internal::deleteObject(namePtr);
        internal::deleteObject(englishNamePtr);
        internal::deleteObject(commentPtr);
        internal::deleteObject(englishCommentPtr);
        parentSceneRef = 0;
        parentModelRef = 0;
        parentBoneRef = 0;
        edgeColor.setZero();
        aabbMin.setZero();
        aabbMax.setZero();
        position.setZero();
        rotation.setValue(0, 0, 0, 1);
        opacity = 1;
        scaleFactor = 1;
    }
    void parseNamesAndComments(const Model::DataInfo &info) {
        IEncoding *encoding = info.encoding;
        codec = info.codec;
        internal::setStringDirect(encoding->toString(info.namePtr, info.nameSize, info.codec), namePtr);
        internal::setStringDirect(encodingRef->toString(info.englishNamePtr, info.englishNameSize, info.codec), englishNamePtr);
        internal::setStringDirect(encodingRef->toString(info.commentPtr, info.commentSize, info.codec), commentPtr);
        internal::setStringDirect(encodingRef->toString(info.englishCommentPtr, info.englishCommentSize, info.codec), englishCommentPtr);
    }
    void parseVertices(const Model::DataInfo &info) {
        const int nvertices = int(info.verticesCount);
        uint8 *ptr = info.verticesPtr;
        vsize size;
        for (int i = 0; i < nvertices; i++) {
            Vertex *vertex = vertices.append(new Vertex(selfRef));
            vertex->read(ptr, info, size);
            ptr += size;
        }
    }
    void parseIndices(const Model::DataInfo &info) {
        const int nindices = int(info.indicesCount), nvertices = int(info.verticesCount);
        uint8 *ptr = info.indicesPtr;
        vsize size = info.vertexIndexSize;
        for (int i = 0; i < nindices; i++) {
            int index = internal::readUnsignedIndex(ptr, size);
            if (internal::checkBound(index, 0, nvertices)) {
                indices.append(index);
            }
            else {
                indices.append(0);
            }
        }
    }
    void parseTextures(const Model::DataInfo &info) {
        const int ntextures = int(info.texturesCount);
        vsize rest = SIZE_MAX;
        uint8 *ptr = info.texturesPtr;
        uint8 *texturePtr = 0;
        int size;
        for (int i = 0; i < ntextures; i++) {
            internal::getText(ptr, rest, texturePtr, size);
            IString *value = encodingRef->toString(texturePtr, size, info.codec);
            const HashString &key = value->toHashString();
            name2textureRefs.insert(key, value);
            textures.append(value);
        }
    }
    void parseMaterials(const Model::DataInfo &info) {
        const int nmaterials = int(info.materialsCount), nindices = indices.count();
        int offset = 0;
        uint8 *ptr = info.materialsPtr;
        vsize size;
        for (int i = 0; i < nmaterials; i++) {
            Material *material = materials.append(new Material(selfRef));
            material->read(ptr, info, size);
            ptr += size;
            IMaterial::IndexRange range = material->indexRange();
            int offsetTo = offset + range.count;
            if (offset < offsetTo && offsetTo <= nindices) {
                range.start = nindices;
                range.end = 0;
                for (int j = offset; j < offsetTo; j++) {
                    const int index = indices.at(j);
                    IVertex *vertex = vertices[index];
                    vertex->setMaterialRef(material);
                    btSetMin(range.start, index);
                    btSetMax(range.end, index);
                }
            }
            else {
                range.count = 0;
            }
            material->setIndexRange(range);
            offset = offsetTo;
        }
    }
    void parseBones(const Model::DataInfo &info) {
        const int nbones = int(info.bonesCount);
        uint8 *ptr = info.bonesPtr;
        vsize size;
        for (int i = 0; i < nbones; i++) {
            Bone *bone = bones.append(new Bone(selfRef));
            bone->read(ptr, info, size);
            name2boneRefs.insert(bone->name(IEncoding::kJapanese)->toHashString(), bone);
            name2boneRefs.insert(bone->name(IEncoding::kEnglish)->toHashString(), bone);
            ptr += size;
        }
    }
    void parseMorphs(const Model::DataInfo &info) {
        const int nmorphs = int(info.morphsCount);
        uint8 *ptr = info.morphsPtr;
        vsize size;
        for(int i = 0; i < nmorphs; i++) {
            Morph *morph = morphs.append(new Morph(selfRef));
            morph->read(ptr, info, size);
            name2morphRefs.insert(morph->name(IEncoding::kJapanese)->toHashString(), morph);
            name2morphRefs.insert(morph->name(IEncoding::kEnglish)->toHashString(), morph);
            ptr += size;
        }
    }
    void parseLabels(const Model::DataInfo &info) {
        const int nlabels = int(info.labelsCount);
        uint8 *ptr = info.labelsPtr;
        vsize size;
        for(int i = 0; i < nlabels; i++) {
            Label *label = labels.append(new Label(selfRef));
            label->read(ptr, info, size);
            ptr += size;
        }
    }
    void parseRigidBodies(const Model::DataInfo &info) {
        const int numRigidBodies = int(info.rigidBodiesCount);
        uint8 *ptr = info.rigidBodiesPtr;
        vsize size;
        for(int i = 0; i < numRigidBodies; i++) {
            RigidBody *rigidBody = rigidBodies.append(new RigidBody(selfRef, encodingRef));
            rigidBody->read(ptr, info, size);
            ptr += size;
        }
    }
    void parseJoints(const Model::DataInfo &info) {
        const int njoints = int(info.jointsCount);
        uint8 *ptr = info.jointsPtr;
        vsize size;
        for(int i = 0; i < njoints; i++) {
            Joint *joint = joints.append(new Joint(selfRef));
            joint->read(ptr, info, size);
            ptr += size;
        }
    }
    void parseSoftBodies(const Model::DataInfo &info) {
        if (info.version >= 2.1) {
            const int numSoftBodies = int(info.softBodiesCount);
            uint8 *ptr = info.softBodiesPtr;
            vsize size;
            for(int i = 0; i < numSoftBodies; i++) {
                SoftBody *body = softBodies.append(new SoftBody(selfRef));
                body->read(ptr, info, size);
                ptr += size;
            }
        }
    }
    void assignIndexSize(Model::DataInfo &info) const {
        info.boneIndexSize = Flags::estimateSize(bones.count());
        info.materialIndexSize = Flags::estimateSize(materials.count());
        info.morphIndexSize = Flags::estimateSize(morphs.count());
        info.rigidBodyIndexSize = Flags::estimateSize(rigidBodies.count());
        info.textureIndexSize = Flags::estimateSize(name2textureRefs.count());
        info.vertexIndexSize = Flags::estimateSize(vertices.count());
    }
    void assignIndexSize(Flags &flags) const {
        flags.boneIndexSize = Flags::estimateSize(bones.count());
        flags.materialIndexSize = Flags::estimateSize(materials.count());
        flags.morphIndexSize = Flags::estimateSize(morphs.count());
        flags.rigidBodyIndexSize = Flags::estimateSize(rigidBodies.count());
        flags.textureIndexSize = Flags::estimateSize(name2textureRefs.count());
        flags.vertexIndexSize = Flags::estimateSize(vertices.count());
    }
    void reportProgress(float value) const {
        if (progressReporterRef) {
            progressReporterRef->reportProgress(value);
        }
    }

    IEncoding *encodingRef;
    Model *selfRef;
    Scene *parentSceneRef;
    IModel *parentModelRef;
    IBone *parentBoneRef;
    IProgressReporter *progressReporterRef;
    PointerArray<Vertex> vertices;
    Array<int> indices;
    PointerArray<IString> textures;
    Hash<HashString, IString *> name2textureRefs;
    PointerArray<Material> materials;
    PointerArray<Bone> bones;
    Array<Bone *> bonesBeforePhysics;
    Array<Bone *> bonesAfterPhysics;
    PointerArray<Morph> morphs;
    PointerArray<Label> labels;
    PointerArray<RigidBody> rigidBodies;
    PointerArray<Joint> joints;
    PointerArray<SoftBody> softBodies;
    Hash<HashString, IBone *> name2boneRefs;
    Hash<HashString, IMorph *> name2morphRefs;
    IString *namePtr;
    IString *englishNamePtr;
    IString *commentPtr;
    IString *englishCommentPtr;
    IString::Codec codec;
    Vector3 edgeColor;
    Vector3 aabbMax;
    Vector3 aabbMin;
    Vector3 position;
    Quaternion rotation;
    Scalar opacity;
    Scalar scaleFactor;
    IVertex::EdgeSizePrecision edgeWidth;
    DataInfo dataInfo;
    bool visible;
    bool enablePhysics;
};

Model::Model(IEncoding *encoding)
    : m_context(new PrivateContext(encoding, this))
{
}

Model::~Model()
{
    m_context->release();
    internal::deleteObject(m_context);
}

bool Model::load(const uint8 *data, vsize size)
{
    DataInfo info;
    internal::zerofill(&info, sizeof(info));
    if (preparse(data, size, info)) {
#define VPVL2_CALCULATE_PROGRESS_PERCENTAGE(value) (value / 15.0)
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(1));
        m_context->release();
        m_context->parseNamesAndComments(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(2));
        m_context->parseVertices(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(3));
        m_context->parseIndices(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(4));
        m_context->parseTextures(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(5));
        m_context->parseMaterials(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(6));
        m_context->parseBones(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(7));
        m_context->parseMorphs(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(8));
        m_context->parseLabels(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(9));
        m_context->parseRigidBodies(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(10));
        m_context->parseJoints(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(11));
        m_context->parseSoftBodies(info);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(12));
        if (!Bone::loadBones(m_context->bones)
                || !Material::loadMaterials(m_context->materials, m_context->textures, m_context->indices.count())
                || !Vertex::loadVertices(m_context->vertices, m_context->bones)
                || !Morph::loadMorphs(m_context->morphs, m_context->bones, m_context->materials, m_context->rigidBodies, m_context->vertices)
                || !Label::loadLabels(m_context->labels, m_context->bones, m_context->morphs)
                || !RigidBody::loadRigidBodies(m_context->rigidBodies, m_context->bones)
                || !Joint::loadJoints(m_context->joints, m_context->rigidBodies)
                || !SoftBody::loadSoftBodies(m_context->softBodies)) {
            m_context->dataInfo.error = info.error;
            return false;
        }
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(13));
        Bone::sortBones(m_context->bones, m_context->bonesBeforePhysics, m_context->bonesAfterPhysics);
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(14));
        performUpdate();
        m_context->reportProgress(VPVL2_CALCULATE_PROGRESS_PERCENTAGE(15));
        m_context->dataInfo = info;
#undef VPVL2_CALCULATE_PROGRESS_PERCENTAGE
        return true;
    }
    else {
        m_context->dataInfo.error = info.error;
    }
    return false;
}

void Model::save(uint8 *data, vsize &written) const
{
    Header header;
    uint8 *base = data;
    uint8 *signature = reinterpret_cast<uint8 *>(header.signature);
    internal::writeBytes("PMX ", sizeof(header.signature), signature);
    header.version = m_context->dataInfo.version;
    internal::writeBytes(&header, sizeof(header), data);
    IEncoding *encodingRef = m_context->encodingRef;
    IString::Codec codec = m_context->codec;
    Flags flags;
    DataInfo info = m_context->dataInfo;
    flags.codec = (codec == IString::kUTF8) ? 1 : 0;
    flags.additionalUVSize = uint8(info.additionalUVSize);
    m_context->assignIndexSize(info);
    m_context->assignIndexSize(flags);
    uint8 flagSize = sizeof(flags);
    internal::writeBytes(&flagSize, sizeof(flagSize), data);
    internal::writeBytes(&flags, sizeof(flags), data);
    internal::writeString(m_context->namePtr, encodingRef, codec, data);
    internal::writeString(m_context->englishNamePtr, encodingRef, codec, data);
    internal::writeString(m_context->commentPtr, encodingRef, codec, data);
    internal::writeString(m_context->englishCommentPtr, encodingRef, codec, data);
    Vertex::writeVertices(m_context->vertices, info, data);
    const int nindices = m_context->indices.count();
    internal::writeBytes(&nindices, sizeof(nindices), data);
    for (int i = 0; i < nindices; i++) {
        const int index = m_context->indices[i];
        internal::writeSignedIndex(index, flags.vertexIndexSize, data);
    }
    const int ntextures = m_context->textures.count();
    internal::writeBytes(&ntextures, sizeof(ntextures), data);
    for (int i = 0; i < ntextures; i++) {
        const IString *texture = m_context->textures[i];
        internal::writeString(texture, encodingRef, codec, data);
    }
    Material::writeMaterials(m_context->materials, info, data);
    Bone::writeBones(m_context->bones, info, data);
    Morph::writeMorphs(m_context->morphs, info, data);
    Label::writeLabels(m_context->labels, info, data);
    RigidBody::writeRigidBodies(m_context->rigidBodies, info, data);
    Joint::writeJoints(m_context->joints, info, data);
    SoftBody::writeSoftBodies(m_context->softBodies, info, data);
    written = data - base;
    VPVL2_VLOG(1, "PMXEOF: base=" << reinterpret_cast<const void *>(base) << " data=" << reinterpret_cast<const void *>(data) << " written=" << written);
}

vsize Model::estimateSize() const
{
    vsize size = 0;
    DataInfo info = m_context->dataInfo;
    IEncoding *encodingRef = m_context->encodingRef;
    m_context->assignIndexSize(info);
    IString::Codec codec = m_context->codec;
    size += sizeof(Header);
    size += sizeof(uint8) + sizeof(Flags);
    size += internal::estimateSize(m_context->namePtr, encodingRef, codec);
    size += internal::estimateSize(m_context->englishNamePtr, encodingRef, codec);
    size += internal::estimateSize(m_context->commentPtr, encodingRef, codec);
    size += internal::estimateSize(m_context->englishCommentPtr, encodingRef, codec);
    size += Vertex::estimateTotalSize(m_context->vertices, info);
    const int nindices = m_context->indices.count();
    size += sizeof(nindices);
    size += info.vertexIndexSize * nindices;
    const int ntextures = m_context->textures.count();
    size += sizeof(ntextures);
    for (int i = 0; i < ntextures; i++) {
        const IString *texture = m_context->textures[i];
        size += internal::estimateSize(texture, encodingRef, codec);
    }
    size += Material::estimateTotalSize(m_context->materials, info);
    size += Bone::estimateTotalSize(m_context->bones, info);
    size += Morph::estimateTotalSize(m_context->morphs, info);
    size += Label::estimateTotalSize(m_context->labels, info);
    size += RigidBody::estimateTotalSize(m_context->rigidBodies, info);
    size += Joint::estimateTotalSize(m_context->joints, info);
    size += SoftBody::estimateTotalSize(m_context->softBodies, info);
    return size;
}

void Model::joinWorld(btDiscreteDynamicsWorld *worldRef)
{
    if (worldRef) {
        const int numRigidBodies = m_context->rigidBodies.count();
        for (int i = 0; i < numRigidBodies; i++) {
            RigidBody *rigidBody = m_context->rigidBodies[i];
            rigidBody->joinWorld(worldRef);
        }
        const int njoints = m_context->joints.count();
        for (int i = 0; i < njoints; i++) {
            Joint *joint = m_context->joints[i];
            joint->joinWorld(worldRef);
        }
    }
}

void Model::leaveWorld(btDiscreteDynamicsWorld *worldRef)
{
    if (worldRef) {
        const int numRigidBodies = m_context->rigidBodies.count();
        for (int i = numRigidBodies - 1; i >= 0; i--) {
            RigidBody *rigidBody = m_context->rigidBodies[i];
            rigidBody->leaveWorld(worldRef);
        }
        const int njoints = m_context->joints.count();
        for (int i = njoints - 1; i >= 0; i--) {
            Joint *joint = m_context->joints[i];
            joint->leaveWorld(worldRef);
        }
    }
}

void Model::resetMotionState(btDiscreteDynamicsWorld *worldRef)
{
    if (worldRef) {
        /* update worldTransform first to use it at RigidBody#setKinematic */
        const int nbones = m_context->bonesBeforePhysics.count();
        for (int i = 0; i < nbones; i++) {
            Bone *bone = m_context->bonesBeforePhysics[i];
            bone->reset();
        }
        updateLocalTransform(m_context->bonesBeforePhysics);
        const int numRigidBodies = m_context->rigidBodies.count();
        for (int i = 0; i < numRigidBodies; i++) {
            RigidBody *rigidBody = m_context->rigidBodies[i];
            rigidBody->resetBody(worldRef);
            rigidBody->updateTransform();
            rigidBody->setActivation(true);
        }
        const int njoints = m_context->joints.count();
        for (int i = 0; i < njoints; i++) {
            Joint *joint = m_context->joints[i];
            joint->updateTransform();
        }
        updateLocalTransform(m_context->bonesAfterPhysics);
    }
}

void Model::performUpdate()
{
    // update local transform matrix
    const int nbones = m_context->bones.count();
    for (int i = 0; i < nbones; i++) {
        Bone *bone = m_context->bones[i];
        bone->reset();
    }
    const int nmaterials = m_context->materials.count();
    for (int i = 0; i < nmaterials; i++) {
        Material *material = m_context->materials[i];
        material->reset();
    }
    internal::ParallelResetVertexProcessor<pmx::Vertex> processor(&m_context->vertices);
    processor.execute();
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        morph->syncWeight();
    }
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        morph->update();
    }
    // before physics simulation
    updateLocalTransform(m_context->bonesBeforePhysics);
    if (m_context->enablePhysics) {
        // physics simulation
        internal::ParallelUpdateRigidBodyProcessor<pmx::RigidBody> processor(&m_context->rigidBodies);
        processor.execute();
    }
    // after physics simulation
    updateLocalTransform(m_context->bonesAfterPhysics);
}

IBone *Model::findBoneRef(const IString *value) const
{
    if (value) {
        const HashString &key = value->toHashString();
        IBone *const *bone = m_context->name2boneRefs.find(key);
        return bone ? *bone : 0;
    }
    return 0;
}

IMorph *Model::findMorphRef(const IString *value) const
{
    if (value) {
        const HashString &key = value->toHashString();
        IMorph *const *morph = m_context->name2morphRefs.find(key);
        return morph ? *morph : 0;
    }
    return 0;
}

int Model::count(ObjectType value) const
{
    switch (value) {
    case kBone: {
        return m_context->bones.count();
    }
    case kIK: {
        const int nbones = m_context->bones.count();
        int nIK = 0;
        for (int i = 0; i < nbones; i++) {
            Bone *bone = static_cast<Bone *>(m_context->bones[i]);
            if (bone->hasInverseKinematics()) {
                nIK++;
            }
        }
        return nIK;
    }
    case kIndex: {
        return m_context->indices.count();
    }
    case kJoint: {
        return m_context->joints.count();
    }
    case kMaterial: {
        return m_context->materials.count();
    }
    case kMorph: {
        return m_context->morphs.count();
    }
    case kRigidBody: {
        return m_context->rigidBodies.count();
    }
    case kSoftBody: {
        return 0;
    }
    case kTexture: {
        return m_context->name2textureRefs.count();
    }
    case kVertex: {
        return m_context->vertices.count();
    }
    default:
        return 0;
    }
}

void Model::getBoneRefs(Array<IBone *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->bones, value);
}

void Model::getJointRefs(Array<IJoint *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->joints, value);
}
void Model::getLabelRefs(Array<ILabel *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->labels, value);
}

void Model::getMaterialRefs(Array<IMaterial *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->materials, value);
}

void Model::getMorphRefs(Array<IMorph *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->morphs, value);
}

void Model::getRigidBodyRefs(Array<IRigidBody *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->rigidBodies, value);
}

void Model::getTextureRefs(Array<const IString *> &value) const
{
    internal::ModelHelper::getTextureRefs(m_context->name2textureRefs, value);
}

void Model::getVertexRefs(Array<IVertex *> &value) const
{
    internal::ModelHelper::getObjectRefs(m_context->vertices, value);
}

void Model::getIndices(Array<int> &value) const
{
    value.copy(m_context->indices);
}

void Model::getIKConstraintRefs(Array<IBone::IKConstraint *> &value) const
{
    const int nbones = m_context->bones.count();
    value.clear();
    for (int i = 0; i < nbones; i++) {
        Bone *bone = m_context->bones[i];
        if (bone->hasInverseKinematics()) {
            value.append(bone);
        }
    }
}

bool Model::preparse(const uint8 *data, vsize size, DataInfo &info)
{
    vsize rest = size;
    if (!data || sizeof(Header) > rest) {
        VPVL2_LOG(WARNING, "Data is null or PMX header not satisfied: " << size);
        m_context->dataInfo.error = kInvalidHeaderError;
        return false;
    }
    /* header */
    uint8 *ptr = const_cast<uint8 *>(data);
    Header header;
    internal::getData(ptr, header);
    info.basePtr = ptr;
    VPVL2_VLOG(1, "PMXBasePtr: ptr=" << static_cast<const void*>(ptr) << " size=" << size);

    /* Check the signature and version is correct */
    if (std::memcmp(header.signature, "PMX ", 4) != 0) {
        VPVL2_LOG(WARNING, "Invalid PMX signature detected: " << header.signature);
        m_context->dataInfo.error = kInvalidSignatureError;
        return false;
    }

    /* version */
    if (header.version != 2.0) {
        VPVL2_LOG(WARNING, "Invalid PMX version detected: " << header.version);
        m_context->dataInfo.error = kInvalidVersionError;
        return false;
    }
    info.version = header.version;

    /* flags */
    Flags flags;
    uint8 flagSize = 0;
    internal::drainBytes(sizeof(Header), ptr, rest);
    if (!internal::getTyped<uint8>(ptr, rest, flagSize) || flagSize != sizeof(flags)) {
        VPVL2_LOG(WARNING, "Invalid PMX flag size: " << flagSize);
        m_context->dataInfo.error = kInvalidFlagSizeError;
        return false;
    }
    if (!internal::getTyped<Flags>(ptr, rest, flags)) {
        VPVL2_LOG(WARNING, "Invalid PMX flag data: " << flagSize);
        m_context->dataInfo.error = kInvalidFlagSizeError;
        return false;
    }
    flags.clamp();
    flags.copy(info);
    VPVL2_VLOG(1, "PMXFlags(codec): " << info.codec);
    VPVL2_VLOG(1, "PMXFlags(additionalUVSize): " << info.additionalUVSize);
    VPVL2_VLOG(1, "PMXFlags(vertexIndexSize): " << info.vertexIndexSize);
    VPVL2_VLOG(1, "PMXFlags(textureIndexSize): " << info.textureIndexSize);
    VPVL2_VLOG(1, "PMXFlags(materialIndexSize): " << info.materialIndexSize);
    VPVL2_VLOG(1, "PMXFlags(boneIndexSize): " << info.boneIndexSize);
    VPVL2_VLOG(1, "PMXFlags(morphIndexSize): " << info.morphIndexSize);
    VPVL2_VLOG(1, "PMXFlags(rigidBodyIndexSize): " << info.rigidBodyIndexSize);

    /* name in Japanese */
    if (!internal::getText(ptr, rest, info.namePtr, info.nameSize)) {
        VPVL2_LOG(WARNING, "Invalid size of name in Japanese detected: " << info.nameSize);
        m_context->dataInfo.error = kInvalidNameSizeError;
        return false;
    }
    VPVL2_VLOG(1, "PMXName(Japanese): ptr=" << static_cast<const void*>(info.namePtr) << " size=" << info.nameSize);

    /* name in English */
    if (!internal::getText(ptr, rest, info.englishNamePtr, info.englishNameSize)) {
        VPVL2_LOG(WARNING, "Invalid size of name in English detected: " << info.englishNameSize);
        m_context->dataInfo.error = kInvalidEnglishNameSizeError;
        return false;
    }
    VPVL2_VLOG(1, "PMXName(English): ptr=" << static_cast<const void*>(info.englishNamePtr) << " size=" << info.englishNameSize);

    /* comment in Japanese */
    if (!internal::getText(ptr, rest, info.commentPtr, info.commentSize)) {
        VPVL2_LOG(WARNING, "Invalid size of comment in Japanese detected: " << info.commentSize);
        m_context->dataInfo.error = kInvalidCommentSizeError;
        return false;
    }
    VPVL2_VLOG(1, "PMXComment(Japanese): ptr=" << static_cast<const void*>(info.commentPtr) << " size=" << info.commentSize);

    /* comment in English */
    if (!internal::getText(ptr, rest, info.englishCommentPtr, info.englishCommentSize)) {
        VPVL2_LOG(WARNING, "Invalid size of comment in English detected: " << info.englishCommentSize);
        m_context->dataInfo.error = kInvalidEnglishCommentSizeError;
        return false;
    }
    VPVL2_VLOG(1, "PMXComment(English): ptr=" << static_cast<const void*>(info.englishCommentPtr) << " size=" << info.englishCommentSize);

    /* vertex */
    if (!Vertex::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidVerticesError;
        return false;
    }
    VPVL2_VLOG(1, "PMXVertices: ptr=" << static_cast<const void*>(info.verticesPtr) << " size=" << info.verticesCount);

    /* indices */
    int nindices;
    if (!internal::getTyped<int>(ptr, rest, nindices) || nindices * info.vertexIndexSize > rest) {
        m_context->dataInfo.error = kInvalidIndicesError;
        return false;
    }
    info.indicesPtr = ptr;
    info.indicesCount = nindices;
    internal::drainBytes(nindices * info.vertexIndexSize, ptr, rest);
    VPVL2_VLOG(1, "PMXIndices: ptr=" << static_cast<const void*>(info.indicesPtr) << " size=" << info.indicesCount);

    /* texture lookup table */
    int ntextures;
    if (!internal::getTyped<int>(ptr, rest, ntextures)) {
        m_context->dataInfo.error = kInvalidTextureSizeError;
        return false;
    }

    info.texturesPtr = ptr;
    for (int i = 0; i < ntextures; i++) {
        int nNameSize;
        uint8 *namePtr;
        if (!internal::getText(ptr, rest, namePtr, nNameSize)) {
            m_context->dataInfo.error = kInvalidTextureError;
            return false;
        }
    }
    info.texturesCount = ntextures;
    VPVL2_VLOG(1, "PMXTextures: ptr=" << static_cast<const void*>(info.texturesPtr) << " size=" << info.texturesCount);

    /* material */
    if (!Material::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidMaterialsError;
        return false;
    }
    VPVL2_VLOG(1, "PMXMaterials: ptr=" << static_cast<const void*>(info.materialsPtr) << " size=" << info.materialsCount);

    /* bone */
    if (!Bone::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidBonesError;
        return false;
    }
    VPVL2_VLOG(1, "PMXBones: ptr=" << static_cast<const void*>(info.bonesPtr) << " size=" << info.bonesCount);

    /* morph */
    if (!Morph::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidMorphsError;
        return false;
    }
    VPVL2_VLOG(1, "PMXMorphs: ptr=" << static_cast<const void*>(info.morphsPtr) << " size=" << info.morphsCount);

    /* display name table */
    if (!Label::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidLabelsError;
        return false;
    }
    VPVL2_VLOG(1, "PMXLabels: ptr=" << static_cast<const void*>(info.labelsPtr) << " size=" << info.labelsCount);

    /* rigid body */
    if (!RigidBody::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidRigidBodiesError;
        return false;
    }
    VPVL2_VLOG(1, "PMXRigidBodies: ptr=" << static_cast<const void*>(info.rigidBodiesPtr) << " size=" << info.rigidBodiesCount);

    /* constraint */
    if (!Joint::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidJointsError;
        return false;
    }
    VPVL2_VLOG(1, "PMXJoints: ptr=" << static_cast<const void*>(info.jointsPtr) << " size=" << info.jointsCount);

    /* softbody */
    if (!SoftBody::preparse(ptr, rest, info)) {
        m_context->dataInfo.error = kInvalidSoftBodiesError;
        return false;
    }
    VPVL2_VLOG(1, "PMXSoftBodies: ptr=" << static_cast<const void*>(info.softBodiesPtr) << " size=" << info.softBodiesCount);

    info.endPtr = ptr;
    info.encoding = m_context->encodingRef;
    VPVL2_VLOG(1, "PMXEOD: ptr=" << static_cast<const void*>(ptr) << " rest=" << rest);

    return rest == 0;
}

void Model::setVisible(bool value)
{
    m_context->visible = value;
}

IVertex::EdgeSizePrecision Model::edgeScaleFactor(const Vector3 &cameraPosition) const
{
    IVertex::EdgeSizePrecision length = 0;
    if (m_context->bones.count() > 1) {
        IBone *bone = m_context->bones.at(1);
        length = (cameraPosition - bone->worldTransform().getOrigin()).length() * m_context->edgeWidth;
    }
    return length / IVertex::EdgeSizePrecision(1000.0);
}

IModel::Type Model::type() const
{
    return kPMXModel;
}

const Array<Vertex *> &Model::vertices() const
{
    return m_context->vertices;
}

const Array<int> &Model::indices() const
{
    return m_context->indices;
}

const Hash<HashString, IString *> &Model::textures() const
{
    return m_context->name2textureRefs;
}

const Array<Material *> &Model::materials() const
{
    return m_context->materials;
}

const Array<Bone *> &Model::bones() const
{
    return m_context->bones;
}

const Array<Morph *> &Model::morphs() const
{
    return m_context->morphs;
}

const Array<Label *> &Model::labels() const
{
    return m_context->labels;
}

const Array<RigidBody *> &Model::rigidBodies() const
{
    return m_context->rigidBodies;
}

const Array<Joint *> &Model::joints() const
{
    return m_context->joints;
}

const Array<SoftBody *> &Model::softBodies() const
{
    return m_context->softBodies;
}

const IString *Model::name(IEncoding::LanguageType type) const
{
    switch (type) {
    case IEncoding::kDefaultLanguage:
    case IEncoding::kJapanese:
        return m_context->namePtr;
    case IEncoding::kEnglish:
        return m_context->englishNamePtr;
    default:
        return 0;
    }
}

const IString *Model::comment(IEncoding::LanguageType type) const
{
    switch (type) {
    case IEncoding::kDefaultLanguage:
    case IEncoding::kJapanese:
        return m_context->commentPtr;
    case IEncoding::kEnglish:
        return m_context->englishCommentPtr;
    default:
        return 0;
    }
}

IModel::ErrorType Model::error() const
{
    return m_context->dataInfo.error;
}

bool Model::isVisible() const
{
    return m_context->visible && !btFuzzyZero(m_context->opacity);
}

bool Model::isPhysicsEnabled() const
{
    return m_context->enablePhysics;
}

Vector3 Model::worldTranslation() const
{
    return m_context->position;
}

Quaternion Model::worldOrientation() const
{
    return m_context->rotation;
}

Scalar Model::opacity() const
{
    return m_context->opacity;
}

Scalar Model::scaleFactor() const
{
    return m_context->scaleFactor;
}

Color Model::edgeColor() const
{
    return kZeroC;
}

IVertex::EdgeSizePrecision Model::edgeWidth() const
{
    return m_context->edgeWidth;
}

Scene *Model::parentSceneRef() const
{
    return m_context->parentSceneRef;
}

IModel *Model::parentModelRef() const
{
    return m_context->parentModelRef;
}

IBone *Model::parentBoneRef() const
{
    return m_context->parentBoneRef;
}

void Model::setName(const IString *value, IEncoding::LanguageType type)
{
    internal::ModelHelper::setName(value, m_context->namePtr, m_context->englishNamePtr, type);
}

void Model::setComment(const IString *value, IEncoding::LanguageType type)
{
    internal::ModelHelper::setName(value, m_context->commentPtr, m_context->englishCommentPtr, type);
}

void Model::setWorldTranslation(const Vector3 &value)
{
    m_context->position = value;
}

void Model::setWorldOrientation(const Quaternion &value)
{
    m_context->rotation = value;
}

void Model::setOpacity(const Scalar &value)
{
    m_context->opacity = value;
}

void Model::setScaleFactor(const Scalar &value)
{
    m_context->scaleFactor = value;
}

void Model::setEdgeColor(const Color &value)
{
    m_context->edgeColor = value;
}

void Model::setEdgeWidth(const IVertex::EdgeSizePrecision &value)
{
    m_context->edgeWidth = value;
}

void Model::setParentSceneRef(Scene *value)
{
    m_context->parentSceneRef = value;
}

void Model::setParentModelRef(IModel *value)
{
    if (m_context->parentModelRef != value && !internal::ModelHelper::hasModelLoopChain(value, this)) {
        m_context->parentModelRef = value;
    }
}

void Model::setParentBoneRef(IBone *value)
{
    if (m_context->parentBoneRef != value && !internal::ModelHelper::hasBoneLoopChain(value, m_context->parentModelRef)) {
        m_context->parentBoneRef = value;
    }
}

void Model::setPhysicsEnable(bool value)
{
    m_context->enablePhysics = value;
}

void Model::updateLocalTransform(Array<Bone *> &bones)
{
    const int nbones = bones.count();
    for (int i = 0; i < nbones; i++) {
        Bone *bone = bones[i];
        bone->performTransform();
        bone->solveInverseKinematics();
    }
    internal::ParallelUpdateLocalTransformProcessor<pmx::Bone> processor(&bones);
    processor.execute();
}

void Model::getIndexBuffer(IndexBuffer *&indexBuffer) const
{
    internal::deleteObject(indexBuffer);
    indexBuffer = new DefaultIndexBuffer(m_context->indices, m_context->vertices.count());
}

void Model::getStaticVertexBuffer(StaticVertexBuffer *&staticBuffer) const
{
    internal::deleteObject(staticBuffer);
    staticBuffer = new DefaultStaticVertexBuffer(this);
}

void Model::getDynamicVertexBuffer(DynamicVertexBuffer *&dynamicBuffer,
                                   const IndexBuffer *indexBuffer) const
{
    internal::deleteObject(dynamicBuffer);
    if (indexBuffer && indexBuffer->ident() == &DefaultIndexBuffer::kIdent) {
        dynamicBuffer = new DefaultDynamicVertexBuffer(this, indexBuffer);
    }
    else {
        dynamicBuffer = 0;
    }
}

void Model::getMatrixBuffer(MatrixBuffer *&matrixBuffer,
                            DynamicVertexBuffer *dynamicBuffer,
                            const IndexBuffer *indexBuffer) const
{
    internal::deleteObject(matrixBuffer);
    if (indexBuffer && indexBuffer->ident() == &DefaultIndexBuffer::kIdent &&
            dynamicBuffer && dynamicBuffer->ident() == &DefaultDynamicVertexBuffer::kIdent) {
        matrixBuffer = new DefaultMatrixBuffer(this,
                                               static_cast<const DefaultIndexBuffer *>(indexBuffer),
                                               static_cast<DefaultDynamicVertexBuffer *>(dynamicBuffer));
    }
    else {
        matrixBuffer = 0;
    }
}

void Model::setAabb(const Vector3 &min, const Vector3 &max)
{
    m_context->aabbMin = min;
    m_context->aabbMax = max;
}

void Model::getAabb(Vector3 &min, Vector3 &max) const
{
    min = m_context->aabbMin;
    max = m_context->aabbMax;
}

float32 Model::version() const
{
    return m_context->dataInfo.version;
}

void Model::setVersion(float32 value)
{
    if ((!btFuzzyZero(value - 2.0f) || !btFuzzyZero(value - 2.1f)) && m_context->dataInfo.version != value) {
        m_context->dataInfo.version = value;
    }
}

IString::Codec Model::encodingType() const
{
    return m_context->codec;
}

void Model::setEncodingType(IString::Codec value)
{
    m_context->codec = m_context->dataInfo.codec = value;
}

int Model::maxUVCount() const
{
    return int(m_context->dataInfo.additionalUVSize);
}

void Model::setMaxUVCount(int value)
{
    if (m_context->dataInfo.additionalUVSize != vsize(value) && internal::checkBound(value, 0, Vertex::kMaxMorphs)) {
        m_context->dataInfo.additionalUVSize = value;
    }
}

IBone *Model::createBone()
{
    return new Bone(this);
}

IJoint *Model::createJoint()
{
    return new Joint(this);
}

ILabel *Model::createLabel()
{
    return new Label(this);
}

IMaterial *Model::createMaterial()
{
    return new Material(this);
}

IMorph *Model::createMorph()
{
    return new Morph(this);
}

IRigidBody *Model::createRigidBody()
{
    return new RigidBody(this, m_context->encodingRef);
}

ISoftBody *Model::createSoftBody()
{
    return new SoftBody(this);
}

IVertex *Model::createVertex()
{
    return new Vertex(this);
}

IBone *Model::findBoneRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Bone, IBone>(m_context->bones, value);
}

IJoint *Model::findJointRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Joint, IJoint>(m_context->joints, value);
}

ILabel *Model::findLabelRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Label, ILabel>(m_context->labels, value);
}

IMaterial *Model::findMaterialRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Material, IMaterial>(m_context->materials, value);
}

IMorph *Model::findMorphRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Morph, IMorph>(m_context->morphs, value);
}

IRigidBody *Model::findRigidBodyRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<RigidBody, IRigidBody>(m_context->rigidBodies, value);
}

ISoftBody *Model::findSoftBodyRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<SoftBody, ISoftBody>(m_context->softBodies, value);
}

IVertex *Model::findVertexRefAt(int value) const
{
    return internal::ModelHelper::findObjectAt<Vertex, IVertex>(m_context->vertices, value);
}

void Model::setIndices(const Array<int> &value)
{
    const int nindices = value.count();
    const int nvertices = m_context->vertices.count();
    m_context->indices.clear();
    for (int i = 0; i < nindices; i++) {
        int index = value[i];
        if (internal::checkBound(index, 0, nvertices)) {
            m_context->indices.append(index);
        }
        else {
            m_context->indices.append(0);
        }
    }
}

void Model::addBone(IBone *value)
{
    internal::ModelHelper::addObject(this, value, m_context->bones);
    if (value) {
        if (const IString *name = value->name(IEncoding::kJapanese)) {
            m_context->name2boneRefs.insert(name->toHashString(), value);
        }
        if (const IString *name = value->name(IEncoding::kEnglish)) {
            m_context->name2boneRefs.insert(name->toHashString(), value);
        }
        Bone::sortBones(m_context->bones, m_context->bonesBeforePhysics, m_context->bonesAfterPhysics);
    }
}

void Model::addJoint(IJoint *value)
{
    internal::ModelHelper::addObject(this, value, m_context->joints);
}

void Model::addLabel(ILabel *value)
{
    internal::ModelHelper::addObject(this, value, m_context->labels);
}

void Model::addMaterial(IMaterial *value)
{
    internal::ModelHelper::addObject(this, value, m_context->materials);
}

void Model::addMorph(IMorph *value)
{
    internal::ModelHelper::addObject(this, value, m_context->morphs);
    if (value) {
        if (const IString *name = value->name(IEncoding::kJapanese)) {
            m_context->name2morphRefs.insert(name->toHashString(), value);
        }
        if (const IString *name = value->name(IEncoding::kEnglish)) {
            m_context->name2morphRefs.insert(name->toHashString(), value);
        }
    }
}

void Model::addRigidBody(IRigidBody *value)
{
    internal::ModelHelper::addObject(this, value, m_context->rigidBodies);
}

void Model::addSoftBody(ISoftBody *value)
{
    internal::ModelHelper::addObject(this, value, m_context->softBodies);
}

void Model::addVertex(IVertex *value)
{
    internal::ModelHelper::addObject(this, value, m_context->vertices);
}

void Model::removeBone(IBone *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->bones);
    internal::ModelHelper::removeBoneReferenceInBones(value, m_context->bones);
    internal::ModelHelper::removeBoneReferenceInRigidBodies(value, m_context->rigidBodies);
    internal::ModelHelper::removeBoneReferenceInVertices(value, m_context->vertices);
    if (value) {
        removeBoneHash(value);
    }
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        if (morph->type() == IMorph::kBoneMorph) {
            const Array<Morph::Bone *> &children = morph->bones();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Bone *child = children[j];
                if (child->bone == value) {
                    child->bone = 0;
                }
            }
        }
    }
}

void Model::removeJoint(IJoint *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->joints);
}

void Model::removeLabel(ILabel *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->labels);
}

void Model::removeMaterial(IMaterial *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->materials);
    internal::ModelHelper::removeMaterialReferenceInVertices(value, m_context->vertices);
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        if (morph->type() == IMorph::kMaterialMorph) {
            const Array<Morph::Material *> &children = morph->materials();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Material *child = children[j];
                const int nmaterials = child->materials->count();
                for (int k = nmaterials - 1; k >= 0; k--) {
                    if (child->materials->at(k) == value) {
                        child->materials->removeAt(k);
                    }
                }
            }
        }
    }
}

void Model::removeMorph(IMorph *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->morphs);
    if (value) {
        removeMorphHash(value);
    }
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        switch (morph->type()) {
        case IMorph::kFlipMorph: {
            const Array<Morph::Flip *> &children = morph->flips();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Flip *child = children[j];
                if (child->morph == value) {
                    child->morph = 0;
                }
            }
            break;
        }
        case IMorph::kGroupMorph: {
            const Array<Morph::Group *> &children = morph->groups();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Group *child = children[j];
                if (child->morph == value) {
                    child->morph = 0;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void Model::removeRigidBody(IRigidBody *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->rigidBodies);
    internal::ModelHelper::removeRigidBodyReferenceInJoints(value, m_context->joints);
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        if (morph->type() == IMorph::kImpulseMorph) {
            const Array<Morph::Impulse *> &children = morph->impulses();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Impulse *child = children[j];
                if (child->rigidBody == value) {
                    child->rigidBody = 0;
                }
            }
        }
    }
}

void Model::removeSoftBody(ISoftBody *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->softBodies);
}

void Model::removeVertex(IVertex *value)
{
    internal::ModelHelper::removeObject(this, value, m_context->vertices);
    const int nmorphs = m_context->morphs.count();
    for (int i = 0; i < nmorphs; i++) {
        Morph *morph = m_context->morphs[i];
        switch (morph->type()) {
        case IMorph::kTexCoordMorph:
        case IMorph::kUVA1Morph:
        case IMorph::kUVA2Morph:
        case IMorph::kUVA3Morph:
        case IMorph::kUVA4Morph:
        {
            const Array<Morph::UV *> &children = morph->uvs();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::UV *child = children[j];
                if (child->vertex == value) {
                    child->vertex = 0;
                }
            }
            break;
        }
        case IMorph::kVertexMorph: {
            const Array<Morph::Vertex *> &children = morph->vertices();
            const int nchildren = children.count();
            for (int j = 0; j < nchildren; j++) {
                Morph::Vertex *child = children[j];
                if (child->vertex == value) {
                    child->vertex = 0;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

IProgressReporter *Model::progressReporterRef() const
{
    return m_context->progressReporterRef;
}

void Model::setProgressReporterRef(IProgressReporter *value)
{
    m_context->progressReporterRef = value;
}

void Model::addBoneHash(Bone *bone)
{
    VPVL2_DCHECK(bone);
    if (const IString *name = bone->name(IEncoding::kJapanese)) {
        m_context->name2boneRefs.insert(name->toHashString(), bone);
    }
    if (const IString *name = bone->name(IEncoding::kEnglish)) {
        m_context->name2boneRefs.insert(name->toHashString(), bone);
    }
}

void Model::removeBoneHash(const IBone *bone)
{
    VPVL2_DCHECK(bone);
    if (const IString *name = bone->name(IEncoding::kJapanese)) {
        m_context->name2boneRefs.remove(name->toHashString());
    }
    if (const IString *name = bone->name(IEncoding::kEnglish)) {
        m_context->name2boneRefs.remove(name->toHashString());
    }
}

void Model::addMorphHash(Morph *morph)
{
    VPVL2_DCHECK(morph);
    if (const IString *name = morph->name(IEncoding::kJapanese)) {
        m_context->name2morphRefs.insert(name->toHashString(), morph);
    }
    if (const IString *name = morph->name(IEncoding::kEnglish)) {
        m_context->name2morphRefs.insert(name->toHashString(), morph);
    }
}

void Model::removeMorphHash(const IMorph *morph)
{
    VPVL2_DCHECK(morph);
    if (const IString *name = morph->name(IEncoding::kJapanese)) {
        m_context->name2morphRefs.remove(name->toHashString());
    }
    if (const IString *name = morph->name(IEncoding::kEnglish)) {
        m_context->name2morphRefs.remove(name->toHashString());
    }
}

int Model::findTextureIndex(const IString *value, int defaultIfNotFound) const
{
    if (value) {
        const int ntextures = m_context->textures.count();
        for (int i = 0; i < ntextures; i++) {
            const IString *texturePath = m_context->textures[i];
            if (value->equals(texturePath)) {
                return i;
            }
        }
    }
    return defaultIfNotFound;
}

IString *Model::addTexture(const IString *value)
{
    IString *texturePath = 0;
    if (value && findTextureIndex(value, -1) == -1) {
        texturePath = m_context->textures.append(value->clone());
        m_context->name2textureRefs.insert(texturePath->toHashString(), texturePath);
    }
    return texturePath;
}

void Model::removeTexture(IString *&value)
{
    int index = findTextureIndex(value, -1);
    if (index >= 0) {
        IString *texturePath = m_context->textures[index];
        m_context->name2textureRefs.remove(value->toHashString());
        m_context->textures.removeAt(index);
        internal::deleteObject(texturePath);
        value = 0;
    }
}

} /* namespace pmx */
} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */
