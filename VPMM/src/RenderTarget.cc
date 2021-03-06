/**

 Copyright (c) 2010-2013  hkrn

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

#include <vpvl2/vpvl2.h>
#include <vpvl2/gl/Texture2D.h>
#include <vpvl2/extensions/BaseApplicationContext.h>
#include <vpvl2/extensions/XMLProject.h>
#include <vpvl2/extensions/qt/String.h>

#include "Common.h"
#include <QtCore>
#include <QtMultimedia>
#include <QQuickWindow>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include <LinearMath/btIDebugDraw.h>
#include <IGizmo.h>

/* GLM_FORCE_RADIANS is defined at vpvl2/extensions/BaseApplicationContext.h */
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ApplicationContext.h"
#include "BoneRefObject.h"
#include "CameraRefObject.h"
#include "GraphicsDevice.h"
#include "Grid.h"
#include "LightRefObject.h"
#include "MotionProxy.h"
#include "RenderTarget.h"
#include "ModelProxy.h"
#include "ProjectProxy.h"
#include "SkeletonDrawer.h"
#include "Util.h"
#include "WorldProxy.h"

using namespace vpvl2;
using namespace vpvl2::extensions;
using namespace vpvl2::extensions::qt;

namespace {

static IApplicationContext::MousePositionType convertMousePositionType(int button)
{
    switch (static_cast<Qt::MouseButton>(button)) {
    case Qt::LeftButton:
        return IApplicationContext::kMouseLeftPressPosition;
    case Qt::MiddleButton:
        return IApplicationContext::kMouseMiddlePressPosition;
    case Qt::RightButton:
        return IApplicationContext::kMouseRightPressPosition;
    default:
        return IApplicationContext::kMouseCursorPosition;
    }
}

static int convertKey(int key)
{
    switch (static_cast<Qt::Key>(key)) {
    case Qt::Key_Backspace:
        return '\b';
    case Qt::Key_Tab:
        return '\t';
    case Qt::Key_Clear:
        return 0x0c;
    case Qt::Key_Return:
        return '\r';
    case Qt::Key_Pause:
        return 0x13;
    case Qt::Key_Escape:
        return 0x1b;
    case Qt::Key_Space:
        return ' ';
    case Qt::Key_Delete:
        return 0x7f;
    case Qt::Key_Up:
        return 273;
    case Qt::Key_Down:
        return 274;
    case Qt::Key_Right:
        return 275;
    case Qt::Key_Left:
        return 276;
    case Qt::Key_Insert:
        return 277;
    case Qt::Key_Home:
        return 278;
    case Qt::Key_End:
        return 279;
    case Qt::Key_PageUp:
        return 280;
    case Qt::Key_PageDown:
        return 281;
    case Qt::Key_F1:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_F6:
    case Qt::Key_F7:
    case Qt::Key_F8:
    case Qt::Key_F9:
    case Qt::Key_F10:
    case Qt::Key_F11:
    case Qt::Key_F12:
    case Qt::Key_F13:
    case Qt::Key_F14:
    case Qt::Key_F15:
        return 282 + (key - Qt::Key_F1);
    default:
        return key;
    }
}

static int convertModifier(int value)
{
    switch (static_cast<Qt::Modifier>(value)) {
    case Qt::ShiftModifier:
        return 0x3;
    case Qt::ControlModifier:
        return 0xc0;
    case Qt::AltModifier:
        return 0x100;
    case Qt::MetaModifier:
        return 0xc00;
    default:
        return 0;
    }
}

}

class RenderTarget::DebugDrawer : public btIDebugDraw {
public:
    DebugDrawer()
        : m_flags(0),
          m_index(0)
    {
    }
    ~DebugDrawer() {
    }

    void initialize() {
        if (!m_program) {
            m_program.reset(new QOpenGLShaderProgram());
            m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":shaders/gui/grid.vsh");
            m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":shaders/gui/grid.fsh");
            m_program->bindAttributeLocation("inPosition", 0);
            m_program->bindAttributeLocation("inColor", 1);
            m_program->link();
            Q_ASSERT(m_program->isLinked());
            allocateBuffer(QOpenGLBuffer::VertexBuffer, m_vbo);
            allocateBuffer(QOpenGLBuffer::VertexBuffer, m_cbo);
            m_vao.reset(new QOpenGLVertexArrayObject());
            if (m_vao->create()) {
                m_vao->bind();
                bindAttributeBuffers();
                m_vao->release();
            }
        }
    }

    void drawContactPoint(const btVector3 &PointOnB,
                          const btVector3 &normalOnB,
                          btScalar distance,
                          int /* lifeTime */,
                          const btVector3 &color) {
        drawLine(PointOnB, PointOnB + normalOnB * distance, color);
    }
    void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) {
        int vertexIndex = m_index, colorIndex = m_index;
        m_vertices[vertexIndex++] = from;
        m_vertices[vertexIndex++] = to;
        m_colors[colorIndex++] = color;
        m_colors[colorIndex++] = color;
        m_index += 2;
        if (m_index >= kPreAllocatedSize) {
            flush();
        }
    }
    void drawLine(const btVector3 &from,
                  const btVector3 &to,
                  const btVector3 &fromColor,
                  const btVector3 & /* toColor */) {
        drawLine(from, to, fromColor);
    }
    void draw3dText(const btVector3 & /* location */, const char *textString) {
        VPVL2_VLOG(1, textString);
    }
    void reportErrorWarning(const char *warningString) {
        VPVL2_LOG(WARNING, warningString);
    }
    int getDebugMode() const {
        return m_flags;
    }
    void setDebugMode(int debugMode) {
        m_flags = debugMode;
    }

    void flush() {
        Q_ASSERT(m_index % 2 == 0);
        m_vbo->bind();
        m_vbo->write(0, m_vertices, m_index * sizeof(m_vertices[0]));
        m_cbo->bind();
        m_cbo->write(0, m_colors, m_index * sizeof(m_colors[0]));
        bindProgram();
        m_program->setUniformValue("modelViewProjectionMatrix", m_viewProjectionMatrix);
        glDrawArrays(GL_LINES, 0, m_index / 2);
        releaseProgram();
        m_index = 0;
    }
    void setViewProjectionMatrix(const QMatrix4x4 &value) {
        m_viewProjectionMatrix = value;
    }

