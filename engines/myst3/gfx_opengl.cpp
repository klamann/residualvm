/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if defined(WIN32)
#include <windows.h>
// winnt.h defines ARRAYSIZE, but we want our own one...
#undef ARRAYSIZE
#endif

#include "common/rect.h"
#include "common/textconsole.h"

#if defined(USE_OPENGL) && !defined(USE_GLES2)

#include "graphics/colormasks.h"
#include "graphics/opengl/context.h"
#include "graphics/surface.h"

#include "engines/myst3/gfx.h"
#include "engines/myst3/gfx_opengl.h"
#include "engines/myst3/gfx_opengl_texture.h"

namespace Myst3 {

OpenGLRenderer::OpenGLRenderer(OSystem *system) :
		Renderer(system) {
}

OpenGLRenderer::~OpenGLRenderer() {
}

void OpenGLRenderer::setViewport(const FloatRect &viewport, bool is3d) {
	int32 screenHeight = _system->getHeight();
	glViewport(viewport.left(), screenHeight - viewport.bottom(), viewport.width(), viewport.height());

	if (is3d) {
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(_projectionMatrix.getData());

		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(_modelViewMatrix.getData());
	} else {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0., 1., 1., 0., -1., 1.);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
}

Texture *OpenGLRenderer::createTexture(const Graphics::Surface &surface) {
	return new OpenGLTexture(surface);
}

void OpenGLRenderer::init() {
	debug("Initializing OpenGL Renderer");

	// Check the available OpenGL extensions
	if (!OpenGLContext.NPOTSupported) {
		warning("GL_ARB_texture_non_power_of_two is not available.");
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::clear() {
	glClearColor(0.f, 0.f, 0.f, 1.f); // Solid black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor3f(1.0f, 1.0f, 1.0f);
}

void OpenGLRenderer::drawRect2D(const FloatRect &screenRect, uint32 color) {
	uint8 a, r, g, b;
	Graphics::colorToARGB< Graphics::ColorMasks<8888> >(color, a, r, g, b);

	glDisable(GL_TEXTURE_2D);
	glColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);

	if (a != 255) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	glBegin(GL_TRIANGLE_STRIP);
		glVertex3f(screenRect.left(), screenRect.bottom(), 0.0f);
		glVertex3f(screenRect.right(), screenRect.bottom(), 0.0f);
		glVertex3f(screenRect.left(), screenRect.top(), 0.0f);
		glVertex3f(screenRect.right(), screenRect.top(), 0.0f);
	glEnd();

	glDisable(GL_BLEND);
}

void OpenGLRenderer::drawTexturedRect2D(const FloatRect &screenRect, const FloatRect &textureRect,
                                        Texture &texture, float transparency, bool additiveBlending) {

	OpenGLTexture &glTexture = static_cast<OpenGLTexture &>(texture);

	const float tLeft   = textureRect.left()   * glTexture.width  / (float)glTexture.internalWidth;
	const float tWidth  = textureRect.width()  * glTexture.width  / (float)glTexture.internalWidth;
	const float tTop    = textureRect.top()    * glTexture.height / (float)glTexture.internalHeight;
	const float tHeight = textureRect.height() * glTexture.height / (float)glTexture.internalHeight;

	float sLeft   = screenRect.left();
	float sTop    = screenRect.top();
	float sRight  = sLeft + screenRect.width();
	float sBottom = sTop  + screenRect.height();

	if (glTexture.upsideDown) {
		SWAP(sTop, sBottom);
	}

	if (transparency >= 0.0) {
		if (additiveBlending) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		} else {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		glEnable(GL_BLEND);
	} else {
		transparency = 1.0;
	}

	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, transparency);
	glDepthMask(GL_FALSE);

	glBindTexture(GL_TEXTURE_2D, glTexture.id);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(tLeft, tTop + tHeight);
		glVertex3f(sLeft + 0, sBottom, 1.0f);

		glTexCoord2f(tLeft + tWidth, tTop + tHeight);
		glVertex3f(sRight, sBottom, 1.0f);

		glTexCoord2f(tLeft, tTop);
		glVertex3f(sLeft + 0, sTop + 0, 1.0f);

		glTexCoord2f(tLeft + tWidth, tTop);
		glVertex3f(sRight, sTop + 0, 1.0f);
	glEnd();

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void OpenGLRenderer::drawFace(uint face, Texture *texture) {
	OpenGLTexture *glTexture = static_cast<OpenGLTexture *>(texture);

	// Used fragment of the texture
	const float w = glTexture->width  / (float) glTexture->internalWidth;
	const float h = glTexture->height / (float) glTexture->internalHeight;

	glBindTexture(GL_TEXTURE_2D, glTexture->id);
	glBegin(GL_TRIANGLE_STRIP);
	for (uint i = 0; i < 4; i++) {
		glTexCoord2f(w * cubeVertices[5 * (4 * face + i) + 0], h * cubeVertices[5 * (4 * face + i) + 1]);
		glVertex3f(cubeVertices[5 * (4 * face + i) + 2], cubeVertices[5 * (4 * face + i) + 3], cubeVertices[5 * (4 * face + i) + 4]);
	}
	glEnd();
}

void OpenGLRenderer::drawCube(Texture **textures) {
	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_FALSE);

	for (uint i = 0; i < 6; i++) {
		drawFace(i, textures[i]);
	}

	glDepthMask(GL_TRUE);
}

void OpenGLRenderer::drawTexturedRect3D(const Math::Vector3d &topLeft, const Math::Vector3d &bottomLeft,
		const Math::Vector3d &topRight, const Math::Vector3d &bottomRight, Texture &texture) {

	OpenGLTexture &glTexture = static_cast<OpenGLTexture &>(texture);

	const float w = glTexture.width  / (float)glTexture.internalWidth;
	const float h = glTexture.height / (float)glTexture.internalHeight;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	glBindTexture(GL_TEXTURE_2D, glTexture.id);

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0, 0);
		glVertex3f(-topLeft.x(), topLeft.y(), topLeft.z());

		glTexCoord2f(0, h);
		glVertex3f(-bottomLeft.x(), bottomLeft.y(), bottomLeft.z());

		glTexCoord2f(w, 0);
		glVertex3f(-topRight.x(), topRight.y(), topRight.z());

		glTexCoord2f(w, h);
		glVertex3f(-bottomRight.x(), bottomRight.y(), bottomRight.z());
	glEnd();

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

Graphics::Surface *OpenGLRenderer::getScreenshot(const Common::Rect &screenViewport) {
	Graphics::Surface *s = new Graphics::Surface();
	s->create(screenViewport.width(), screenViewport.height(), Texture::getRGBAPixelFormat());

	glReadPixels(screenViewport.left, screenViewport.top, screenViewport.width(), screenViewport.height(), GL_RGBA, GL_UNSIGNED_BYTE, s->getPixels());

	flipVertical(s);

	return s;
}

Texture *OpenGLRenderer::copyScreenshotToTexture(const Common::Rect &screenViewport) {
	OpenGLTexture *texture = new OpenGLTexture();
	texture->copyFromFramebuffer(screenViewport);

	return texture;
}

} // End of namespace Myst3

#endif
