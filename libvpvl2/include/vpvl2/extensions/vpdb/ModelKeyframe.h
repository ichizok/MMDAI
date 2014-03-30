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

#include <vpvl2/IModelKeyframe.h>

namespace vpvl2
{
namespace VPVL2_VERSION_NS
{
namespace extensions
{
namespace vpdb
{

class Motion;

class VPVL2_API ModelKeyframe VPVL2_DECL_FINAL : public IModelKeyframe {
public:
    ModelKeyframe(Motion *parent);
    ~ModelKeyframe();

    void read(const uint8 *data);
    void write(uint8 *data) const;
    vsize estimateSize() const;
    const IString *name() const;
    TimeIndex timeIndex() const;
    LayerIndex layerIndex() const;
    void setName(const IString *value);
    void setTimeIndex(const TimeIndex &value);
    void setLayerIndex(const LayerIndex &value);
    Type type() const;

    IModelKeyframe *clone() const;
    bool isVisible() const;
    bool isShadowEnabled() const;
    bool isAddBlendEnabled() const;
    bool isPhysicsEnabled() const;
    bool isInverseKinematicsEnabled(const IBone *value) const;
    uint8 physicsStillMode() const;
    IVertex::EdgeSizePrecision edgeWidth() const;
    Color edgeColor() const;
    void setVisible(bool value);
    void setShadowEnable(bool value);
    void setAddBlendEnable(bool value);
    void setPhysicsEnable(bool value);
    void setPhysicsStillMode(uint8 value);
    void setEdgeWidth(const IVertex::EdgeSizePrecision &value);
    void setEdgeColor(const Color &value);
    void setInverseKinematicsEnable(IBone *bone, bool value);

private:
    struct PrivateContext;
    PrivateContext *m_context;
};

} /* namespace vpdb */
} /* namespace extensions */
} /* namespace VPVL2_VERSION_NS */
using namespace VPVL2_VERSION_NS;

} /* namespace vpvl2 */