private:
    enum {
        kPreAllocatedSize = 1024000
    };
    static void allocateBuffer(QOpenGLBuffer::Type type, QScopedPointer<QOpenGLBuffer> &buffer) {
        buffer.reset(new QOpenGLBuffer(type));
        buffer->create();
        buffer->bind();
        buffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        buffer->allocate(sizeof(QVector3D) * kPreAllocatedSize);
        buffer->release();
    }
    void bindAttributeBuffers() {
        m_program->enableAttributeArray(0);
        m_program->enableAttributeArray(1);
        m_vbo->bind();
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(m_vertices[0]));
        m_cbo->bind();
        m_program->setAttributeBuffer(1, GL_FLOAT, 0, 3, sizeof(m_colors[0]));
    }
    void bindProgram() {
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        m_program->bind();
        if (m_vao->isCreated()) {
            m_vao->bind();
        }
        else {
            bindAttributeBuffers();
        }
    }
    void releaseProgram() {
        if (m_vao->isCreated()) {
            m_vao->release();
        }
        else {
            m_vbo->release();
            m_cbo->release();
            m_program->disableAttributeArray(0);
            m_program->disableAttributeArray(1);
        }
        m_program->release();
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    QScopedPointer<QOpenGLShaderProgram> m_program;
    QScopedPointer<QOpenGLVertexArrayObject> m_vao;
    QScopedPointer<QOpenGLBuffer> m_vbo;
    QScopedPointer<QOpenGLBuffer> m_cbo;
    QMatrix4x4 m_viewProjectionMatrix;
    Vector3 m_vertices[kPreAllocatedSize];
    Vector3 m_colors[kPreAllocatedSize];
    int m_flags;
    int m_index;
};

const QVector3D RenderTarget::kDefaultShadowMapSize = QVector3D(1024, 1024, 1);

RenderTarget::RenderTarget(QQuickItem *parent)
    : QQuickItem(parent),
      m_grid(new Grid()),
      m_shadowMapSize(kDefaultShadowMapSize),
      m_editMode(SelectMode),
      m_projectProxyRef(0),
      m_currentGizmoRef(0),
      m_lastTimeIndex(0),
      m_currentTimeIndex(0),
      m_snapStepSize(5, 5, 5),
      m_visibleGizmoMasks(AxisX | AxisY | AxisZ | AxisScreen),
      m_grabbingGizmo(false),
      m_playing(false),
      m_dirty(false)
{
    connect(this, &RenderTarget::windowChanged, this, &RenderTarget::handleWindowChange);
    connect(this, &RenderTarget::shadowMapSizeChanged, this, &RenderTarget::prepareUpdatingLight);
}

RenderTarget::~RenderTarget()
{
    m_projectProxyRef = 0;
    m_currentGizmoRef = 0;
    m_lastTimeIndex = 0;
    m_currentTimeIndex = 0;
    m_grabbingGizmo = false;
    m_playing = false;
    m_dirty = false;
}

bool RenderTarget::handleMousePress(int x, int y, int button)
{
    glm::vec4 v(x - m_viewport.x(), y - m_viewport.y(), 1, 0);
    IApplicationContext::MousePositionType type = convertMousePositionType(button);
    m_applicationContext->setMousePosition(v, type);
    if (!m_applicationContext->handleMouse(v, type) && m_currentGizmoRef) {
        m_grabbingGizmo = m_currentGizmoRef->OnMouseDown(x, y);
        if (m_grabbingGizmo) {
            ModelProxy *modelProxy = m_projectProxyRef->currentModel();
            Q_ASSERT(modelProxy);
            switch (m_editMode) {
            case RotateMode:
            case MoveMode:
                modelProxy->beginTransform(0);
                break;
            case SelectMode:
            default:
                break;
            }
            emit grabbingGizmoChanged();
        }
    }
    return m_grabbingGizmo;
}

bool RenderTarget::handleMouseMove(int x, int y,  bool pressed)
{
    glm::vec4 v(x - m_viewport.x(), y - m_viewport.y(), pressed, 0);
    m_applicationContext->setMousePosition(v, IApplicationContext::kMouseCursorPosition);
    if (m_applicationContext->handleMouse(v, IApplicationContext::kMouseCursorPosition)) {
        return true;
    }
    else if (m_currentGizmoRef) {
        m_currentGizmoRef->OnMouseMove(x, y);
        if (m_grabbingGizmo) {
            const ModelProxy *modelProxy = m_projectProxyRef->currentModel();
            Q_ASSERT(modelProxy);
            const BoneRefObject *boneProxy = modelProxy->firstTargetBone();
            Q_ASSERT(boneProxy);
            IBone *boneRef = boneProxy->data();
            Scalar rawMatrix[16];
            for (int i = 0; i < 16; i++) {
                rawMatrix[i] = static_cast<Scalar>(m_editMatrix.constData()[i]);
            }
            Transform transform(Transform::getIdentity());
            transform.setFromOpenGLMatrix(rawMatrix);
            boneRef->setLocalTranslation(transform.getOrigin());
            boneRef->setLocalOrientation(transform.getRotation());
            return true;
        }
    }
    return false;
}

bool RenderTarget::handleMouseRelease(int x, int y, int button)
{
    glm::vec4 v(x - m_viewport.x(), y - m_viewport.y(), 0, 0);
    IApplicationContext::MousePositionType type = convertMousePositionType(button);
    m_applicationContext->setMousePosition(v, type);
    if (m_applicationContext->handleMouse(v, type)) {
        return true;
    }
    else if (m_currentGizmoRef) {
        m_currentGizmoRef->OnMouseUp(x, y);
        if (m_grabbingGizmo) {
            ModelProxy *modelProxy = m_projectProxyRef->currentModel();
            Q_ASSERT(modelProxy);
            switch (m_editMode) {
            case RotateMode:
            case MoveMode:
                modelProxy->commitTransform();
                break;
            case SelectMode:
            default:
                break;
            }
            m_grabbingGizmo = false;
            emit grabbingGizmoChanged();
            return true;
        }
    }
    return false;
}

bool RenderTarget::handleMouseWheel(int x, int y)
{
    glm::vec4 v(x, y, 0, 0);
    return m_applicationContext->handleMouse(v, IApplicationContext::kMouseWheelPosition);
}

bool RenderTarget::handleKeyPress(int key, int modifier)
{
    return m_applicationContext->handleKeyPress(convertKey(key), convertModifier(modifier));
}

void RenderTarget::toggleRunning(bool value)
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    if (value) {
        connect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::draw, Qt::DirectConnection);
    }
    else {
        disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::draw);
    }
}

bool RenderTarget::isInitialized() const
{
    return Scene::isInitialized();
}

qreal RenderTarget::currentTimeIndex() const
{
    return m_currentTimeIndex;
}

void RenderTarget::setCurrentTimeIndex(qreal value)
{
    if (value != m_currentTimeIndex) {
        m_currentTimeIndex = value;
        emit currentTimeIndexChanged();
        if (QQuickWindow *win = window()) {
            win->update();
        }
    }
}

qreal RenderTarget::lastTimeIndex() const
{
    return m_lastTimeIndex;
}

void RenderTarget::setLastTimeIndex(qreal value)
{
    if (value != m_lastTimeIndex) {
        m_lastTimeIndex = value;
        emit lastTimeIndexChanged();
        if (QQuickWindow *win = window()) {
            win->update();
        }
    }
}

qreal RenderTarget::currentFPS() const
{
    return m_counter.value();
}

ProjectProxy *RenderTarget::projectProxy() const
{
    return m_projectProxyRef;
}

Grid *RenderTarget::grid() const
{
    return m_grid.data();
}

