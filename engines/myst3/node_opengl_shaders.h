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

#ifndef MYST3_NODE_OPENGL_SHADERS_H
#define MYST3_NODE_OPENGL_SHADERS_H

#include "myst3/gfx.h"
#include "myst3/node.h"

#include "common/hashmap.h"
#include "common/hash-ptr.h"
#include "graphics/surface.h"

namespace OpenGL {
class Shader;
}

namespace Myst3 {

class GameState;
class Face;
class Node;
class OpenGLTexture;
class ShaderRenderer;
class SpotItem;

class NodeShaderRenderer : public NodeRenderer {
public:
	NodeShaderRenderer(Node &node, Layout &layout, Renderer &gfx, GameState &state, ResourceLoader &resourceLoader);
	~NodeShaderRenderer() override;

	void draw() override;
	void initSpotItem(SpotItem &spotItem) override;
	void initSpotItemMenu(SpotItem &spotItem, const Common::Rect &faceRect) override;
	void updateSpotItemBitmap(uint16 spotItemId, const Graphics::Surface &surface) override;
	void updateSpotItemTexture(uint16 spotItemId, Texture *texture, const FloatRect &textureRect) override;
	void clearSpotItemBitmap(uint16 spotItemId) override;
	void initEffects() override;
	void update() override;

private:
	struct Face {
		uint16 id;
		bool dirty;
		OpenGLTexture *baseTexture;
		uint fbo;
		OpenGLTexture *assembledTexture;

		Face() : id(0), dirty(true), baseTexture(nullptr), fbo(0), assembledTexture(nullptr) {}
	};

	struct SpotItemTexture {
		uint16 spotItemId;
		uint16 faceId;
		FloatRect faceRect;
		OpenGLTexture *texture;
		FloatRect textureRect;

		SpotItemTexture() : spotItemId(0), faceId(0), texture(nullptr), textureRect(0.f, 0.f, 1.f, 1.f) {}
	};

	struct EffectFace {
		Effect *effect;
		uint16 faceId;

		EffectFace(Effect *e, uint16 face) : effect(e), faceId(face) {}
	};

	struct EffectFace_Hash {
		Common::Hash<Effect *> effectHash;

		uint operator()(const EffectFace& x) const {
			uint hash = 7;
			hash = 31 * hash + effectHash(x.effect);
			hash = 31 * hash + x.faceId;
			return hash;
		}
	};

	struct EffectFace_EqualTo {
		bool operator()(const EffectFace& x, const EffectFace& y) const {
			return (x.effect == y.effect)
			    && (x.faceId == y.faceId);
		}
	};

	typedef Common::Array<SpotItemTexture> SpotItemTextureArray;
	typedef Common::HashMap<EffectFace, OpenGLTexture *, EffectFace_Hash, EffectFace_EqualTo> EffectMaskTextureMap;

	void drawFrame(bool menu);
	void drawCube();

	void setupEffectsShader(OpenGL::Shader &shader, uint faceId, const EffectArray &effects);

	void drawSpotItemTexture(SpotItemTexture &spotItemTexture, float transparency);

	void drawFace(Face &face);
	void drawSpotItem(SpotItem &spotItem, Face &face, float transparency);

	bool areSpotItemsDirty();
	bool isFaceVisible(uint faceId);

	Node &_node;
	Layout &_layout;
	ShaderRenderer &_gfx;
	GameState &_state;
	ResourceLoader &_resourceLoader;

	OpenGL::Shader *_effectsCubeShader;
	OpenGL::Shader *_effectsFrameShader;
	Common::Array<Face> _faces;
	SpotItemTextureArray _spotItemTextures;
	EffectMaskTextureMap _effectMaskTextures;
	OpenGLTexture *_shieldEffectPattern;
};

} // end of namespace Myst3

#endif
