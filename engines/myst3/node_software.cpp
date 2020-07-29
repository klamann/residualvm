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

#include "engines/myst3/node_software.h"

#include "engines/myst3/effects.h"
#include "engines/myst3/myst3.h"
#include "engines/myst3/node.h"
#include "engines/myst3/resource_loader.h"
#include "engines/myst3/state.h"
#include "engines/myst3/subtitles.h"

namespace Myst3 {

NodeSoftwareRenderer::NodeSoftwareRenderer(Node &node, Layout &layout, Renderer &gfx, GameState &state, ResourceLoader &resourceLoader) :
		_node(node),
		_layout(layout),
		_gfx(gfx),
		_state(state),
		_resourceLoader(resourceLoader) {
	_faces.resize(_node.type() == Node::kCube ? 6 : 1);
	for (uint faceId = 0; faceId < _faces.size(); faceId++) {
		Face &face = _faces[faceId];

		Graphics::Surface bitmap;
		if (_node.type() == Node::kCube)  {
			ResourceDescription resource = resourceLoader.getCubeBitmap(_node.room(), _node.id(), faceId);
			bitmap = Myst3Engine::decodeJpeg(resource);
		} else {
			ResourceDescription resource = resourceLoader.getFrameBitmap(_node.room(), _node.id());
			bitmap = Myst3Engine::decodeJpeg(resource);
		}

		face.bitmap  = bitmap;
		face.texture = _gfx.createTexture(face.bitmap);

		addFaceTextureDirtyRect(face, Common::Rect(face.bitmap.w, face.bitmap.h));
	}
}

NodeSoftwareRenderer::~NodeSoftwareRenderer() {
	for (uint i = 0; i < _faces.size(); i++) {
		delete _faces[i].texture;
		_faces[i].bitmap.free();
		_faces[i].finalBitmap.free();
	}

	for (uint i = 0; i < _spotItemImages.size(); i++) {
		_spotItemImages[i].drawBitmap.free();
		_spotItemImages[i].undrawBitmap.free();
	}
}

void NodeSoftwareRenderer::drawSpotItemImage(SpotItemImage &spotItemImage, Face &face) {
	Common::Rect faceRect = spotItemImage.getFaceRect();

	Graphics::Surface &faceBitmap = face.bitmap;
	faceBitmap.copyRectToSurface(
	            spotItemImage.drawBitmap,
	            faceRect.left,
	            faceRect.top,
	            Common::Rect(faceRect.width(), faceRect.height())
	);

	addFaceTextureDirtyRect(face, faceRect);
	spotItemImage.bitmapDirty = false;
}

void NodeSoftwareRenderer::undrawSpotItemImage(SpotItemImage &spotItemImage, Face &face) {
	Common::Rect faceRect = spotItemImage.getFaceRect();

	Graphics::Surface &faceBitmap = face.bitmap;
	faceBitmap.copyRectToSurface(
	            spotItemImage.undrawBitmap,
	            faceRect.left,
	            faceRect.top,
	            Common::Rect(faceRect.width(), faceRect.height())
	);

	addFaceTextureDirtyRect(face, faceRect);
}

void NodeSoftwareRenderer::fadeDrawSpotItemImage(SpotItemImage &spotItemImage, Face &face, uint16 fadeValue) {
	Common::Rect faceRect = spotItemImage.getFaceRect();

	const Graphics::Surface &spotItemDrawBitmap = spotItemImage.drawBitmap;
	const Graphics::Surface &spotItemUndrawBitmap = spotItemImage.undrawBitmap;
	Graphics::Surface faceBitmap = face.bitmap.getSubArea(faceRect);

	for (int i = 0; i < spotItemDrawBitmap.h; i++) {
		const byte *ptrND = (const byte *)spotItemUndrawBitmap.getBasePtr(0, i);
		const byte *ptrD = (const byte *)spotItemDrawBitmap.getBasePtr(0, i);
		byte *ptrDest = (byte *)faceBitmap.getBasePtr(0, i);

		for (int j = 0; j < spotItemDrawBitmap.w; j++) {
			byte rND = *ptrND++;
			byte gND = *ptrND++;
			byte bND = *ptrND++;
			ptrND++; // Alpha
			byte rD = *ptrD++;
			byte gD = *ptrD++;
			byte bD = *ptrD++;
			ptrD++; // Alpha

			// TODO: optimize ?
			*ptrDest++ = rND * (100 - fadeValue) / 100 + rD * fadeValue / 100;
			*ptrDest++ = gND * (100 - fadeValue) / 100 + gD * fadeValue / 100;
			*ptrDest++ = bND * (100 - fadeValue) / 100 + bD * fadeValue / 100;
			ptrDest++; // Alpha
		}
	}

	addFaceTextureDirtyRect(face, faceRect);
	spotItemImage.bitmapDirty = false;
}

void NodeSoftwareRenderer::initUndrawSpotItem(SpotItem &spotItem) {
	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage &spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId != spotItem.id()) continue;

		Face &face = _faces[spotItemImage.faceId];