void RenderTarget::setProjectProxy(ProjectProxy *value)
{
    Q_ASSERT(value);
    connect(this, &RenderTarget::enqueuedModelsDidDelete, value, &ProjectProxy::enqueuedModelsDidDelete);
    connect(value, &ProjectProxy::gridVisibleChanged, this, &RenderTarget::toggleGridVisible);
    connect(value, &ProjectProxy::modelDidAdd, this, &RenderTarget::enqueueUploadingModel);
    connect(value, &ProjectProxy::modelDidCommitUploading, this, &RenderTarget::commitUploadingModels);
    connect(value, &ProjectProxy::modelDidRemove, this, &RenderTarget::enqueueDeletingModel);
    connect(value, &ProjectProxy::modelDidCommitDeleting, this, &RenderTarget::commitDeletingModels);
    connect(value, &ProjectProxy::effectDidAdd, this, &RenderTarget::enqueueUploadingEffect);
    connect(value, &ProjectProxy::effectDidCommitUploading, this, &RenderTarget::commitUploadingEffects);
    connect(value, &ProjectProxy::motionDidInitialize, this, &RenderTarget::prepareSyncMotionState);
    connect(value, &ProjectProxy::motionDidLoad, this, &RenderTarget::prepareSyncMotionState);
    connect(value, &ProjectProxy::currentModelChanged, this, &RenderTarget::updateGizmo);
    connect(value, &ProjectProxy::projectDidRelease, this, &RenderTarget::commitDeletingModels);
    connect(value, &ProjectProxy::projectWillCreate, this, &RenderTarget::disconnectProjectSignals);
    connect(value, &ProjectProxy::projectDidCreate, this, &RenderTarget::prepareUploadingModelsInProject);
    connect(value, &ProjectProxy::projectWillLoad, this, &RenderTarget::disconnectProjectSignals);
    connect(value, &ProjectProxy::projectDidLoad, this, &RenderTarget::prepareUploadingModelsInProject);
    connect(value, &ProjectProxy::modelBoneDidReset, this, &RenderTarget::updateGizmo);
    connect(value, &ProjectProxy::undoDidPerform, this, &RenderTarget::updateGizmoAndRender);
    connect(value, &ProjectProxy::redoDidPerform, this, &RenderTarget::updateGizmoAndRender);
    connect(value, &ProjectProxy::rewindDidPerform, this, &RenderTarget::rewind);
    connect(value->world(), &WorldProxy::simulationTypeChanged, this, &RenderTarget::prepareSyncMotionState);
    CameraRefObject *camera = value->camera();
    connect(camera, &CameraRefObject::lookAtChanged, this, &RenderTarget::markDirty);
    connect(camera, &CameraRefObject::angleChanged, this, &RenderTarget::markDirty);
    connect(camera, &CameraRefObject::distanceChanged, this, &RenderTarget::markDirty);
    connect(camera, &CameraRefObject::fovChanged, this, &RenderTarget::markDirty);
    connect(camera, &CameraRefObject::cameraDidReset, this, &RenderTarget::markDirty);
    LightRefObject *light = value->light();
    connect(light, &LightRefObject::directionChanged, this, &RenderTarget::prepareUpdatingLight);
    connect(light, &LightRefObject::shadowTypeChanged, this, &RenderTarget::prepareUpdatingLight);
    m_grid->setVisible(value->isGridVisible());
    m_projectProxyRef = value;
}

bool RenderTarget::isPlaying() const
{
    return m_playing;
}

void RenderTarget::setPlaying(bool value)
{
    Q_ASSERT(m_projectProxyRef);
    if (m_playing != value) {
        m_projectProxyRef->world()->setPlaying(value);
        m_playing = value;
        emit playingChanged();
    }
}

void RenderTarget::setTransforming(bool value)
{
    Q_ASSERT(m_projectProxyRef);
    if (m_playing != value) {
        m_playing = value;
        emit playingChanged();
    }
}

bool RenderTarget::isDirty() const
{
    return m_dirty;
}

void RenderTarget::setDirty(bool value)
{
    if (m_dirty != value) {
        m_dirty = value;
        emit dirtyChanged();
    }
}

bool RenderTarget::isSnapGizmoEnabled() const
{
    return translationGizmo()->IsUsingSnap();
}

void RenderTarget::setSnapGizmoEnabled(bool value)
{
    IGizmo *translationGizmoRef = translationGizmo();
    if (translationGizmoRef->IsUsingSnap() != value) {
        translationGizmoRef->UseSnap(value);
        emit enableSnapGizmoChanged();
    }
}

bool RenderTarget::grabbingGizmo() const
{
    return m_grabbingGizmo;
}

QRect RenderTarget::viewport() const
{
    return m_viewport;
}

void RenderTarget::setViewport(const QRect &value)
{
    if (m_viewport != value) {
        m_viewport = value;
        setDirty(true);
        emit viewportChanged();
    }
}

QVector3D RenderTarget::shadowMapSize() const
{
    return m_shadowMapSize;
}

void RenderTarget::setShadowMapSize(const QVector3D &value)
{
    Q_ASSERT(m_projectProxyRef);
    if (value != m_shadowMapSize) {
        m_projectProxyRef->setGlobalString("shadow.texture.size", value);
        m_shadowMapSize = value;
        emit shadowMapSizeChanged();
    }
}

QUrl RenderTarget::audioUrl() const
{
    return QUrl();
}

void RenderTarget::setAudioUrl(const QUrl &value)
{
    if (value != audioUrl()) {
        QScopedPointer<QAudioDecoder> decoder(new QAudioDecoder());
        decoder->setSourceFilename(value.toLocalFile());
        /* XXX: under construction */
        while (decoder->bufferAvailable()) {
            const QAudioBuffer &buffer = decoder->read();
            qDebug() << buffer.startTime() << buffer.frameCount() << buffer.sampleCount();
        }
        emit audioUrlChanged();
    }
}

RenderTarget::EditModeType RenderTarget::editMode() const
{
    return m_editMode;
}

void RenderTarget::setEditMode(EditModeType value)
{
    if (m_editMode != value) {
        switch (value) {
        case RotateMode:
            m_currentGizmoRef = orientationGizmo();
            break;
        case MoveMode:
            m_currentGizmoRef = translationGizmo();
            break;
        case SelectMode:
        default:
            m_currentGizmoRef = 0;
            break;
        }
        m_editMode = value;
        emit editModeChanged();
    }
}

RenderTarget::VisibleGizmoMasks RenderTarget::visibleGizmoMasks() const
{
    return m_visibleGizmoMasks;
}

void RenderTarget::setVisibleGizmoMasks(VisibleGizmoMasks value)
{
    if (value != m_visibleGizmoMasks) {
        orientationGizmo()->SetAxisMask(value);
        m_visibleGizmoMasks = value;
        emit visibleGizmoMasksChanged();
    }
}

QVector3D RenderTarget::snapGizmoStepSize() const
{
    return m_snapStepSize;
}

void RenderTarget::setSnapGizmoStepSize(const QVector3D &value)
{
    if (!qFuzzyCompare(value, m_snapStepSize)) {
        translationGizmo()->SetSnap(value.x(), value.y(), value.z());
        m_snapStepSize = value;
        emit snapGizmoStepSizeChanged();
    }
}

