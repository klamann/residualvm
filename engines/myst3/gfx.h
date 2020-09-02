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

#ifndef GFX_H_
#define GFX_H_

#include "common/rect.h"
#include "common/system.h"

#include "math/frustum.h"
#include "math/matrix4.h"
#include "math/vector3d.h"

#include "myst3/rect.h"

namespace Image {
class DDS;
}

namespace Myst3 {

class GameState;
class Layout;
class Node;
class NodeRenderer;
class ResourceLoader;

class Layout {
public:
	Layout(OSystem &system, bool widescreenMod);

	FloatRect menuViewport() const;
	FloatRect frameViewport() const;
	FloatRect screenViewport() const;
	Common::Rect screenViewportInt() const;
	FloatRect unconstrainedViewport() const;
	FloatRect bottomBorderViewport() const;
	float scale() const;

private:
	FloatRect sceneViewport(FloatSize viewportSize, float verticalPositionRatio) const;

	OSystem &_system;
	bool _widescreenMod;
};

class Drawable {
public:
	Drawable();
	virtual ~Drawable() {}

	virtual void draw() {}
	virtual void drawOverlay() {}
};

class Texture {
public:
	uint width;
	uint height;
	Graphics::PixelFormat format;

	virtual ~Texture() {}

	FloatSize size() const { return FloatSize(width, height); }

	virtual void update(const Graphics::Surface &surface) = 0;
	virtual void updatePartial(const Graphics::Surface &surface, const Common::Rect &rect) = 0;

	static const Graphics::PixelFormat getRGBAPixelFormat();
protected:
	Texture() {}
};

class Renderer {
public:
	Renderer(OSystem *system);
	virtual ~Renderer();

	virtual void init() {}
	virtual void setViewport(const FloatRect &viewport, bool is3d) = 0;
	virtual void clear() = 0;
	void toggleFullscreen();

	virtual bool supportsCompressedTextures() const { return false; }

	/**
	 *  Swap the buffers, making the drawn screen visible
	 */
	virtual void flipBuffer() { }

	virtual Texture *createTexture(const Graphics::Surface &surface) = 0;
	virtual Texture *createTexture(const Image::DDS &dds);

	virtual NodeRenderer *createNodeRenderer(Node &node, Layout &layout, GameState &state, ResourceLoader &resourceLoader);

	virtual void drawRect2D(const FloatRect &screenRect, uint32 color) = 0;

	virtual void drawTexturedRect2D(const FloatRect &screenRect, const FloatRect &textureRect, Texture &texture,
									float transparency = -1.0, bool additiveBlending = false) = 0;

	virtual void drawTexturedRect3D(const Math::Vector3d &topLeft, const Math::Vector3d &bottomLeft,
									const Math::Vector3d &topRight, const Math::Vector3d &bottomRight,
									Texture &texture) = 0;

	virtual void drawCube(Texture **textures) = 0;

	virtual Graphics::Surface *getScreenshot(const Common::Rect &screenViewport) = 0;
	virtual Texture *copyScreenshotToTexture(const Common::Rect &screenViewport);

	void setupCameraPerspective(float pitch, float heading, float fov);

	bool isCubeFaceVisible(uint face);

	Math::Matrix4 getMvpMatrix() const { return _mvpMatrix; }

	static void flipVertical(Graphics::Surface *s);

	static const int kOriginalWidth = 640;
	static const int kOriginalHeight = 480;
	static const int kTopBorderHeight = 30;
	static const int kBottomBorderHeight = 90;
	static const int kFrameHeight = 360;

protected:
	OSystem *_system;

	Math::Matrix4 _projectionMatrix;
	Math::Matrix4 _modelViewMatrix;
	Math::Matrix4 _mvpMatrix;

	Math::Frustum _frustum;

	static const float cubeVertices[5 * 6 * 4];
	Math::AABB _cubeFacesAABB[6];

	Math::Matrix4 makeProjectionMatrix(float fov) const;
};

class TextRenderer {
public:
	TextRenderer(Renderer &gfx, ResourceLoader &resourceLoader);
	~TextRenderer();

	void draw2DText(const Common::String &text, const Common::Point &position);

private:
	FloatRect getFontCharacterRect(uint8 character) const;

	static const uint kCharacterWidth    = 16;
	static const uint kCharacterAdvance  = 13;
	static const uint kCharacterHeight   = 32;

	Renderer &_gfx;
	Texture *_fontTexture;
};

/**
 * A framerate limiter
 *
 * Ensures the framerate does not exceed the specified value
 * by delaying until all of the timeslot allocated to the frame
 * is consumed.
 * Allows to curb CPU usage and have a stable framerate.
 */
class FrameLimiter {
public:
	FrameLimiter(OSystem *system, const uint framerate);

	void startFrame();
	void delayBeforeSwap();
private:
	OSystem *_system;

	bool _enabled;
	uint _speedLimitMs;
	uint _startFrameTime;
};

} // End of namespace Myst3

#endif // GFX_H_
