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

#ifndef MYST3_RECT_H
#define MYST3_RECT_H

#include "common/scummsys.h"
#include "common/util.h"

namespace Myst3 {

struct FloatRect;

class FloatPoint {
public:
	FloatPoint() :
			_x(0), _y(0) {}

	FloatPoint(float x, float y) :
			_x(x), _y(y) {}

	float x() const { return _x; }
	float y() const { return _y; }

private:
	float _x;
	float _y;
};

struct FloatSize {
public:
	FloatSize() : _width(0.f), _height(0.f) {}
	FloatSize(float width, float height) : _width(width), _height(height) {}

	static FloatSize unit() {
		return FloatSize(1.f, 1.f);
	}

	float width() const { return _width; }
	float height() const { return _height; }

	bool isEmpty() const { return _width == 0.f && _height == 0.f; }

	bool operator==(const FloatSize &s) const { return _width == s._width && _height == s._height; }

	FloatRect translate(const FloatPoint &position) const;
	FloatRect centerIn(const FloatRect &containingRect) const;
	FloatRect positionIn(const FloatRect &containingRect, float xRatio, float yRatio) const;

	FloatSize scale(float scale) const {
		return FloatSize(width() * scale, height() * scale);
	}

	FloatSize fitIn(const FloatSize &containing) const {
		float aspectRatio = width() / height();

		return FloatSize(
		            MIN<float>(containing.width(),  containing.height() * aspectRatio),
		            MIN<float>(containing.height(), containing.width()  / aspectRatio)
		);
	}
private:
	float _width;
	float _height;
};

struct FloatRect {
public:
	FloatRect() : _left(0), _top(0), _right(0), _bottom(0) {}
	FloatRect(float left, float top, float right, float bottom) : _left(left), _top(top), _right(right), _bottom(bottom) {}
	FloatRect(float left, float top, FloatSize size) : _left(left), _top(top), _right(left + size.width()), _bottom(top + size.height()) {}
	FloatRect(FloatPoint position, FloatSize size) : _left(position.x()), _top(position.y()), _right(position.x() + size.width()), _bottom(position.y() + size.height()) {}
	explicit FloatRect(FloatSize size) : _left(0), _top(0), _right(size.width()), _bottom(size.height()) {}

	static FloatRect unit() {
		return FloatRect(FloatSize::unit());
	}

	static FloatRect center(FloatPoint center, FloatSize size) {
		float x = center.x() - size.width()  / 2;
		float y = center.y() - size.height() / 2;
		return FloatRect(x, y, size);
	}

	float left() const { return _left; }
	float top() const { return _top; }
	float right() const { return _right; }
	float bottom() const { return _bottom; }

	float width() const { return _right - _left; }
	float height() const { return _bottom - _top; }
	FloatSize size() const { return FloatSize(width(), height()); }
	FloatPoint center() const {
		return FloatPoint(
		            (left() + right())  / 2,
		            (top()  + bottom()) / 2
		);
	}

	FloatRect clip(const FloatSize size) const {
		return FloatRect(
		            CLIP<float>(_left,   0.f, size.width()),
		            CLIP<float>(_top,    0.f, size.height()),
		            CLIP<float>(_right,  0.f, size.width()),
		            CLIP<float>(_bottom, 0.f, size.height())
		);
	}

	bool contains(const FloatPoint point) const {
		return _left <= point.x() && point.x() < _right
		    && _top  <= point.y() && point.y() < _bottom;
	}

	FloatRect scale(float scale) const {
		return FloatRect(
		            left()   * scale,
		            top()    * scale,
		            right()  * scale,
		            bottom() * scale
		);
	}

	FloatRect translate(const FloatPoint point) const {
		return FloatRect(
		            left()   + point.x(),
		            top()    + point.y(),
		            right()  + point.x(),
		            bottom() + point.y()
		);
	}

	FloatRect normalize(const FloatSize containing) const {
		return FloatRect(
		            left()   / containing.width(),
		            top()    / containing.height(),
		            right()  / containing.width(),
		            bottom() / containing.height()
		);
	}

private:
	float _left;
	float _top;
	float _right;
	float _bottom;
};

inline FloatRect FloatSize::translate(const FloatPoint &position) const {
	return FloatRect(position, *this);
}

inline FloatRect FloatSize::centerIn(const FloatRect &containingRect) const {
	return positionIn(containingRect, .5f, .5f);
}

inline FloatRect FloatSize::positionIn(const FloatRect &containingRect, float xRatio, float yRatio) const {
	return FloatRect(
	            containingRect.left() + (containingRect.width()  - width())  * xRatio,
	            containingRect.top()  + (containingRect.height() - height()) * yRatio,
	            *this
	);
}

} // End of namespace Myst3

#endif // MYST3_RECT_H
