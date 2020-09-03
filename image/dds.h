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

#ifndef IMAGE_DDS_H
#define IMAGE_DDS_H

#include "common/array.h"
#include "common/stream.h"
#include "graphics/surface.h"

namespace Image {

// Based on xoreos' DDS code

/**
 * DDS texture
 *
 * Only a very small subset of DDS features are supported. Especially,
 * compressed formats are not supported. This class is meant to
 * load a single DDS file per instance.
 */
class DDS {
public:
	DDS();
	~DDS();

	enum DataFormat {
		kDataFormatInvalid,
		kDataFormatMipMaps,
		kDataFormatRawBC1Unorm,
		kDataFormatRawBC2Unorm,
		kDataFormatRawBC3Unorm,
		kDataFormatRawBC7Unorm
	};

	typedef Common::Array<Graphics::Surface> MipMaps;

	/** Load a DDS texture from a stream */
	bool load(Common::SeekableReadStream &dds, const Common::String &name);

	uint32 width() const  { return _width; }
	uint32 height() const { return _height; }
	DataFormat dataFormat() const { return _dataFormat; }
	const Common::String &name() const { return _name; }

	/**
	 * Retrieve the mip map levels for a loaded texture
	 *
	 * The first mipmap is the full size image. Each further
	 * mipmap divides by two the with and the height of the
	 * previous one.
	 */
	const MipMaps &getMipMaps() const;
	const byte *rawData() const;
	uint rawDataSize() const;

private:
	/** The specific pixel format of the included image data. */
	struct DDSPixelFormat {
		/** The size of the image data in bytes */
		uint32 size;

		/** Features of the image data */
		uint32 flags;

		/** The FourCC to detect the format by */
		uint32 fourCC;

		/** Number of bits per pixel */
		uint32 bitCount;

		/** Bit mask for the red color component */
		uint32 rBitMask;

		/** Bit mask for the green color component */
		uint32 gBitMask;

		/** Bit mask for the blue color component */
		uint32 bBitMask;

		/** Bit mask for the alpha component */
		uint32 aBitMask;
	};

	enum DXGIFormat {
		kDXGIFormatUnknown  = 0x00,
		kDXGIFormatBC1Unorm = 0x47,
		kDXGIFormatBC2Unorm = 0x4a,
		kDXGIFormatBC3Unorm = 0x4d,
		kDXGIFormatBC7Unorm = 0x62
	};

	enum DDSResourceDimension {
		kDDSDimensionTexture1D = 2,
		kDDSDimensionTexture2D = 3,
		kDDSDimensionTexture3D = 4
	};

	struct DDSHeaderDXT10 {
		DXGIFormat dxgiFormat;
		DDSResourceDimension resourceDimension;
		uint32 miscFlag;
		uint32 arraySize;
		uint32 miscFlags2;

		DDSHeaderDXT10() : dxgiFormat(kDXGIFormatUnknown), resourceDimension(kDDSDimensionTexture2D), miscFlag(0), arraySize(1), miscFlags2(0) {}
	};

	bool readHeader(Common::SeekableReadStream &dds);
	bool readMipMaps(Common::SeekableReadStream &dds);
	bool readRaw(Common::SeekableReadStream &dds);

	bool detectFormat(const DDSPixelFormat &format, const DDSHeaderDXT10 &dxt10Header);

	uint32 _width;
	uint32 _height;
	uint32 _mipMapCount;

	DataFormat _dataFormat;

	byte *_rawData;
	uint _rawDataSize;

	MipMaps _mipmaps;
	Graphics::PixelFormat _format;
	Common::String _name;
};

} // End of namespace Image

#endif // IMAGE_DDS_H
