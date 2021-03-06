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

#ifndef VPVL2_PMD2_MORPH_H_
#define VPVL2_PMD2_MORPH_H_

#include "vpvl2/Common.h"
#include "vpvl2/IMorph.h"
#include "vpvl2/pmd2/Model.h"

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{

class IEncoding;
class IString;

namespace pmd2
{

class Vertex;

class VPVL2_API Morph VPVL2_DECL_FINAL : public IMorph
{
public:
    static const int kNameSize;

    Morph(Model *parentModelRef, IEncoding *encodingRef);
    ~Morph();

    void resetTransform();
    Label *internalParentLabelRef() const;
    IModel *parentModelRef() const;
    const IString *name(IEncoding::LanguageType type) const;
    void setName(const IString *value, IEncoding::LanguageType type);
    int index() const;
    Category category() const;
    Type type() const;
    bool hasParent() const;
    WeightPrecision weight() const;
    void setInternalParentLabelRef(Label *value);
    void setWeight(const WeightPrecision &value);
    void setIndex(int value);
    void markDirty();

    static bool preparse(uint8 *&ptr, vsize &rest, Model::DataInfo &info);
    static bool loadMorphs(const Array<Morph *> &morphs, const Array<pmd2::Vertex *> &vertices);
    static void writeMorphs(const Array<Morph *> &morphs, const Model::DataInfo &info, uint8 *&data);
    static void writeEnglishNames(const Array<Morph *> &morphs, const Model::DataInfo &info, uint8 *&data);
    static vsize estimateTotalSize(const Array<Morph *> &morphs, const Model::DataInfo &info);

    void read(const uint8 *data, vsize &size);
    void readEnglishName(const uint8 *data, int index);
    vsize estimateSize(const Model::DataInfo &info) const;
    void write(uint8 *&data, const Model::DataInfo &info) const;
    void update();

    void addBoneMorph(Bone *value);
    void removeBoneMorph(Bone *value);
    void addGroupMorph(Group *value);
    void removeGroupMorph(Group *value);
    void addMaterialMorph(Material *value);
    void removeMaterialMorph(Material *value);
    void addUVMorph(UV *value);
    void removeUVMorph(UV *value);
    void addVertexMorph(Vertex *value);
    void removeVertexMorph(Vertex *value);
    void addFlipMorph(Flip *value);
    void removeFlipMorph(Flip *value);
    void addImpulseMorph(Impulse *value);
    void removeImpulseMorph(Impulse *value);
    void setType(Type value);
    void setCategory(Category value);

    void getBoneMorphs(Array<Bone *> &morphs) const;
    void getGroupMorphs(Array<Group *> &morphs) const;
    void getMaterialMorphs(Array<Material *> &morphs) const;
    void getUVMorphs(Array<UV *> &morphs) const;
    void getVertexMorphs(Array<Vertex *> &morphs) const;
    void getFlipMorphs(Array<Flip *> &morphs) const;
    void getImpulseMorphs(Array<Impulse *> &morphs) const;

    Array<IMorph::Vertex *> &vertices() const;

private:
    struct PrivateContext;
    PrivateContext *m_context;

    VPVL2_DISABLE_COPY_AND_ASSIGN(Morph)
};

} /* namespace pmd2 */
} /* namespace VPVL2_VERSION_NS */
} /* namespace vpvl2 */

#endif
