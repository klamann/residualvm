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

#include "image/dds.h"

#include "common/textconsole.h"

namespace Image {

// Based on xoreos' DDS code

static const uint32 kDDSID        = MKTAG('D', 'D', 'S', ' ');
static const uint32 kFOURCC_DX10  = MKTAG('D', 'X', '1', '0');
static const uint32 kFOURCC_DXT1  = MKTAG('D', 'X', 'T', '1');
static const uint32 kFOURCC_DXT3  = MKTAG('D', 'X', 'T', '3');
static const uint32 kFOURCC_DXT5  = MKTAG('D', 'X', 'T', '5');

static const uint32 kHeaderFlagsHasMipMaps = 0x00020000;

static const uint32 kPixelFlagsHasAlpha  = 0x00000001;
static const uint32 kPixelFlagsHasFourCC = 0x00000004;
static const uint32 kPixelFlagsIsIndexed = 0x00000020;
static const uint32 kPixelFlagsIsRGB     = 0x00000040;

static const uint32 kMiscFlagsTextureCube = 0x00000004;

DDS::DDS() :
		_width(0),
		_height(0),
		_mipMapCount(0),
		_dataFormat(kDataFormatInvalid),
		_rawData(nullptr),
		_rawDataSize(0) {
}

DDS::~DDS() {
	for (uint i = 0; i < _mipmaps.size(); i++) {
		_mipmaps[i].free();
	}
	free(_rawData);
}

bool DDS::load(Common::SeekableReadStream &dds, const Common::String &name) {
	assert(_mipmaps.empty());
	assert(!_rawData);

	_name = name;

	if (!readHeader(dds)) {
		return false;
	}

	switch (_dataFormat) {
	case kDataFormatMipMaps:
		return readMipMaps(dds);
	case kDataFormatRawBC1Unorm:
	case kDataFormatRawBC2Unorm:
	case kDataFormatRawBC3Unorm:
	case kDataFormatRawBC7Unorm:
		return readRaw(dds);
	case kDataFormatInvalid:
	default:
		error("Invalid data format");
	}
}

const DDS::MipMaps &DDS::getMipMaps() const {
	assert(_dataFormat == kDataFormatMipMaps);
	return _mipmaps;
}

const byte *DDS::rawData() const {
	assert(_dataFormat != kDataFormatInvalid
	        && _dataFormat != kDataFormatMipMaps);
	return _rawData;
}

uint DDS::rawDataSize() const {
	assert(_dataFormat != kDataFormatInvalid
	        && _dataFormat != kDataFormatMipMaps);
	return _rawDataSize;
}

bool DDS::readHeader(Common::SeekableReadStream &dds) {
	// We found the FourCC of a standard DDS
	uint32 magic = dds.readUint32BE();
	if (magic != kDDSID) {
		warning("Invalid DDS magic number: %d for %s", magic, _name.c_str());
		return false;
	}

	// All DDS header should be 124 bytes (+ 4 for the FourCC)
	uint32 headerSize = dds.readUint32LE();
	if (headerSize != 124) {
		warning("Invalid DDS header size: %d for %s", headerSize, _name.c_str());
		return false;
	}

	// DDS features
	uint32 flags = dds.readUint32LE();

	// Image dimensions
	_height = dds.readUint32LE();
	_width  = dds.readUint32LE();

	if ((_width >= 0x8000) || (_height >= 0x8000)) {
		warning("Unsupported DDS image dimensions (%ux%u) for %s", _width, _height, _name.c_str());
		return false;
	}

	dds.skip(4 + 4); // Pitch + Depth
	//uint32 pitchOrLineSize = dds.readUint32LE();
	//uint32 depth           = dds.readUint32LE();
	_mipMapCount     = dds.readUint32LE();

	// DDS doesn't provide any mip maps, only one full-size image
	if ((flags & kHeaderFlagsHasMipMaps) == 0) {
		_mipMapCount = 1;
	}

	dds.skip(44); // Reserved

	// Read the pixel data format
	DDSPixelFormat format;
	format.size     = dds.readUint32LE();
	format.flags    = dds.readUint32LE();
	format.fourCC   = dds.readUint32BE();
	format.bitCount = dds.readUint32LE();
	format.rBitMask = dds.readUint32LE();
	format.gBitMask = dds.readUint32LE();
	format.bBitMask = dds.readUint32LE();
	format.aBitMask = dds.readUint32LE();

	dds.skip(16 + 4); // DDCAPS2 + Reserved

	DDSHeaderDXT10 dxt10Header;
	if (format.fourCC == kFOURCC_DX10) {
		uint32 dxgiFormat             = dds.readUint32LE();
		if (dxgiFormat != kDXGIFormatBC1Unorm
		        && dxgiFormat != kDXGIFormatBC2Unorm
		        && dxgiFormat != kDXGIFormatBC3Unorm
		        && dxgiFormat != kDXGIFormatBC7Unorm) {
			warning("Unsupported DXGI format %x for %s", dxgiFormat, _name.c_str());
			return false;
		}

		uint32 resourceDimension      = dds.readUint32LE();
		if (resourceDimension != kDDSDimensionTexture2D) {
			warning("Unsupported resource dimension %d for %s", resourceDimension, _name.c_str());
			return false;
		}

		dxt10Header.dxgiFormat        = static_cast<DXGIFormat>(dxgiFormat);
		dxt10Header.resourceDimension = static_cast<DDSResourceDimension>(resourceDimension);
		dxt10Header.miscFlag          = dds.readUint32LE();
		dxt10Header.arraySize         = dds.readUint32LE();
		dxt10Header.miscFlags2        = dds.readUint32LE();
	}

	// Detect which specific format it describes
	if (!detectFormat(format, dxt10Header)) {
		return false;
	}

	return true;
}

bool DDS::readMipMaps(Common::SeekableReadStream &dds) {
	_mipmaps.resize(_mipMapCount);

	uint32 width  = _width;
	uint32 height = _height;

	for (uint i = 0; i < _mipMapCount; i++) {
		Graphics::Surface &mipmap = _mipmaps[i];
		mipmap.create(width, height, _format);

		uint32 size = mipmap.pitch * mipmap.h;
		uint32 readSize = dds.read(mipmap.getPixels(), size);

		if (readSize != size) {
			warning("Inconsistent read size in DDS file: %d, expected %d for %s level %d",
			        readSize, size, _name.c_str(), i);
			return false;
		}

		width  >>= 1;
		height >>= 1;
	}

	return true;
}

bool DDS::readRaw(Common::SeekableReadStream &dds) {
	uint sizeToRead = dds.size() - dds.pos();

	_rawData = (byte *)malloc(sizeToRead);
	_rawDataSize = dds.read(_rawData, sizeToRead);

	return _rawDataSize == sizeToRead;
}

bool DDS::detectFormat(const DDSPixelFormat &format, const DDSHeaderDXT10 &dxt10Header) {
	if ((format.flags & kPixelFlagsHasFourCC) && format.fourCC == kFOURCC_DXT1) {
		_dataFormat = kDataFormatRawBC1Unorm;
		return true;
	} else if ((format.flags & kPixelFlagsHasFourCC) && format.fourCC == kFOURCC_DXT3) {
		_dataFormat = kDataFormatRawBC2Unorm;
		return true;
	} else if ((format.flags & kPixelFlagsHasFourCC) && format.fourCC == kFOURCC_DXT5) {
		_dataFormat = kDataFormatRawBC3Unorm;
		return true;
	} else if ((format.flags & kPixelFlagsHasFourCC) && format.fourCC == kFOURCC_DX10) {
		if (dxt10Header.arraySize != 1) {
			warning("Unsupported DDS feature: array of textures with %d elements for %s", dxt10Header.arraySize, _name.c_str());
			return false;
		}

		if (dxt10Header.miscFlag & kMiscFlagsTextureCube) {
			warning("Unsupported DDS feature: texture cube flag for %s", _name.c_str());
			return false;
		}

		switch (dxt10Header.dxgiFormat) {
		case kDXGIFormatBC1Unorm:
			_dataFormat = kDataFormatRawBC1Unorm;
			break;
		case kDXGIFormatBC7Unorm:
			_dataFormat = kDataFormatRawBC7Unorm;
			break;
		default:
			warning("Unsupported DDS DXGI format: %x for %s", dxt10Header.dxgiFormat, _name.c_str());
			return false;
		}

		return true;
	} else if (format.flags & kPixelFlagsHasFourCC) {
		warning("Unsupported DDS feature: FourCC pixel format %d for %s", format.fourCC, _name.c_str());
		return false;
	}

	if (format.flags & kPixelFlagsIsIndexed) {
		warning("Unsupported DDS feature: Indexed %d-bits pixel format for %s", format.bitCount, _name.c_str());
		return false;
	}

	if (!(format.flags & kPixelFlagsIsRGB)) {
		warning("Only RGB DDS files are supported for %s", _name.c_str());
		return false;
	}

	if (format.bitCount != 24 && format.bitCount != 32) {
		warning("Only 24-bits and 32-bits DDS files are supported for %s", _name.c_str());
		return false;
	}

	if ((format.flags & kPixelFlagsHasAlpha) &&
	           (format.bitCount == 32) &&
	           (format.rBitMask == 0x00FF0000) && (format.gBitMask == 0x0000FF00) &&
	           (format.bBitMask == 0x000000FF) && (format.aBitMask == 0xFF000000)) {
		_dataFormat = kDataFormatMipMaps;
#ifdef SCUMM_BIG_ENDIAN
		_format = Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 0, 8, 16);
#else
		_format = Graphics::PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24);
#endif
		return true;
	} else if (!(format.flags & kPixelFlagsHasAlpha) &&
	           (format.bitCount == 24) &&
	           (format.rBitMask == 0x00FF0000) && (format.gBitMask == 0x0000FF00) &&
	           (format.bBitMask == 0x000000FF)) {
		_dataFormat = kDataFormatMipMaps;
#ifdef SCUMM_BIG_ENDIAN
		_format = Graphics::PixelFormat(3, 8, 8, 8, 0, 0, 8, 16, 0);
#else
		_format = Graphics::PixelFormat(3, 8, 8, 8, 0, 16, 8, 0, 0);
#endif
		return true;
	} else {
		warning("Unsupported pixel format (%X, %X, %d, %X, %X, %X, %X) for %s",
		        format.flags, format.fourCC, format.bitCount,
		        format.rBitMask, format.gBitMask, format.bBitMask, format.aBitMask,
		        _name.c_str());
		return false;
	}
}

} // End of namespace Image
