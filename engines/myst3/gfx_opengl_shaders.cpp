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

#include "common/scummsys.h"

#if defined(USE_GLES2) || defined(USE_OPENGL_SHADERS)

#include "engines/myst3/gfx_opengl_shaders.h"
#include "engines/myst3/gfx_opengl_texture.h"
#include "engines/myst3/node_opengl_shaders.h"

#include "graphics/colormasks.h"
#include "graphics/renderer.h"
#include "graphics/surface.h"

#ifdef USE_OPENGL
#include "graphics/opengl/context.h"
#endif

#include "image/dds.h"

#include "math/glmath.h"

namespace Myst3 {

static const GLfloat boxVertices[] = {
	// XS   YT
	0.0, 0.0,
	1.0, 0.0,
	0.0, 1.0,
	1.0, 1.0,
};

ShaderRenderer::ShaderRenderer(OSystem *system) :
		Renderer(system),
		_boxShader(nullptr),
		_rect3dCubeShader(nullptr),
		_effectsCubeShader(nullptr),
		_effectsFrameShader(nullptr),
		_rect3dShader(nullptr),
		_boxVBO(0),
		_cubeVBO(0),
		_rect3dVBO(0),
		_quadEBO(0) {

	// Compute the cube faces Axis Aligned Bounding Boxes
	for (uint i = 0; i < ARRAYSIZE(_cubeFacesAABB); i++) {
		for (uint j = 0; j < 4; j++) {
			_cubeFacesAABB[i].expand(Math::Vector3d(cubeVertices[5 * (4 * i + j) + 2], cubeVertices[5 * (4 * i + j) + 3], cubeVertices[5 * (4 * i + j) + 4]));
		}
	}
}

ShaderRenderer::~ShaderRenderer() {
	OpenGL::Shader::freeBuffer(_boxVBO);
	OpenGL::Shader::freeBuffer(_cubeVBO);
	OpenGL::Shader::freeBuffer(_rect3dVBO);
	OpenGL::Shader::freeBuffer(_quadEBO);

	delete _boxShader;
	delete _rect3dCubeShader;
	delete _effectsCubeShader;
	delete _effectsFrameShader;
	delete _rect3dShader;
}

void ShaderRenderer::setViewport(const FloatRect &viewport, bool is3d) {
	int32 screenHeight = _system->getHeight();
	glViewport(viewport.left(), screenHeight - viewport.bottom(), viewport.width(), viewport.height());
}

void ShaderRenderer::setupQuadEBO() {
	unsigned short quadIndices[6 * 100];

	unsigned short start = 0;
	for (unsigned short *p = quadIndices; p < &quadIndices[6 * 100]; p += 6) {
		p[0] = p[3] = start++;
		p[1] = start++;
		p[2] = p[4] = start++;
		p[5] = start++;
	}

	_quadEBO = OpenGL::Shader::createBuffer(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
}

Texture *ShaderRenderer::createTexture(const Graphics::Surface &surface) {
	return new OpenGLTexture(surface);
}

OpenGL::Shader *ShaderRenderer::createCubeEffectsShaderInstance() {
	return _effectsCubeShader->clone();
}

OpenGL::Shader *Myst3::ShaderRenderer::createFrameEffectsShaderInstance() {
	return _effectsFrameShader->clone();
}

NodeRenderer *ShaderRenderer::createNodeRenderer(Node &node, Layout &layout, GameState &state, ResourceLoader &resourceLoader) {
	return new NodeShaderRenderer(node, layout, *this, state, resourceLoader);
}

void ShaderRenderer::init() {
	debug("Initializing OpenGL Renderer with shaders");

	glEnable(GL_DEPTH_TEST);

	static const char* attributes[] = { "position", "texcoord", NULL };
	_boxShader = OpenGL::Shader::fromFiles("myst3_box", attributes);
	_boxVBO = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER, sizeof(boxVertices), boxVertices);
	_boxShader->enableVertexAttribute("position", _boxVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);
	_boxShader->enableVertexAttribute("texcoord", _boxVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);

	_cubeVBO = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices);

	_rect3dShader = OpenGL::Shader::fromFiles("myst3_rect3d", attributes);
	_rect3dVBO = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER, 20 * sizeof(float), NULL, GL_STREAM_DRAW);
	_rect3dShader->enableVertexAttribute("texcoord", _rect3dVBO, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(float), 0);
	_rect3dShader->enableVertexAttribute("position", _rect3dVBO, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 2 * sizeof(float));

	_rect3dCubeShader = OpenGL::Shader::fromFiles("myst3_rect3d", attributes);
	_rect3dCubeShader->enableVertexAttribute("texcoord", _cubeVBO, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(float), 0);
	_rect3dCubeShader->enableVertexAttribute("position", _cubeVBO, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 2 * sizeof(float));

	_effectsCubeShader = OpenGL::Shader::fromFiles("myst3_rect3d", "myst3_effects", attributes);
	_effectsCubeShader->enableVertexAttribute("texcoord", _cubeVBO, 2, GL_FLOAT, GL_TRUE, 5 * sizeof(float), 0);
	_effectsCubeShader->enableVertexAttribute("position", _cubeVBO, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 2 * sizeof(float));

	_effectsFrameShader = OpenGL::Shader::fromFiles("myst3_box", "myst3_effects", attributes);
	_effectsFrameShader->enableVertexAttribute("position", _boxVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);
	_effectsFrameShader->enableVertexAttribute("texcoord", _boxVBO, 2, GL_FLOAT, GL_TRUE, 2 * sizeof(float), 0);

	setupQuadEBO();
}