QMatrix4x4 RenderTarget::viewMatrix() const
{
    return QMatrix4x4(glm::value_ptr(m_viewMatrix));
}

QMatrix4x4 RenderTarget::projectionMatrix() const
{
    return QMatrix4x4(glm::value_ptr(m_projectionMatrix));
}

GraphicsDevice *RenderTarget::graphicsDevice() const
{
    return m_graphicsDevice.data();
}

void RenderTarget::handleWindowChange(QQuickWindow *window)
{
    if (window) {
        Q_ASSERT(window->thread() == thread());
        connect(window, &QQuickWindow::sceneGraphInitialized, this, &RenderTarget::initializeOpenGLContext, Qt::DirectConnection);
        connect(window, &QQuickWindow::frameSwapped, this, &RenderTarget::synchronizeImplicitly, Qt::DirectConnection);
        window->setClearBeforeRendering(false);
    }
}

void RenderTarget::update()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    window()->update();
}

void RenderTarget::render()
{
    Q_ASSERT(window());
    connect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::synchronizeExplicitly, Qt::DirectConnection);
}

void RenderTarget::exportImage(const QUrl &fileUrl, const QSize &size, bool checkFileUrl)
{
    Q_ASSERT(window());
    if (checkFileUrl && (fileUrl.isEmpty() || !fileUrl.isValid())) {
        /* do nothing if url is empty or invalid */
        VPVL2_VLOG(2, "fileUrl is empty or invalid: url=" << fileUrl.toString().toStdString());
        return;
    }
    m_exportLocation = fileUrl;
    m_exportSize = size;
    if (!m_exportSize.isValid()) {
        m_exportSize = m_viewport.size();
    }
    connect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::drawOffscreenForImage, Qt::DirectConnection);
}

void RenderTarget::loadJson(const QUrl &fileUrl)
{
    QFile file(fileUrl.toLocalFile());
    if (file.open(QFile::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument &document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            const QJsonObject &root = document.object(), &projectObject = root.value("project").toObject();
            QFileInfo fileInfo(projectObject.value("path").toString());
            if (fileInfo.exists() && fileInfo.isFile()) {
                m_projectProxyRef->loadAsync(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
            }
            else {
                m_projectProxyRef->world()->setSimulationType(projectObject.value("simulationPhysics").toBool(false) ? WorldProxy::EnableSimulationPlayOnly : WorldProxy::DisableSimulation);
                m_projectProxyRef->setTitle(projectObject.value("title").toString(tr("Untitled Project")));
                m_projectProxyRef->setScreenColor(QColor(projectObject.value("screenColor").toString("#ffffff")));
                if (projectObject.contains("accelerationType")) {
                    QHash<QString, ProjectProxy::AccelerationType> string2AccelerationType;
                    string2AccelerationType.insert("opencl", ProjectProxy::OpenCLGPUAcceleration);
                    string2AccelerationType.insert("opencl_gpu", ProjectProxy::OpenCLGPUAcceleration);
                    string2AccelerationType.insert("opencl_cpu", ProjectProxy::OpenCLCPUAcceleration);
                    string2AccelerationType.insert("vss", ProjectProxy::VertexShaderAcceleration);
                    string2AccelerationType.insert("parallel", ProjectProxy::ParallelAcceleration);
                    const QString &accelerationType = projectObject.value("accelerationType").toString();
                    if (string2AccelerationType.contains(accelerationType)) {
                        m_projectProxyRef->setAccelerationType(string2AccelerationType.value(accelerationType));
                    }
                    else {
                        m_projectProxyRef->setAccelerationType(ProjectProxy::NoAcceleration);
                    }
                }
                const QJsonObject &gridObject = root.value("grid").toObject();
                m_projectProxyRef->setGridVisible(gridObject.value("visible").toBool(true));
                const QJsonObject &audioObject = root.value("audio").toObject();
                fileInfo.setFile(audioObject.value("source").toString());
                if (fileInfo.exists() && fileInfo.isFile()) {
                    m_projectProxyRef->setAudioSource(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
                    m_projectProxyRef->setAudioVolume(audioObject.value("volume").toDouble());
                }
                CameraRefObject *camera = m_projectProxyRef->camera();
                const QJsonObject &cameraObject = root.value("camera").toObject();
                if (cameraObject.contains("motion")) {
                    fileInfo.setFile(cameraObject.value("motion").toString());
                    if (fileInfo.exists() && fileInfo.isFile()) {
                        m_projectProxyRef->loadMotion(QUrl::fromLocalFile(fileInfo.absoluteFilePath()), 0, ProjectProxy::CameraMotion);
                    }
                }
                else {
                    camera->setFov(cameraObject.value("fov").toDouble(camera->fov()));
                    camera->setDistance(cameraObject.value("distance").toDouble(camera->distance()));
                }
                const QJsonObject &lightObject = root.value("light").toObject();
                LightRefObject *light = m_projectProxyRef->light();
                light->setColor(QColor(lightObject.value("color").toString("#999999")));
                foreach (const QJsonValue &v, root.value("models").toArray()) {
                    const QJsonObject &item = v.toObject();
                    fileInfo.setFile(item.value("path").toString());
                    if (fileInfo.exists() && fileInfo.isFile()) {
                        ModelProxy *modelProxy = m_projectProxyRef->internalLoadModel(QUrl::fromLocalFile(fileInfo.absoluteFilePath()), QUuid::createUuid());
                        modelProxy->setScaleFactor(item.value("scaleFactor").toDouble(1.0));
                        modelProxy->setOpacity(item.value("opacity").toDouble(1.0));
                        modelProxy->setEdgeWidth(item.value("edgeWidth").toDouble(1.0));
                        foreach (const QJsonValue &m, item.value("motions").toArray()) {
                            fileInfo.setFile(m.toString());
                            if (fileInfo.exists() && fileInfo.isFile()) {
                                m_projectProxyRef->loadMotion(QUrl::fromLocalFile(fileInfo.absoluteFilePath()), modelProxy, ProjectProxy::ModelMotion);
                            }
                        }
                    }
                }
                foreach (const QJsonValue &v, root.value("effects").toArray()) {
                    const QJsonObject &item = v.toObject();
                    fileInfo.setFile(item.value("path").toString());
                    if (fileInfo.exists() && fileInfo.isFile()) {
                        m_projectProxyRef->loadEffect(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
                    }
                }
            }
        }
        else {
            VPVL2_LOG(WARNING, "Cannot parse JSON file: " << parseError.errorString().toStdString() << " at offset " << parseError.offset);
        }
    }
    else {
        VPVL2_LOG(WARNING, "Cannot open JSON file: " << file.errorString().toStdString());
    }
}

void RenderTarget::markDirty()
{
    m_dirty = true;
}

void RenderTarget::updateGizmo()
{
    Q_ASSERT(m_projectProxyRef);
    if (const ModelProxy *modelProxy = m_projectProxyRef->currentModel()) {
        IGizmo *translationGizmoRef = translationGizmo(), *orientationGizmoRef = orientationGizmo();
        switch (modelProxy->transformType()) {
        case ModelProxy::GlobalTransform:
            translationGizmoRef->SetLocation(IGizmo::LOCATE_WORLD);
            orientationGizmoRef->SetLocation(IGizmo::LOCATE_WORLD);
            break;
        case ModelProxy::LocalTransform:
            translationGizmoRef->SetLocation(IGizmo::LOCATE_LOCAL);
            orientationGizmoRef->SetLocation(IGizmo::LOCATE_LOCAL);
            break;
        case ModelProxy::ViewTransform:
            translationGizmoRef->SetLocation(IGizmo::LOCATE_VIEW);
            orientationGizmoRef->SetLocation(IGizmo::LOCATE_VIEW);
            break;
        }
        setSnapGizmoStepSize(m_snapStepSize);
        if (const BoneRefObject *boneProxy = modelProxy->firstTargetBone()) {
            const IBone *boneRef = boneProxy->data();
            Transform transform;
            float32 m[16];
            m_applicationContext->getMatrix(m, modelProxy->data(), IApplicationContext::kCameraMatrix | IApplicationContext::kWorldMatrix);
            transform.setFromOpenGLMatrix(m);
            const Vector3 &v = transform.getOrigin();
            translationGizmoRef->SetOffset(v.x(), v.y(), v.z());
            orientationGizmoRef->SetOffset(v.x(), v.y(), v.z());
            if (!boneRef->isMovable() && editMode() == MoveMode) {
                setEditMode(RotateMode);
            }
            if (!boneRef->isRotateable() && editMode() == RotateMode) {
                setEditMode(SelectMode);
            }
            transform.setIdentity();
            transform.setOrigin(boneRef->localTranslation());
            transform.setRotation(boneRef->localOrientation());
            transform.getOpenGLMatrix(m);
            m_editMatrix = Util::fromMatrix4(glm::make_mat4(m));
        }
    }
    else {
        m_currentGizmoRef = 0;
    }
}

void RenderTarget::updateGizmoAndRender()
{
    updateGizmo();
    render();
}

void RenderTarget::handleAudioDecoderError(QAudioDecoder::Error error)
{
    const QString &message = ""; //mediaPlayer()->errorString();
    VPVL2_LOG(WARNING, "The audio " << audioUrl().toString().toStdString() << " cannot be loaded: code=" << error << " message=" << message.toStdString());
    emit errorDidHappen(QStringLiteral("%1 (code=%2)").arg(message).arg(error));
}

void RenderTarget::handleFileChange(const QString &filePath)
{
    Q_ASSERT(window());
    QMutexLocker locker(&m_fileChangeQueueMutex); Q_UNUSED(locker);
    connect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::consumeFileChangeQueue, Qt::DirectConnection);
    m_fileChangeQueue.enqueue(filePath);
}

void RenderTarget::handleTextureChange(const QUrl &newTexturePath, const QUrl &oldTexturePath)
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(qobject_cast<ModelProxy *>(sender()));
    m_applicationContext->renameTexturePath(newTexturePath, oldTexturePath, qobject_cast<ModelProxy *>(sender()));
    handleFileChange(newTexturePath.toLocalFile());
}

void RenderTarget::consumeFileChangeQueue()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    QMutexLocker locker(&m_fileChangeQueueMutex); Q_UNUSED(locker);
    disconnect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::consumeFileChangeQueue);
    while (!m_fileChangeQueue.isEmpty()) {
        m_applicationContext->reloadFile(m_fileChangeQueue.dequeue());
    }
}