		Graphics::Surface &faceBitmap = face.bitmap;
		Common::Rect faceRect = spotItemImage.getFaceRect();

		// Copy not drawn SpotItem image from face
		const Graphics::Surface undrawBitmap = faceBitmap.getSubArea(faceRect);
		spotItemImage.undrawBitmap.copyFrom(undrawBitmap);
	}
}

void NodeSoftwareRenderer::drawSpotItem(SpotItem &spotItem) {
	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage &spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId != spotItem.id()) continue;

		Face &face = _faces[spotItemImage.faceId];

		drawSpotItemImage(spotItemImage, face);
	}

	spotItem.setDrawn(true);
}

void NodeSoftwareRenderer::undrawSpotItem(SpotItem &spotItem) {
	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage &spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId != spotItem.id()) continue;

		Face &face = _faces[spotItemImage.faceId];

		undrawSpotItemImage(spotItemImage, face);
	}

	spotItem.setDrawn(false);
}

void NodeSoftwareRenderer::fadeDrawSpotItem(SpotItem &spotItem, uint16 fadeValue) {
	uint16 drawFadeValue = CLIP<uint16>(fadeValue, 0, 100);

	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage &spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId != spotItem.id()) continue;

		Face &face = _faces[spotItemImage.faceId];

		fadeDrawSpotItemImage(spotItemImage, face, drawFadeValue);
	}

	spotItem.setDrawn(true);
	spotItem.setFadeValue(fadeValue);
}

bool NodeSoftwareRenderer::hasDirtyBitmap(SpotItem &spotItem) const {
	for (uint i = 0; i < _spotItemImages.size(); i++) {
		if (_spotItemImages[i].spotItemId == spotItem.id()
		        && _spotItemImages[i].bitmapDirty) {
			return true;
		}
	}

	return false;
}

bool NodeSoftwareRenderer::isFaceVisible(uint faceId) {
	switch (_node.type()) {
	case Node::kFrame:
	case Node::kMenu:
		return true;
	case Node::kCube:
		return _gfx.isCubeFaceVisible(faceId);
	}

	return false;
}

void NodeSoftwareRenderer::addFaceTextureDirtyRect(Face &face, const Common::Rect &rect) {
	if (!face.textureDirty) {
		face.textureDirtyRect = rect;
	} else {
		face.textureDirtyRect.extend(rect);
	}

	face.textureDirty = true;
}

void NodeSoftwareRenderer::initSpotItem(SpotItem &spotItem) {
	ResourceDescriptionArray resources = _resourceLoader.listSpotItemImages(_node.room(), spotItem.id());

	for (uint i = 0; i < resources.size(); i++) {
		const ResourceDescription &resource = resources[i];

		ResourceDescription::SpotItemData spotItemData = resource.spotItemData();

		SpotItemImage spotItemImage;
		spotItemImage.spotItemId = spotItem.id();
		spotItemImage.faceId = resource.face() - 1;
		spotItemImage.posX = spotItemData.u;
		spotItemImage.posY = spotItemData.v;
		spotItemImage.drawBitmap = Myst3Engine::decodeJpeg(resource);
		spotItemImage.bitmapDirty = true;

		_spotItemImages.push_back(spotItemImage);
	}

	// SpotItems with an always true conditions cannot be undrawn.
	// Draw them now to make sure the "non drawn backups" for other, potentially
	// overlapping SpotItems have them drawn.
	if (spotItem.condition() == 1) {
		drawSpotItem(spotItem);
	} else {
		initUndrawSpotItem(spotItem);
	}
}

void NodeSoftwareRenderer::initSpotItemMenu(SpotItem &spotItem, const Common::Rect &rect) {
	Graphics::Surface black;
	black.create(rect.width(), rect.height(), Texture::getRGBAPixelFormat());

	SpotItemImage spotItemImage;
	spotItemImage.spotItemId = spotItem.id();
	spotItemImage.faceId = 0;
	spotItemImage.posX = rect.left;
	spotItemImage.posY = rect.top;
	spotItemImage.drawBitmap = black;
	spotItemImage.bitmapDirty = true;

	_spotItemImages.push_back(spotItemImage);

	initUndrawSpotItem(spotItem);
}

void NodeSoftwareRenderer::updateSpotItemBitmap(uint16 spotItemId, const Graphics::Surface &surface) {
	assert(surface.format == Texture::getRGBAPixelFormat());

	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId == spotItemId) {
			spotItemImage.drawBitmap.copyFrom(surface);
			spotItemImage.bitmapDirty = true;
			break;
		}
	}
}

void NodeSoftwareRenderer::clearSpotItemBitmap(uint16 spotItemId) {
	for (uint i = 0; i < _spotItemImages.size(); i++) {
		SpotItemImage spotItemImage = _spotItemImages[i];
		if (spotItemImage.spotItemId == spotItemId) {
			memset(spotItemImage.drawBitmap.getPixels(), 0, spotItemImage.drawBitmap.pitch * spotItemImage.drawBitmap.h);

			spotItemImage.bitmapDirty = true;
			break;
		}
	}
}

