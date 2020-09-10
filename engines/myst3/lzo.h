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

#ifndef MYST3_LZO_H
#define MYST3_LZO_H

#include "common/scummsys.h"

namespace Myst3 {

enum class LzoResult {
	LookbehindOverrun = -4,
	OutputOverrun = -3,
	InputOverrun = -2,
	Error = -1,
	Success = 0,
	InputNotConsumed = 1,
};

LzoResult lzoDecompress(const uint8 *src, size_t srcSize,
                        uint8 *dst, size_t dstSize,
                        size_t &outSize);
LzoResult lzoCompress(const uint8 *src, size_t srcSize,
                      uint8 *dst, size_t dstSize,
                      size_t &outSize);

inline size_t lzoCompressWorstSize(size_t s) {
	return s + s / 16 + 64 + 3;
}

} // namespace Myst3

#endif