void RenderTarget::toggleGridVisible()
{
    m_grid->setVisible(m_projectProxyRef->isGridVisible());
}

void RenderTarget::draw()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    if (m_projectProxyRef) {
        emit renderWillPerform();
        resetOpenGLStates();
        drawShadowMap();
        updateViewport();
        clearScene();
        m_applicationContext->saveDirtyEffects();
        drawGrid();
        drawScene();
        drawDebug();
        drawModelBones();
        drawCurrentGizmo();
        drawEffectParameterUIWidgets();
        bool flushed = false;
        m_counter.update(m_renderTimer.elapsed(), flushed);
        if (flushed) {
            emit currentFPSChanged();
        }
        emit renderDidPerform();
    }
}

void RenderTarget::drawOffscreenForImage()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::drawOffscreenForImage);
    if (m_exportLocation.isValid()) {
        connect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::writeExportedImage);
    }
    int samples = m_projectProxyRef->globalSetting("msaa.samples", QVariant(window()->format().samples())).toInt();
    QOpenGLFramebufferObject fbo(m_exportSize, ApplicationContext::framebufferObjectFormat(samples));
    drawOffscreen(&fbo);
    m_exportImage = fbo.toImage();
    emit offscreenImageDidExport();
}

void RenderTarget::writeExportedImage()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::frameSwapped, this, &RenderTarget::writeExportedImage);
    QFileInfo finfo(m_exportLocation.toLocalFile());
    const QString &suffix = finfo.suffix();
    if (suffix != "bmp" && !QQuickWindow::hasDefaultAlphaBuffer()) {
        QTemporaryFile tempFile;
        if (tempFile.open()) {
            m_exportImage.save(&tempFile, "bmp");
            QSaveFile saveFile(finfo.filePath());
            if (saveFile.open(QFile::WriteOnly)) {
                const QImage image(tempFile.fileName());
                image.save(&saveFile, qPrintable(suffix));
                if (!saveFile.commit()) {
                    VPVL2_LOG(WARNING, "Cannot commit the file to: path=" << saveFile.fileName().toStdString() << " reason=" << saveFile.errorString().toStdString());
                }
            }
            else {
                VPVL2_LOG(WARNING, "Cannot open the file to commit: path=" << saveFile.fileName().toStdString() << "  reason=" << saveFile.errorString().toStdString());
            }
        }
        else {
            VPVL2_LOG(WARNING, "Cannot open temporary file: path=" << tempFile.fileName().toStdString() << "  reason=" << tempFile.errorString().toStdString());
        }
    }
    else {
        QSaveFile saveFile(finfo.filePath());
        if (saveFile.open(QFile::WriteOnly)) {
            m_exportImage.save(&saveFile, qPrintable(suffix));
            saveFile.commit();
        }
        else {
            VPVL2_LOG(WARNING, "Cannot open file to commit: path=" << saveFile.fileName().toStdString() << " " << saveFile.errorString().toStdString());
        }
    }
    m_exportImage = QImage();
    m_exportSize = QSize();
}

void RenderTarget::prepareSyncMotionState()
{
    Q_ASSERT(window());
    connect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::synchronizeMotionState, Qt::DirectConnection);
}

void RenderTarget::prepareUpdatingLight()
{
    if (QQuickWindow *win = window()) {
        connect(win, &QQuickWindow::beforeRendering, this, &RenderTarget::performUpdatingLight, Qt::DirectConnection);
    }
}

void RenderTarget::synchronizeExplicitly()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::synchronizeExplicitly);
    if (m_projectProxyRef) {
        m_projectProxyRef->update(Scene::kUpdateAll | Scene::kForceUpdateAllMorphs);
    }
    if (m_modelDrawer) {
        m_modelDrawer->update();
    }
    draw();
}