void NodeSoftwareRenderer::update() {
	// First undraw ...
	for (uint i = 0; i < _node.spotItems().size(); i++) {
		SpotItem &spotItem = _node.spotItems()[i];

		bool newDrawn = _state.evaluate(spotItem.condition());
		if (!newDrawn && spotItem.drawn()) {
			undrawSpotItem(spotItem);
		}
	}

	// ... then redraw
	for (uint i = 0; i < _node.spotItems().size(); i++) {
		SpotItem &spotItem = _node.spotItems()[i];

		bool newDrawn = _state.evaluate(spotItem.condition());
		if (spotItem.shouldFade()) {
			uint16 newFadeValue = _state.getVar(spotItem.fadeVariable());

			if (newDrawn && (spotItem.fadeValue() != newFadeValue || hasDirtyBitmap(spotItem))) {
				fadeDrawSpotItem(spotItem, newFadeValue);
			}
		} else {
			if (newDrawn && (!spotItem.drawn() || hasDirtyBitmap(spotItem))) {
				drawSpotItem(spotItem);
			}
		}
	}

	bool needsUpdate = false;
	for (uint i = 0; i < _node.effects().size(); i++) {
		needsUpdate |= _node.effects()[i]->update();
	}

	// Apply the effects for all the faces
	for (uint faceId = 0; faceId < _faces.size(); faceId++) {
		Face &face = _faces[faceId];

		if (!isFaceVisible(faceId)) {
			continue; // This face is not currently visible
		}

		uint effectsForFace = 0;
		for (uint i = 0; i < _node.effects().size(); i++) {
			if (_node.effects()[i]->hasFace(faceId))
				effectsForFace++;
		}

		if (effectsForFace == 0)
			continue;
		if (!needsUpdate && !face.textureDirty)
			continue;

		// Alloc the target surface if necessary
		face.finalBitmap.copyFrom(face.bitmap);

		if (effectsForFace == 1) {
			_node.effects()[0]->applyForFace(faceId, &face.bitmap, &face.finalBitmap);

			addFaceTextureDirtyRect(face, _node.effects()[0]->getUpdateRectForFace(faceId));
		} else if (effectsForFace == 2) {
			// TODO: Keep the same temp surface to avoid heap fragmentation ?
			Graphics::Surface tmp;
			tmp.copyFrom(face.bitmap);

			_node.effects()[0]->applyForFace(faceId, &face.bitmap, &tmp);
			_node.effects()[1]->applyForFace(faceId, &tmp, &face.finalBitmap);

			tmp.free();

			addFaceTextureDirtyRect(face, _node.effects()[0]->getUpdateRectForFace(faceId));
			addFaceTextureDirtyRect(face, _node.effects()[1]->getUpdateRectForFace(faceId));
		} else {
			error("Unable to render more than 2 effects per faceId (%d)", effectsForFace);
		}
	}
}

void NodeSoftwareRenderer::uploadFaceTexture(Face &face) {
	if (face.finalBitmap.getPixels())
		face.texture->updatePartial(face.finalBitmap, face.textureDirtyRect);
	else
		face.texture->updatePartial(face.bitmap, face.textureDirtyRect);

	face.textureDirty = false;
}

void NodeSoftwareRenderer::draw() {
	// Update the OpenGL textures if needed
	for (uint i = 0; i < _faces.size(); i++) {
		if (_faces[i].textureDirty && isFaceVisible(i)) {
			uploadFaceTexture(_faces[i]);
		}
	}

	switch (_node.type()) {
	case Node::kFrame:
		drawFrame(false);
		break;
	case Node::kMenu:
		drawFrame(true);
		break;
	case Node::kCube:
		drawCube();
		break;
	}
}

void NodeSoftwareRenderer::drawFrame(bool menu) {
	assert(!_faces.empty());

	Texture *texture = _faces[0].texture;

	FloatRect sceneViewport;
	if (menu) {
		sceneViewport = _layout.menuViewport();
	} else {
		sceneViewport = _layout.frameViewport();
	}

	_gfx.setViewport(sceneViewport, false);
	_gfx.drawTexturedRect2D(FloatRect::unit(), FloatRect::unit(), *texture);
}

void NodeSoftwareRenderer::drawCube() {
	assert(_faces.size() == 6);

	FloatRect sceneViewport = _layout.frameViewport();
	_gfx.setViewport(sceneViewport, true);

	Texture *textures[6];
	for (uint i = 0; i < _faces.size(); i++) {
		textures[i] = _faces[i].texture;
	}

	_gfx.drawCube(textures);
}

void Node::addSpotItem(const SpotItem &spotItem) {
	_spotItems.push_back(spotItem);
}

SpotItem::SpotItem(uint16 id, int16 condition, bool enableFade, uint16 fadeVariable) :
		_id(id),
		_condition(condition),
		_enableFade(enableFade),
		_fadeVariable(fadeVariable),
		_drawn(false),
		_fadeValue(0) {
}

} // end of namespace Myst3
