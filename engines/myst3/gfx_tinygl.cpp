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

#include "common/config-manager.h"
#include "common/rect.h"
#include "common/textconsole.h"

#include "graphics/colormasks.h"
#include "graphics/surface.h"

#include "math/vector2d.h"
#include "math/glmath.h"

#include "engines/myst3/gfx.h"
#include "engines/myst3/gfx_tinygl.h"
#include "engines/myst3/gfx_tinygl_texture.h"
#include "graphics/tinygl/zblit.h"

namespace Myst3 {

TinyGLRenderer::TinyGLRenderer(OSystem *system) :
		Renderer(system),
		_fb(nullptr) {
}

TinyGLRenderer::~TinyGLRenderer() {
}

void TinyGLRenderer::setViewport(const FloatRect &viewport, bool is3d) {
	tglViewport(viewport.left(), viewport.top(), viewport.width(), viewport.height());

	if (is3d) {
		tglMatrixMode(TGL_PROJECTION);
		tglLoadMatrixf(_projectionMatrix.getData());

		tglMatrixMode(TGL_MODELVIEW);
		tglLoadMatrixf(_modelViewMatrix.getData());
	} else {
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglOrtho(0., 1., 1., 0., -1., 1.);

		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
	}
}

Texture *TinyGLRenderer::createTexture(const Graphics::Surface &surface) {
	return new TinyGLTexture(surface);
}

void TinyGLRenderer::init() {
	debug("Initializing Software 3D Renderer");

	Graphics::PixelBuffer screenBuffer = _system->getScreenPixelBuffer();
	_fb = new TinyGL::FrameBuffer(_system->getWidth(), _system->getHeight(), screenBuffer);
	TinyGL::glInit(_fb, 512);
	tglEnableDirtyRects(ConfMan.getBool("dirtyrects"));

	tglMatrixMode(TGL_PROJECTION);
	tglLoadIdentity();

	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();

	tglDisable(TGL_LIGHTING);
	tglEnable(TGL_TEXTURE_2D);
	tglEnable(TGL_DEPTH_TEST);
}

void TinyGLRenderer::clear() {
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
	tglColor3f(1.0f, 1.0f, 1.0f);
}

void TinyGLRenderer::drawRect2D(const FloatRect &screenRect, uint32 color) {
	uint8 a, r, g, b;
	Graphics::colorToARGB< Graphics::ColorMasks<8888> >(color, a, r, g, b);

	tglDisable(TGL_TEXTURE_2D);
	tglColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);

	if (a != 255) {
		tglEnable(TGL_BLEND);
		tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
	}

	tglBegin(TGL_TRIANGLE_STRIP);
		tglVertex3f(screenRect.left(), screenRect.bottom(), 0.0f);
		tglVertex3f(screenRect.right(), screenRect.bottom(), 0.0f);
		tglVertex3f(screenRect.left(), screenRect.top(), 0.0f);
		tglVertex3f(screenRect.right(), screenRect.top(), 0.0f);
	tglEnd();

	tglDisable(TGL_BLEND);
}

void TinyGLRenderer::drawTexturedRect2D(const FloatRect &screenRect, const FloatRect &textureRect,
                                        Texture &texture, float transparency, bool additiveBlending) {
	if (transparency >= 0.0) {
		if (additiveBlending) {
			tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE);
		} else {
			tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
		}
		tglEnable(TGL_BLEND);
	} else {
		transparency = 1.0;
	}

	// HACK: tglBlit is not affected by the viewport, so we offset the draw coordinates here
	int viewPort[4];
	tglGetIntegerv(TGL_VIEWPORT, viewPort);

	const float sLeft   = viewPort[2] * screenRect.left()  + viewPort[0];
	const float sTop    = viewPort[3] * screenRect.top()   + viewPort[1];
	const float sWidth  = viewPort[2] * screenRect.width();
	const float sHeight = viewPort[3] * screenRect.height();

	tglEnable(TGL_TEXTURE_2D);
	tglDepthMask(TGL_FALSE);

	Graphics::BlitTransform transform(sLeft, sTop);
	transform.sourceRectangle(textureRect.left() * texture.width, textureRect.top() * texture.height, sWidth, sHeight);
	transform.tint(transparency);
	tglBlit(((TinyGLTexture &)texture).getBlitTexture(), transform);

	tglDisable(TGL_BLEND);
	tglDepthMask(TGL_TRUE);
}

void TinyGLRenderer::drawFace(uint face, Texture *texture) {
	TinyGLTexture *glTexture = static_cast<TinyGLTexture *>(texture);

	tglBindTexture(TGL_TEXTURE_2D, glTexture->id);
	tglBegin(TGL_TRIANGLE_STRIP);
	for (uint i = 0; i < 4; i++) {
		tglTexCoord2f(cubeVertices[5 * (4 * face + i) + 0], cubeVertices[5 * (4 * face + i) + 1]);
		tglVertex3f(cubeVertices[5 * (4 * face + i) + 2], cubeVertices[5 * (4 * face + i) + 3], cubeVertices[5 * (4 * face + i) + 4]);
	}
	tglEnd();
}

void TinyGLRenderer::drawCube(Texture **textures) {
	tglEnable(TGL_TEXTURE_2D);
	tglDepthMask(TGL_FALSE);

	for (uint i = 0; i < 6; i++) {
		drawFace(i, textures[i]);
	}

	tglDepthMask(TGL_TRUE);
}

void TinyGLRenderer::drawTexturedRect3D(const Math::Vector3d &topLeft, const Math::Vector3d &bottomLeft,
		const Math::Vector3d &topRight, const Math::Vector3d &bottomRight, Texture &texture) {

	TinyGLTexture &glTexture = static_cast<TinyGLTexture &>(texture);

	tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
	tglEnable(TGL_BLEND);
	tglDepthMask(TGL_FALSE);

	tglBindTexture(TGL_TEXTURE_2D, glTexture.id);

	tglBegin(TGL_TRIANGLE_STRIP);
		tglTexCoord2f(0, 0);
		tglVertex3f(-topLeft.x(), topLeft.y(), topLeft.z());

		tglTexCoord2f(0, 1);
		tglVertex3f(-bottomLeft.x(), bottomLeft.y(), bottomLeft.z());

		tglTexCoord2f(1, 0);
		tglVertex3f(-topRight.x(), topRight.y(), topRight.z());

		tglTexCoord2f(1, 1);
		tglVertex3f(-bottomRight.x(), bottomRight.y(), bottomRight.z());
	tglEnd();

	tglDisable(TGL_BLEND);
	tglDepthMask(TGL_TRUE);
}

Graphics::Surface *TinyGLRenderer::getScreenshot(const Common::Rect &screenViewport) {
	Graphics::Surface fullScreen;
	fullScreen.create(_system->getWidth(), _system->getHeight(), Texture::getRGBAPixelFormat());

	Graphics::PixelBuffer buf(fullScreen.format, (byte *)fullScreen.getPixels());
	_fb->copyToBuffer(buf);

	Graphics::Surface viewportSurface = fullScreen.getSubArea(screenViewport);

	Graphics::Surface *out = new Graphics::Surface();
	out->copyFrom(viewportSurface);

	fullScreen.free();

	return out;
}

void TinyGLRenderer::flipBuffer() {
	TinyGL::tglPresentBuffer();
}

} // End of namespace Myst3