void RenderTarget::synchronizeMotionState()
{
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::synchronizeMotionState);
    m_projectProxyRef->update(Scene::kUpdateAll | Scene::kForceUpdateAllMorphs | Scene::kResetMotionState);
    m_projectProxyRef->world()->rewind();
    draw();
}

void RenderTarget::synchronizeImplicitly()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    if (m_projectProxyRef) {
        int flags = m_playing ? Scene::kUpdateAll : (Scene::kUpdateCamera | Scene::kUpdateRenderEngines);
        m_projectProxyRef->update(flags);
    }
}

void RenderTarget::initializeOpenGLContext()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    if (!Scene::isInitialized()) {
        bool isCoreProfile = window()->format().profile() == QSurfaceFormat::CoreProfile;
        m_applicationContext.reset(new ApplicationContext(m_projectProxyRef, &m_config, isCoreProfile));
        gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
        connect(m_applicationContext.data(), &ApplicationContext::fileDidChange, this, &RenderTarget::handleFileChange);
        Scene::initialize(m_applicationContext->sharedFunctionResolverInstance());
        m_graphicsDevice.reset(new GraphicsDevice());
        m_graphicsDevice->initialize();
        emit graphicsDeviceChanged();
        m_applicationContext->initializeOpenGLContext(false);
        m_grid->load(m_applicationContext->sharedFunctionResolverInstance());
        m_applicationContext->setViewportRegion(glm::ivec4(0, 0, window()->width(), window()->height()));
        connect(window()->openglContext(), &QOpenGLContext::aboutToBeDestroyed, m_projectProxyRef, &ProjectProxy::reset, Qt::DirectConnection);
        connect(window()->openglContext(), &QOpenGLContext::aboutToBeDestroyed, this, &RenderTarget::releaseOpenGLResources, Qt::DirectConnection);
        toggleRunning(true);
        disconnect(window(), &QQuickWindow::sceneGraphInitialized, this, &RenderTarget::initializeOpenGLContext);
        emit initializedChanged();
        m_renderTimer.start();
        gl::popAnnotationGroup(m_applicationContext.data());
    }
}

void RenderTarget::releaseOpenGLResources()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    disconnect(m_applicationContext.data(), &ApplicationContext::fileDidChange, this, &RenderTarget::handleFileChange);
    m_applicationContext->deleteAllModelProxies(m_projectProxyRef);
    m_applicationContext->release();
    m_currentGizmoRef = 0;
    m_translationGizmo.reset();
    m_orientationGizmo.reset();
    m_modelDrawer.reset();
    m_debugDrawer.reset();
    m_grid.reset();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::enqueueUploadingModel(ModelProxy *model, bool isProject)
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(model);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    const QUuid &uuid = model->uuid();
    VPVL2_VLOG(1, "Enqueued uploading the model " << uuid.toString().toStdString() << " a.k.a " << model->name().toStdString());
    m_applicationContext->enqueueUploadingModel(model, isProject);
}

void RenderTarget::enqueueUploadingEffect(ModelProxy *model)
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(model);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    const QUuid &uuid = model->uuid();
    VPVL2_VLOG(1, "Enqueued uploading the effect " << uuid.toString().toStdString());
    m_applicationContext->enqueueUploadingEffect(model);
}

void RenderTarget::enqueueDeletingModel(ModelProxy *model)
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    if (model) {
        VPVL2_VLOG(1, "The model " << model->uuid().toString().toStdString() << " a.k.a " << model->name().toStdString() << " will be released from RenderTarget");
        if (m_modelDrawer) {
            m_modelDrawer->removeModelRef(model);
        }
        m_applicationContext->enqueueDeletingModelProxy(model);
    }
}

void RenderTarget::commitUploadingModels()
{
    Q_ASSERT(window());
    connect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::performUploadingEnqueuedModels, Qt::DirectConnection);
}

void RenderTarget::commitUploadingEffects()
{
    Q_ASSERT(window());
    connect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::performUploadingEnqueuedEffects, Qt::DirectConnection);
}

void RenderTarget::commitDeletingModels()
{
    if (QQuickWindow *win = window()) {
        connect(win, &QQuickWindow::beforeRendering, this, &RenderTarget::performDeletingEnqueuedModels, Qt::DirectConnection);
    }
    else {
        performDeletingEnqueuedModels();
    }
}