void ShaderRenderer::clear() {
	glClearColor(0.f, 0.f, 0.f, 1.f); // Solid black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ShaderRenderer::drawRect2D(const FloatRect &screenRect, uint32 color) {
	uint8 a, r, g, b;
	Graphics::colorToARGB< Graphics::ColorMasks<8888> >(color, a, r, g, b);

	_boxShader->use();
	_boxShader->setUniform("textured", false);
	_boxShader->setUniform("color", Math::Vector4d(r / 255.0, g / 255.0, b / 255.0, a / 255.0));
	_boxShader->setUniform("verOffsetXY", Math::Vector2d(screenRect.left(), screenRect.top()));
	_boxShader->setUniform("verSizeWH", Math::Vector2d(screenRect.width(), screenRect.height()));
	_boxShader->setUniform("flipY", false);

	glDepthMask(GL_FALSE);

	if (a != 255) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void ShaderRenderer::drawTexturedRect2D(const FloatRect &screenRect, const FloatRect &textureRect,
                                        Texture &texture, float transparency, bool additiveBlending) {
	OpenGLTexture &glTexture = static_cast<OpenGLTexture &>(texture);

	const float tLeft   = textureRect.left()   * glTexture.width  / (float)glTexture.internalWidth;
	const float tWidth  = textureRect.width()  * glTexture.width  / (float)glTexture.internalWidth;
	const float tTop    = textureRect.top()    * glTexture.height / (float)glTexture.internalHeight;
	const float tHeight = textureRect.height() * glTexture.height / (float)glTexture.internalHeight;

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

	_boxShader->use();
	_boxShader->setUniform("textured", true);
	_boxShader->setUniform("color", Math::Vector4d(1.0f, 1.0f, 1.0f, transparency));
	_boxShader->setUniform("verOffsetXY", Math::Vector2d(screenRect.left(), screenRect.top()));
	_boxShader->setUniform("verSizeWH", Math::Vector2d(screenRect.width(), screenRect.height()));
	_boxShader->setUniform("texOffsetXY", Math::Vector2d(tLeft, tTop));
	_boxShader->setUniform("texSizeWH", Math::Vector2d(tWidth, tHeight));
	_boxShader->setUniform("flipY", glTexture.upsideDown);

	glDepthMask(GL_FALSE);

	glBindTexture(GL_TEXTURE_2D, glTexture.id);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void ShaderRenderer::drawCube(Texture **textures) {
	OpenGLTexture *texture0 = static_cast<OpenGLTexture *>(textures[0]);

	glDepthMask(GL_FALSE);

	_rect3dCubeShader->use();
	_rect3dCubeShader->setUniform1f("texScale", texture0->width / (float) texture0->internalWidth);
	_rect3dCubeShader->setUniform("mvpMatrix", _mvpMatrix);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[0])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[1])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[2])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[3])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[4])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);

	glBindTexture(GL_TEXTURE_2D, static_cast<OpenGLTexture *>(textures[5])->id);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);

	glDepthMask(GL_TRUE);
}

void ShaderRenderer::drawTexturedRect3D(const Math::Vector3d &topLeft, const Math::Vector3d &bottomLeft,
		const Math::Vector3d &topRight, const Math::Vector3d &bottomRight, Texture &texture) {
	OpenGLTexture &glTexture = static_cast<OpenGLTexture &>(texture);

	const float w = glTexture.width  / (float)glTexture.internalWidth;
	const float h = glTexture.height / (float)glTexture.internalHeight;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	glBindTexture(GL_TEXTURE_2D, glTexture.id);

	const GLfloat vertices[] = {
		// S   T         X                 Y                 Z
		   0,  0,  -topLeft.x(),      topLeft.y(),      topLeft.z(),
		   0,  h,  -bottomLeft.x(),   bottomLeft.y(),   bottomLeft.z(),
		   w,  0,  -topRight.x(),     topRight.y(),     topRight.z(),
		   w,  h,  -bottomRight.x(),  bottomRight.y(),  bottomRight.z(),
	};

	_rect3dShader->use();
	_rect3dShader->setUniform1f("texScale", 1.0f);
	_rect3dShader->setUniform("mvpMatrix", _mvpMatrix);
	glBindBuffer(GL_ARRAY_BUFFER, _rect3dVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, 20 * sizeof(float), vertices);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

Graphics::Surface *ShaderRenderer::getScreenshot(const Common::Rect &screenViewport) {
	Graphics::Surface *s = new Graphics::Surface();
	s->create(screenViewport.width(), screenViewport.height(), Texture::getRGBAPixelFormat());

	glReadPixels(screenViewport.left, screenViewport.top, screenViewport.width(), screenViewport.height(), GL_RGBA, GL_UNSIGNED_BYTE, s->getPixels());

	flipVertical(s);

	return s;
}

Texture *ShaderRenderer::copyScreenshotToTexture(const Common::Rect &screenViewport) {
	OpenGLTexture *texture = new OpenGLTexture();
	texture->copyFromFramebuffer(screenViewport);

	return texture;
}

} // End of namespace Myst3

#endif
