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

#include "engines/myst3/gfx.h"
#include "engines/myst3/node_software.h"
#include "engines/myst3/resource_loader.h"

#include "graphics/colormasks.h"
#include "graphics/renderer.h"
#include "graphics/surface.h"

#include "image/dds.h"

#include "math/glmath.h"

namespace Myst3 {

const float Renderer::cubeVertices[] = {
	// S     T      X      Y      Z
	0.0f, 1.0f, -320.0f, -320.0f, -320.0f,
	1.0f, 1.0f,  320.0f, -320.0f, -320.0f,
	0.0f, 0.0f, -320.0f,  320.0f, -320.0f,
	1.0f, 0.0f,  320.0f,  320.0f, -320.0f,
	0.0f, 1.0f,  320.0f, -320.0f, -320.0f,
	1.0f, 1.0f, -320.0f, -320.0f, -320.0f,
	0.0f, 0.0f,  320.0f, -320.0f,  320.0f,
	1.0f, 0.0f, -320.0f, -320.0f,  320.0f,
	0.0f, 1.0f,  320.0f, -320.0f,  320.0f,
	1.0f, 1.0f, -320.0f, -320.0f,  320.0f,
	0.0f, 0.0f,  320.0f,  320.0f,  320.0f,
	1.0f, 0.0f, -320.0f,  320.0f,  320.0f,
	0.0f, 1.0f,  320.0f, -320.0f, -320.0f,
	1.0f, 1.0f,  320.0f, -320.0f,  320.0f,
	0.0f, 0.0f,  320.0f,  320.0f, -320.0f,
	1.0f, 0.0f,  320.0f,  320.0f,  320.0f,
	0.0f, 1.0f, -320.0f, -320.0f,  320.0f,
	1.0f, 1.0f, -320.0f, -320.0f, -320.0f,
	0.0f, 0.0f, -320.0f,  320.0f,  320.0f,
	1.0f, 0.0f, -320.0f,  320.0f, -320.0f,
	0.0f, 1.0f,  320.0f,  320.0f,  320.0f,
	1.0f, 1.0f, -320.0f,  320.0f,  320.0f,
	0.0f, 0.0f,  320.0f,  320.0f, -320.0f,
	1.0f, 0.0f, -320.0f,  320.0f, -320.0f
};

Renderer::Renderer(OSystem *system) :
		_system(system) {

	// Compute the cube faces Axis Aligned Bounding Boxes
	for (uint i = 0; i < ARRAYSIZE(_cubeFacesAABB); i++) {
		for (uint j = 0; j < 4; j++) {
			_cubeFacesAABB[i].expand(Math::Vector3d(cubeVertices[5 * (4 * i + j) + 2], cubeVertices[5 * (4 * i + j) + 3], cubeVertices[5 * (4 * i + j) + 4]));
		}
	}
}

Renderer::~Renderer() {
}

Texture *Renderer::copyScreenshotToTexture(const Common::Rect &screenViewport) {
	Graphics::Surface *surface = getScreenshot(screenViewport);

	Texture *texture = createTexture(*surface);

	surface->free();
	delete surface;

	return texture;
}

Math::Matrix4 Renderer::makeProjectionMatrix(float fov) const {
	static const float nearClipPlane = 1.0;
	static const float farClipPlane = 10000.0;

	float aspectRatio = kOriginalWidth / (float) kFrameHeight;

	float xmaxValue = nearClipPlane * tan(fov * M_PI / 360.0);
	float ymaxValue = xmaxValue / aspectRatio;

	return Math::makeFrustumMatrix(-xmaxValue, xmaxValue, -ymaxValue, ymaxValue, nearClipPlane, farClipPlane);
}

void Renderer::setupCameraPerspective(float pitch, float heading, float fov) {
	_projectionMatrix = makeProjectionMatrix(fov);
	_modelViewMatrix = Math::Matrix4(180.0f - heading, pitch, 0.0f, Math::EO_YXZ);

	Math::Matrix4 proj = _projectionMatrix;
	Math::Matrix4 model = _modelViewMatrix;
	proj.transpose();
	model.transpose();

	_mvpMatrix = proj * model;

	_frustum.setup(_mvpMatrix);

	_mvpMatrix.transpose();
}

bool Renderer::isCubeFaceVisible(uint face) {
	assert(face < 6);

	return _frustum.isInside(_cubeFacesAABB[face]);
}

void Renderer::flipVertical(Graphics::Surface *s) {
	for (int y = 0; y < s->h / 2; ++y) {
		// Flip the lines
		byte *line1P = (byte *)s->getBasePtr(0, y);
		byte *line2P = (byte *)s->getBasePtr(0, s->h - y - 1);

		for (int x = 0; x < s->pitch; ++x)
			SWAP(line1P[x], line2P[x]);
	}
}

void Renderer::toggleFullscreen() {
	if (!_system->hasFeature(OSystem::kFeatureFullscreenToggleKeepsContext)) {
		warning("Unable to toggle the fullscreen state because the current backend would destroy the graphics context");
		return;
	}

	bool oldFullscreen = _system->getFeatureState(OSystem::kFeatureFullscreenMode);
	_system->setFeatureState(OSystem::kFeatureFullscreenMode, !oldFullscreen);
}

NodeRenderer *Renderer::createNodeRenderer(Node &node, Layout &layout, GameState &state, ResourceLoader &resourceLoader) {
	return new NodeSoftwareRenderer(node, layout, *this, state, resourceLoader);
}

Drawable::Drawable() {
}

FrameLimiter::FrameLimiter(OSystem *system, const uint framerate) :
	_system(system),
	_speedLimitMs(0),
	_startFrameTime(0) {
	// The frame limiter is disabled when vsync is enabled.
	_enabled = !_system->getFeatureState(OSystem::kFeatureVSync) && framerate != 0;

	if (_enabled) {
		_speedLimitMs = 1000 / CLIP<uint>(framerate, 0, 100);
	}
}

void FrameLimiter::startFrame() {
	_startFrameTime = _system->getMillis();
}

void FrameLimiter::delayBeforeSwap() {
	uint endFrameTime = _system->getMillis();
	uint frameDuration = endFrameTime - _startFrameTime;

	if (_enabled && frameDuration < _speedLimitMs) {
		_system->delayMillis(_speedLimitMs - frameDuration);
	}
}

const Graphics::PixelFormat Texture::getRGBAPixelFormat() {
#ifdef SCUMM_BIG_ENDIAN
	return Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0);
#else
	return Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 8, 16, 24);
