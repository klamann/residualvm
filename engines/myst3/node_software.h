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

#ifndef MYST3_NODE_SOFTWARE_H
#define MYST3_NODE_SOFTWARE_H

#include "myst3/gfx.h"
#include "myst3/node.h"

#include "graphics/surface.h"

namespace Myst3 {

class Face;
class Node;
class GameState;
class SpotItem;

class NodeSoftwareRenderer : public NodeRenderer {
public:
	NodeSoftwareRenderer(Node &node, Renderer &gfx, GameState &state, ResourceLoader &resourceLoader);
	~NodeSoftwareRenderer() override;

	void draw() override;

	void initSpotItem(SpotItem &spotItem) override;
	void initSpotItemMenu(SpotItem &spotItem, const Common::Rect &rect) override;
	void updateSpotItemBitmap(uint16 spotItemId, const Graphics::Surface &surface) override;
	void clearSpotItemBitmap(uint16 spotItemId) override;
	void update() override;

private:
	struct Face {
		Graphics::Surface bitmap;
		Graphics::Surface finalBitmap;

		Texture *texture;
		bool textureDirty;
		Common::Rect textureDirtyRect;

		Face() : texture(nullptr), textureDirty(false) {}
	};

	struct SpotItemImage {
		uint16 spotItemId;
		uint faceId;

		uint16 posX;
		uint16 posY;

		bool bitmapDirty;
		Graphics::Surface drawBitmap;
		Graphics::Surface undrawBitmap;

		SpotItemImage() : spotItemId(0), faceId(0), posX(0), posY(0), bitmapDirty(false) {}

		Common::Rect getFaceRect() const {
			Common::Rect r(drawBitmap.w, drawBitmap.h);
			r.translate(posX, posY);
			return r;
		}
	};

	typedef Common::Array<SpotItemImage> SpotItemImageArray;

	void drawFrame(bool menu);
	void drawCube();

	void drawSpotItemImage(SpotItemImage &spotItemImage, Face &face);
	void undrawSpotItemImage(SpotItemImage &spotItemImage, Face &face);
	void fadeDrawSpotItemImage(SpotItemImage &spotItemImage, Face &face, uint16 fadeValue);

	void initUndrawSpotItem(SpotItem &spotItem);
	void drawSpotItem(SpotItem &spotItem);
	void undrawSpotItem(SpotItem &spotItem);
	void fadeDrawSpotItem(SpotItem &spotItem, uint16 fadeValue);
	bool hasDirtyBitmap(SpotItem &spotItem) const;

	bool isFaceVisible(uint faceId);

	void addFaceTextureDirtyRect(Face &face, const Common::Rect &rect);
	void uploadFaceTexture(Face &face);

	Node &_node;
	Renderer &_gfx;
	GameState &_state;
	ResourceLoader &_resourceLoader;

	Common::Array<Face> _faces;
	SpotItemImageArray _spotItemImages;
};

} // end of namespace Myst3

#endif
