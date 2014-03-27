/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmoztexturenode.h"
#include "quickmozview.h"
#include <QQuickWindow>
#include <QThread>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <QtQuick/QSGSimpleMaterial>

#define LOCAL_GL_TEXTURE_EXTERNAL 0x8D65

struct MozExternalTexture
{
    GLuint id;
};

class MozTextureShader : public QSGSimpleMaterialShader<MozExternalTexture>
{
    QSG_DECLARE_SIMPLE_SHADER(MozTextureShader, MozExternalTexture)

public:

    const char *vertexShader() const {
        return  "attribute highp vec4 aVertex;              \n"
                "attribute highp vec2 aTexCoord;            \n"
                "uniform highp mat4 qt_Matrix;              \n"
                "varying highp vec2 vTexCoord;              \n"
                "void main() {                              \n"
                "    gl_Position = qt_Matrix * aVertex;     \n"
                "    vTexCoord = aTexCoord;                 \n"
                "}";
    }

    const char *fragmentShader() const {
        return  "#extension GL_OES_EGL_image_external : require                     \n"
                "uniform lowp float qt_Opacity;                                     \n"
                "uniform lowp samplerExternalOES texture;                           \n"
                "varying highp vec2 vTexCoord;                                      \n"
                "void main() {                                                      \n"
                "    gl_FragColor = qt_Opacity * texture2D(texture, vTexCoord);     \n"
                "}";
    }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "aVertex" << "aTexCoord";
    }

    void updateState(const MozExternalTexture *texture, const MozExternalTexture *) {
        glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, texture->id);
    }

    void deactivate() {
        glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, 0);
    }

};

void MozTextureNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateTexturedRectGeometry(geometry(), rect, QRectF(0, 0, 1, 1));
    markDirty(QSGNode::DirtyGeometry);
}


MozTextureNode::MozTextureNode(QuickMozView* aView)
  : m_id(0)
  , m_view(aView)
  , mIsConnected(false)
{
    setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));

    QSGSimpleMaterial<MozExternalTexture> *material = MozTextureShader::createMaterial();
    material->setFlag(QSGMaterial::Blending, false);
    material->state()->id = 0;
    setMaterial(material);

    setFlags(OwnsMaterial | OwnsGeometry);
}

void
MozTextureNode::newTexture(int id, const QSize &size)
{
    m_mutex.lock();
    m_id = id;
    m_mutex.unlock();

    // We cannot call QQuickWindow::update directly here, as this is only allowed
    // from the rendering thread or GUI thread.
    Q_EMIT pendingNewTexture();
}

#define LOCAL_GL_TEXTURE_EXTERNAL 0x8D65
// Before the scene graph starts to render, we update to the pending texture
void
MozTextureNode::prepareNode()
{
    m_mutex.lock();
    int newId = m_id;
    m_id = 0;
    m_mutex.unlock();
    if (newId) {
        MozExternalTexture *texture = static_cast<QSGSimpleMaterial<MozExternalTexture> *>(material())->state();
        texture->id = newId;
        markDirty(QSGNode::DirtyMaterial);
    }
}