#endif
}

Texture *Renderer::createTexture(const Image::DDS &dds) {
	switch (dds.dataFormat()) {
	case Image::DDS::kDataFormatMipMaps:
		return createTexture(dds.getMipMaps()[0]);
	default:
		error("Unhandled DDS dataformat: %d when decoding %s", dds.dataFormat(), dds.name().c_str());
	}
}

Layout::Layout(OSystem &system, bool widescreenMod) :
		_system(system),
		_widescreenMod(widescreenMod) {
}

FloatRect Layout::menuViewport() const {
	return sceneViewport(
	            FloatSize(Renderer::kOriginalWidth, Renderer::kOriginalHeight),
	            .5f
	);
}

FloatRect Layout::frameViewport() const {
	return sceneViewport(
	            FloatSize(Renderer::kOriginalWidth, Renderer::kFrameHeight),
	            Renderer::kTopBorderHeight / (float)(Renderer::kTopBorderHeight + Renderer::kBottomBorderHeight)
	);
}

FloatRect Layout::screenViewport() const {
	FloatSize screenSize(g_system->getWidth(), g_system->getHeight());

	if (_widescreenMod) {
		return FloatRect(screenSize);
	}

	return FloatSize(Renderer::kOriginalWidth, Renderer::kOriginalHeight)
	        .fitIn(screenSize)
	        .centerIn(FloatRect(screenSize));
}

Common::Rect Layout::screenViewportInt() const {
	FloatRect screenViewPort = screenViewport();
	return Common::Rect(screenViewPort.left(), screenViewPort.top(), screenViewPort.right(), screenViewPort.bottom());
}

FloatRect Layout::unconstrainedViewport() const {
	FloatSize screenSize(g_system->getWidth(), g_system->getHeight());
	return FloatRect(screenSize);
}

FloatRect Layout::bottomBorderViewport() const {
	FloatRect screenRect = screenViewport();
	FloatRect frameRect  = frameViewport();

	if (_widescreenMod) {
		float height = Renderer::kBottomBorderHeight * scale();
		float bottom = CLIP<float>(frameRect.bottom() + height, 0, screenRect.bottom());

		return FloatRect(frameRect.left(), bottom - height, frameRect.right(), bottom);
	}

	return FloatRect(screenRect.left(), frameRect.bottom(), screenRect.right(), screenRect.bottom());
}

float Layout::scale() const {
	FloatRect screenRect = screenViewport();

	return MIN(
			screenRect.width()  / (float) Renderer::kOriginalWidth,
			screenRect.height() / (float) Renderer::kOriginalHeight
	);
}

FloatRect Layout::sceneViewport(FloatSize viewportSize, float verticalPositionRatio) const {
	FloatRect screenRect = screenViewport();

	return viewportSize
	        .fitIn(screenRect.size())
	        .positionIn(screenRect, .5f, verticalPositionRatio);
}

TextRenderer::TextRenderer(Renderer &gfx, ResourceLoader &resourceLoader) :
		_gfx(gfx),
		_fontTexture(nullptr) {

	ResourceDescription fontDesc = resourceLoader.getRawData("GLOB", 1206);
	if (!fontDesc.isValid())
		error("The font texture, GLOB-1206 was not found");

	TextureLoader textureLoader(_gfx);
	_fontTexture = textureLoader.load(fontDesc, TextureLoader::kImageFormatTEX);
}

TextRenderer::~TextRenderer() {
	delete _fontTexture;
}

void TextRenderer::draw2DText(const Common::String &text, const Common::Point &position) {
	// The font only has uppercase letters
	Common::String textToDraw = text;
	textToDraw.toUppercase();

	for (uint i = 0; i < textToDraw.size(); i++) {
		FloatRect screenRect = FloatSize(kCharacterWidth, kCharacterHeight)
		        .translate(FloatPoint(position.x + kCharacterAdvance * i, position.y))
		        .normalize(FloatSize(Renderer::kOriginalWidth, Renderer::kOriginalHeight));

		FloatRect textureRect = getFontCharacterRect(textToDraw[i]);

		_gfx.drawTexturedRect2D(screenRect, textureRect, *_fontTexture, 0.99f);
	}
}

FloatRect TextRenderer::getFontCharacterRect(uint8 character) const {
	uint index = 0;

	if (character == ' ')
		index = 0;
	else if (character >= '0' && character <= '9')
		index = 1 + character - '0';
	else if (character >= 'A' && character <= 'Z')
		index = 1 + 10 + character - 'A';
	else if (character == '|')
		index = 1 + 10 + 26;
	else if (character == '/')
		index = 2 + 10 + 26;
	else if (character == ':')
		index = 3 + 10 + 26;

	return FloatRect(kCharacterWidth * index, kCharacterHeight, kCharacterWidth * (index + 1), 0)
	        .normalize(FloatSize(1024.f, 32.f));
}

} // End of namespace Myst3
