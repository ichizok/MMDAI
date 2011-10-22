/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2010-2011  hkrn                                    */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAI project team nor the names of     */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#ifndef INFOPANEL_H
#define INFOPANEL_H

#include <GL/glew.h>
#include <QtCore/QtCore>
#include <QtOpenGL/QtOpenGL>
#include "util.h"

namespace vpvl {
class Bone;
class PMDModel;
}

namespace internal {

class InfoPanel
{
public:
    InfoPanel(QGLWidget *widget)
        : m_widget(widget),
          m_font("System", 16),
          m_fontMetrics(m_font),
          m_rect(0, 0, 256, 256),
          m_textureID(0),
          m_width(0),
          m_height(0),
          m_visible(true)
    {
    }
    ~InfoPanel() {
        deleteTexture();
    }

    void resize(int width, int height) {
        m_width = width;
        m_height = height;
    }
    void load() {
    }
    void update() {
        int height = m_fontMetrics.height();
        QImage texture(m_rect.size(), QImage::Format_ARGB32);
        texture.fill(0);
        QPainter painter(&texture);
        painter.setFont(m_font);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.drawText(0, height, m_modelText);
        painter.drawText(0, height * 2, m_boneText);
        painter.drawText(0, height * 3, m_FPSText);
        painter.end();
        deleteTexture();
        m_textureID = m_widget->bindTexture(QGLWidget::convertToGLFormat(texture.rgbSwapped()));
    }
    void draw() {
        if (!m_visible)
            return;
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, m_width, 0, m_height);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0, m_height - m_rect.height(), 0);
        m_widget->drawTexture(m_rect, m_textureID);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
    }

    void setModel(vpvl::PMDModel *model) {
        m_modelText = QString("Model: %1").arg(internal::toQString(model));
    }
    void setBone(vpvl::Bone *bone) {
        m_boneText = QString("Bone: %1").arg(internal::toQString(bone));
    }
    void setFPS(float value) {
        m_FPSText = QString("FPS: %1").arg(value);
    }
    void setVisible(bool value) {
        m_visible = value;
    }

private:
    void deleteTexture() {
        if (m_textureID) {
            m_widget->deleteTexture(m_textureID);
            m_textureID = 0;
        }
    }

    QGLWidget *m_widget;
    QFont m_font;
    QFontMetrics m_fontMetrics;
    QRect m_rect;
    QString m_modelText;
    QString m_boneText;
    QString m_FPSText;
    GLuint m_textureID;
    int m_width;
    int m_height;
    bool m_visible;
};

}

#endif // INFOPANEL_H
