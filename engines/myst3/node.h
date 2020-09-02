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

#ifndef MYST3_NODE_H
#define MYST3_NODE_H

#include "myst3/gfx.h"

#include "common/array.h"
#include "common/rect.h"

#include "graphics/surface.h"

namespace Myst3 {

class Myst3Engine;
class ResourceLoader;
class Effect;

class SpotItem {
public:
	SpotItem(uint16 id, int16 condition, bool fade, uint16 fadeVariable);

	uint16 id() const { return _id; }
	int16 condition() const { return _condition; }
	bool shouldFade() const { return _enableFade; }
	uint16 fadeVariable() const { return _fadeVariable; }

	bool drawn() const { return _drawn; }
	void setDrawn(bool drawn) { _drawn = drawn; }

	uint16 fadeValue() const { return _fadeValue; }
	void setFadeValue(uint16 fadeValue) { _fadeValue = fadeValue; }

private:
	uint16 _id;
	int16 _condition;
	bool _enableFade;
	uint16 _fadeVariable;

	bool _drawn;
	uint16 _fadeValue;
};

class SunSpot {
public:
	uint16 pitch;
	uint16 heading;
	float intensity;
	uint32 color;
	uint16 var;
	bool variableIntensity;
	float radius;
};

typedef Common::Array<SpotItem> SpotItemArray;
typedef Common::Array<Effect *> EffectArray;

class Node {
public:
	enum Type {
		kFrame,
		kMenu,
		kCube
	};

	Node(const Common::String &room, uint16 id, Type type);
	~Node();

	const Common::String &room() const { return _room; }
	uint16 id() const { return _id; }
	Type type() const { return _type; }
	SpotItemArray &spotItems() { return _spotItems; }
	EffectArray &effects() { return _effects; }

	void addEffect(Effect *effect);
	void addSpotItem(const SpotItem &spotItem);

private:
	Common::String _room;
	uint16 _id;
	Type _type;
	SpotItemArray _spotItems;
	EffectArray _effects;
};

class NodeRenderer {
public:
	virtual void draw() = 0;
	virtual void initSpotItem(SpotItem &spotItem) = 0;
	virtual void initSpotItemMenu(SpotItem &spotItem, const Common::Rect &faceRect) = 0;
	virtual void updateSpotItemBitmap(uint16 spotItemId, const Graphics::Surface &surface) = 0;
	virtual void updateSpotItemTexture(uint16 spotItemId, Texture *texture, const FloatRect &textureRect);
	virtual void clearSpotItemBitmap(uint16 spotItemId) = 0;
	virtual void initEffects() {}
	virtual void update() = 0;
	virtual ~NodeRenderer() {}
};

} // end of namespace Myst3

#endif