void RenderTarget::performUploadingEnqueuedModels()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::performUploadingEnqueuedModels);
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    QList<ApplicationContext::ModelProxyPair> succeededModelProxies, failedModelProxies;
    m_applicationContext->uploadEnqueuedModelProxies(m_projectProxyRef, succeededModelProxies, failedModelProxies);
    /*
    if (!m_modelDrawer) {
        m_modelDrawer.reset(new SkeletonDrawer());
        connect(m_projectProxyRef, &ProjectProxy::currentTimeIndexChanged, m_modelDrawer.data(), &SkeletonDrawer::markDirty);
        connect(m_projectProxyRef, &ProjectProxy::modelBoneDidReset, m_modelDrawer.data(), &SkeletonDrawer::markDirty);
        connect(m_projectProxyRef, &ProjectProxy::undoDidPerform, m_modelDrawer.data(), &SkeletonDrawer::markDirty);
        connect(m_projectProxyRef, &ProjectProxy::redoDidPerform, m_modelDrawer.data(), &SkeletonDrawer::markDirty);
        m_modelDrawer->initialize();
        m_modelDrawer->setViewProjectionMatrix(Util::fromMatrix4(m_viewProjectionMatrix));
    }
    */
    foreach (const ApplicationContext::ModelProxyPair &pair, succeededModelProxies) {
        ModelProxy *modelProxy = pair.first;
        VPVL2_VLOG(1, "The model " << modelProxy->uuid().toString().toStdString() << " a.k.a " << modelProxy->name().toStdString() << " is uploaded" << (pair.second ? " from the project." : "."));
        connect(modelProxy, &ModelProxy::transformDidCommit, this, &RenderTarget::updateGizmoAndRender);
        connect(modelProxy, &ModelProxy::transformDidDiscard, this, &RenderTarget::updateGizmoAndRender);
        connect(modelProxy, &ModelProxy::firstTargetBoneChanged, this, &RenderTarget::updateGizmoAndRender);
        // connect(modelProxy, &ModelProxy::firstTargetBoneChanged, m_modelDrawer.data(), &SkeletonDrawer::markDirty);
        connect(modelProxy, &ModelProxy::transformTypeChanged, this, &RenderTarget::updateGizmo);
        connect(modelProxy, &ModelProxy::translationChanged, this, &RenderTarget::updateGizmo);
        connect(modelProxy, &ModelProxy::orientationChanged, this, &RenderTarget::updateGizmo);
        connect(modelProxy, &ModelProxy::texturePathDidChange, this, &RenderTarget::handleTextureChange);
        emit uploadingModelDidSucceed(modelProxy, pair.second);
    }
    foreach (const ApplicationContext::ModelProxyPair &pair, failedModelProxies) {
        ModelProxy *modelProxy = pair.first;
        VPVL2_VLOG(1, "The model " << modelProxy->uuid().toString().toStdString() << " a.k.a " << modelProxy->name().toStdString() << " is not uploaded " << (pair.second ? " from the project." : "."));
        emit uploadingModelDidFail(modelProxy, pair.second);
    }
    emit enqueuedModelsDidUpload();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::performUploadingEnqueuedEffects()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::performUploadingEnqueuedEffects);
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    QList<ModelProxy *> succeededEffects, failedEffects;
    m_applicationContext->uploadEnqueuedEffects(m_projectProxyRef, succeededEffects, failedEffects);
    foreach (ModelProxy *modelProxy, succeededEffects) {
        VPVL2_VLOG(1, "The effect " << modelProxy->uuid().toString().toStdString() << " a.k.a " << modelProxy->name().toStdString() << " is uploaded.");
        emit uploadingEffectDidSucceed(modelProxy);
    }
    foreach (ModelProxy *modelProxy, failedEffects) {
        VPVL2_VLOG(1, "The effect " << modelProxy->uuid().toString().toStdString() << " a.k.a " << modelProxy->name().toStdString() << " is not uploaded.");
        emit uploadingEffectDidFail(modelProxy);
    }
    emit enqueuedEffectsDidUpload();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::performDeletingEnqueuedModels()
{
    Q_ASSERT(m_applicationContext);
    if (QQuickWindow *win = window()) {
        Q_ASSERT(win->thread() == thread());
        disconnect(win, &QQuickWindow::beforeRendering, this, &RenderTarget::performDeletingEnqueuedModels);
    }
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    const QList<ModelProxy *> &deletedModelProxies = m_applicationContext->deleteEnqueuedModelProxies(m_projectProxyRef);
    foreach (ModelProxy *modelProxy, deletedModelProxies) {
        VPVL2_VLOG(1, "The model " << modelProxy->uuid().toString().toStdString() << " a.k.a " << modelProxy->name().toStdString() << " is scheduled to be delete from RenderTarget and will be deleted");
        modelProxy->deleteLater();
    }
    emit enqueuedModelsDidDelete();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::performUpdatingLight()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(window(), &QQuickWindow::beforeRendering, this, &RenderTarget::performUpdatingLight);
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    const LightRefObject *light = m_projectProxyRef->light();
    const qreal &shadowDistance = light->shadowDistance();
    const Vector3 &direction = light->data()->direction(),
            &eye = -direction * shadowDistance,
            &center = direction * shadowDistance;
    const glm::mediump_float &aspectRatio = m_shadowMapSize.x() / float(m_shadowMapSize.y());
    const glm::mat4 &lightView = glm::lookAt(glm::vec3(eye.x(), eye.y(), eye.z()),
                                             glm::vec3(center.x(), center.y(), center.z()),
                                             glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 &lightProjection = glm::infinitePerspective(45.0f, aspectRatio, 0.1f);
    m_applicationContext->setLightMatrices(glm::mat4(), lightView, lightProjection);
    Scene *scene = m_projectProxyRef->projectInstanceRef();
    if (light->shadowType() == LightRefObject::SelfShadow) {
        const Vector3 size(m_shadowMapSize.x(), m_shadowMapSize.y(), 1);
        m_applicationContext->createShadowMap(size);
    }
    else {
        scene->setShadowMapRef(0);
    }
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::disconnectProjectSignals()
{
    /* disable below signals behavior while loading project */
    disconnect(m_projectProxyRef, &ProjectProxy::motionDidLoad, this, &RenderTarget::prepareSyncMotionState);
    disconnect(this, &RenderTarget::shadowMapSizeChanged, this, &RenderTarget::prepareUpdatingLight);
    disconnect(m_projectProxyRef, &ProjectProxy::rewindDidPerform, this, &RenderTarget::rewind);
    disconnect(m_projectProxyRef->world(), &WorldProxy::simulationTypeChanged, this, &RenderTarget::prepareSyncMotionState);
    disconnect(m_projectProxyRef->light(), &LightRefObject::directionChanged, this, &RenderTarget::prepareUpdatingLight);
    disconnect(m_projectProxyRef->light(), &LightRefObject::shadowTypeChanged, this, &RenderTarget::prepareUpdatingLight);
}

void RenderTarget::rewind()
{
    m_currentTimeIndex = 0;
    emit currentTimeIndexChanged();
    m_lastTimeIndex = 0;
    emit lastTimeIndexChanged();
    prepareSyncMotionState();
}

void RenderTarget::prepareUploadingModelsInProject()
{
    /* must use Qt::DirectConnection to release OpenGL resources and recreate shadowmap */
    connect(this, &RenderTarget::enqueuedModelsDidUpload, this, &RenderTarget::activateProject, Qt::DirectConnection);
    commitUploadingModels();
}

void RenderTarget::activateProject()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    disconnect(this, &RenderTarget::enqueuedModelsDidUpload, this, &RenderTarget::activateProject);
    setShadowMapSize(m_projectProxyRef->globalSetting("shadow.texture.size", kDefaultShadowMapSize));
    m_applicationContext->release();
    m_applicationContext->resetOrderIndex(m_projectProxyRef->modelProxies().count() + 1);
    m_applicationContext->createShadowMap(Vector3(m_shadowMapSize.x(), m_shadowMapSize.y(), 1));
    m_grid->setVisible(m_projectProxyRef->isGridVisible());
    connect(this, &RenderTarget::shadowMapSizeChanged, this, &RenderTarget::prepareUpdatingLight);
    connect(m_projectProxyRef, &ProjectProxy::motionDidLoad, this, &RenderTarget::prepareSyncMotionState);
    connect(m_projectProxyRef, &ProjectProxy::rewindDidPerform, this, &RenderTarget::rewind);
    connect(m_projectProxyRef->world(), &WorldProxy::simulationTypeChanged, this, &RenderTarget::prepareSyncMotionState);
    connect(m_projectProxyRef->light(), &LightRefObject::directionChanged, this, &RenderTarget::prepareUpdatingLight);
    connect(m_projectProxyRef->light(), &LightRefObject::shadowTypeChanged, this, &RenderTarget::prepareUpdatingLight);
    prepareSyncMotionState();
    prepareUpdatingLight();
}

IGizmo *RenderTarget::translationGizmo() const
{
    if (!m_translationGizmo) {
        m_translationGizmo.reset(CreateMoveGizmo());
        m_translationGizmo->SetSnap(m_snapStepSize.x(), m_snapStepSize.y(), m_snapStepSize.z());
        m_translationGizmo->SetEditMatrix(const_cast<float *>(m_editMatrix.data()));
    }
    return m_translationGizmo.data();
}

IGizmo *RenderTarget::orientationGizmo() const
{
    if (!m_orientationGizmo) {
        m_orientationGizmo.reset(CreateRotateGizmo());
        m_orientationGizmo->SetEditMatrix(const_cast<float *>(m_editMatrix.data()));
        m_orientationGizmo->SetAxisMask(m_visibleGizmoMasks);
    }
    return m_orientationGizmo.data();
}

void RenderTarget::resetOpenGLStates()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    window()->resetOpenGLState();
    Scene::setRequiredOpenGLState();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::clearScene()
{
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    const QColor &color = m_projectProxyRef ? m_projectProxyRef->screenColor() : QColor(Qt::white);
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void RenderTarget::drawGrid()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    m_grid->draw(m_viewProjectionMatrix);
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::drawShadowMap()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    m_applicationContext->renderShadowMap();
    m_applicationContext->renderOffscreen();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::drawScene()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    Array<IRenderEngine *> enginesForPreProcess, enginesForStandard, enginesForPostProcess;
    Hash<HashPtr, IEffect *> nextPostEffects;
    Scene *scene = m_projectProxyRef->projectInstanceRef();
    scene->getRenderEnginesByRenderOrder(enginesForPreProcess,
                                         enginesForStandard,
                                         enginesForPostProcess,
                                         nextPostEffects);
    const bool isProjectiveShadow = m_projectProxyRef->light()->shadowType() == LightRefObject::ProjectiveShadow;
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    for (int i = enginesForPostProcess.count() - 1; i >= 0; i--) {
        IRenderEngine *engine = enginesForPostProcess[i];
        engine->preparePostProcess();
    }
    for (int i = 0, nengines = enginesForPreProcess.count(); i < nengines; i++) {
        IRenderEngine *engine = enginesForPreProcess[i];
        engine->performPreProcess();
    }
    for (int i = 0, nengines = enginesForStandard.count(); i < nengines; i++) {
        IRenderEngine *engine = enginesForStandard[i];
        engine->renderModel(0);
        engine->renderEdge(0);
        if (isProjectiveShadow) {
            engine->renderShadow(0);
        }
    }
    for (int i = 0, nengines = enginesForPostProcess.count(); i < nengines; i++) {
        IRenderEngine *engine = enginesForPostProcess[i];
        IEffect *const *nextPostEffect = nextPostEffects[engine];
        engine->performPostProcess(*nextPostEffect);
    }
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::drawDebug()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(m_projectProxyRef);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    WorldProxy *worldProxy = m_projectProxyRef->world();
    if (worldProxy->isDebugEnabled() && worldProxy->simulationType() != WorldProxy::DisableSimulation) {
        gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
        if (!m_debugDrawer) {
            m_debugDrawer.reset(new DebugDrawer());
            m_debugDrawer->initialize();
            m_debugDrawer->setViewProjectionMatrix(Util::fromMatrix4(m_viewProjectionMatrix));
            m_debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawAabb |
                                        // btIDebugDraw::DBG_DrawConstraintLimits |
                                        // btIDebugDraw::DBG_DrawConstraints |
                                        // btIDebugDraw::DBG_DrawContactPoints |
                                        // btIDebugDraw::DBG_DrawFeaturesText |
                                        // btIDebugDraw::DBG_DrawText |
                                        // btIDebugDraw::DBG_FastWireframe |
                                        // btIDebugDraw::DBG_DrawWireframe |
                                        0);
            worldProxy->setDebugDrawer(m_debugDrawer.data());
        }
        worldProxy->debugDraw();
        m_debugDrawer->flush();
        gl::popAnnotationGroup(m_applicationContext.data());
    }
}

void RenderTarget::drawModelBones()
{
    Q_ASSERT(m_projectProxyRef);
    ModelProxy *currentModelRef = m_projectProxyRef->currentModel();
    if (!m_playing && m_editMode == SelectMode && currentModelRef && currentModelRef->isVisible()) {
        Q_ASSERT(m_applicationContext);
        Q_ASSERT(window());
        Q_ASSERT(window()->thread() == thread());
        gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
        if (m_modelDrawer) {
            float32 m[16];
            m_modelDrawer->setModelProxyRef(currentModelRef);
            m_applicationContext->getMatrix(m, currentModelRef->data(), IApplicationContext::kCameraMatrix | IApplicationContext::kWorldMatrix);
            m_modelDrawer->draw(Util::fromMatrix4(glm::make_mat4(m)));
        }
        gl::popAnnotationGroup(m_applicationContext.data());
    }
}

void RenderTarget::drawCurrentGizmo()
{
    if (!m_playing && m_currentGizmoRef) {
        Q_ASSERT(m_applicationContext);
        Q_ASSERT(window());
        Q_ASSERT(window()->thread() == thread());
        gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
        m_currentGizmoRef->Draw();
        gl::popAnnotationGroup(m_applicationContext.data());
    }
}

void RenderTarget::drawEffectParameterUIWidgets()
{
    if (!m_playing) {
        Q_ASSERT(m_applicationContext);
        Q_ASSERT(window());
        Q_ASSERT(window()->thread() == thread());
        m_applicationContext->renderEffectParameterUIWidgets();
    }
}

void RenderTarget::drawOffscreen(QOpenGLFramebufferObject *fbo)
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    m_applicationContext->setViewportRegion(glm::ivec4(0, 0, fbo->width(), fbo->height()));
    Scene::setRequiredOpenGLState();
    drawShadowMap();
    Q_ASSERT(fbo->isValid());
    fbo->bind();
    glViewport(0, 0, fbo->width(), fbo->height());
    clearScene();
    drawGrid();
    drawScene();
    m_applicationContext->setViewportRegion(glm::ivec4(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height()));
    QOpenGLFramebufferObject::bindDefault();
    gl::popAnnotationGroup(m_applicationContext.data());
}

void RenderTarget::updateViewport()
{
    Q_ASSERT(m_applicationContext);
    Q_ASSERT(window());
    Q_ASSERT(window()->thread() == thread());
    int w = m_viewport.width(), h = m_viewport.height();
    gl::pushAnnotationGroup(Q_FUNC_INFO, m_applicationContext.data());
    if (isDirty()) {
        glm::mat4 cameraWorld, cameraView, cameraProjection;
        m_applicationContext->setViewportRegion(glm::ivec4(m_viewport.x(), m_viewport.y(), w, h));
        m_applicationContext->updateCameraMatrices();
        m_applicationContext->getCameraMatrices(cameraWorld, cameraView, cameraProjection);
        m_viewMatrix = cameraView;
        m_projectionMatrix = cameraProjection;
        m_viewProjectionMatrix = cameraProjection * cameraView;
        if (m_debugDrawer) {
            m_debugDrawer->setViewProjectionMatrix(Util::fromMatrix4(m_viewProjectionMatrix));
        }
        if (m_modelDrawer) {
            m_modelDrawer->setViewProjectionMatrix(Util::fromMatrix4(m_viewProjectionMatrix));
        }
        IGizmo *translationGizmoRef = translationGizmo(), *orientationGizmoRef = orientationGizmo();
        translationGizmoRef->SetScreenDimension(w, h);
        translationGizmoRef->SetCameraMatrix(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection));
        orientationGizmoRef->SetScreenDimension(w, h);
        orientationGizmoRef->SetCameraMatrix(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection));
        emit viewMatrixChanged();
        emit projectionMatrixChanged();
        setDirty(false);
    }
    glViewport(m_viewport.x(), m_viewport.y(), w, h);
    gl::popAnnotationGroup(m_applicationContext.data());
}
