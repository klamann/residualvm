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

#include "common/scummsys.h"

#if defined(USE_GLES2) || defined(USE_OPENGL_SHADERS)

#include "engines/myst3/node_opengl_shaders.h"

#include "engines/myst3/effects.h"
#include "engines/myst3/gfx_opengl_shaders.h"
#include "engines/myst3/gfx_opengl_texture.h"
#include "engines/myst3/node.h"
#include "engines/myst3/resource_loader.h"
#include "engines/myst3/state.h"

#include "graphics/opengl/system_headers.h"

namespace Myst3 {

NodeShaderRenderer::NodeShaderRenderer(Node &node, Layout &layout, Renderer &gfx, GameState &state, ResourceLoader &resourceLoader) :
		_node(node),
		_layout(layout),
		_gfx(static_cast<ShaderRenderer &>(gfx)),
		_state(state),
		_resourceLoader(resourceLoader),
		_shieldEffectPattern(nullptr) {

	_effectsCubeShader  = _gfx.createCubeEffectsShaderInstance();
	_effectsFrameShader = _gfx.createFrameEffectsShaderInstance();

	TextureLoader textureLoader(_gfx);

	_faces.resize(_node.type() == Node::kCube ? 6 : 1);
	for (uint faceId = 0; faceId < _faces.size(); faceId++) {
		Face &face = _faces[faceId];

		ResourceDescription resource;
		if (_node.type() == Node::kCube)  {
			resource = resourceLoader.getCubeBitmap(_node.room(), _node.id(), faceId);
		} else {
			resource = resourceLoader.getFrameBitmap(_node.room(), _node.id());
		}

		face.id               = faceId;
		face.baseTexture      = static_cast<OpenGLTexture *>(textureLoader.load(resource, TextureLoader::kImageFormatJPEG));
		face.assembledTexture = new OpenGLTexture(face.baseTexture->width, face.baseTexture->height, Texture::getRGBAPixelFormat());

		if (_node.type() == Node::kCube)  {
			face.baseTexture->upsideDown = true;
		} else {
			face.assembledTexture->upsideDown = true;
		}

		glGenFramebuffers(1, &face.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, face.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, face.assembledTexture->id, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			error("Framebuffer is not complete! status: %d", status);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

NodeShaderRenderer::~NodeShaderRenderer() {
	for (uint i = 0; i < _faces.size(); i++) {
		delete _faces[i].baseTexture;
		glDeleteFramebuffers(1, &_faces[i].fbo);
		delete _faces[i].assembledTexture;
	}

	for (uint i = 0; i < _spotItemTextures.size(); i++) {
		delete _spotItemTextures[i].texture;
	}

	for (EffectMaskTextureMap::iterator it = _effectMaskTextures.begin(); it != _effectMaskTextures.end(); ++it) {
		delete it->_value;
	}

	delete _effectsCubeShader;
	delete _effectsFrameShader;
	delete _shieldEffectPattern;
}

void NodeShaderRenderer::drawSpotItemTexture(SpotItemTexture &spotItemTexture, float transparency) {
	FloatSize faceSize;
	switch (_node.type()) {
	case Node::kCube:
		faceSize = FloatSize(Renderer::kOriginalWidth, Renderer::kOriginalWidth);
		break;
	case Node::kFrame:
		faceSize = FloatSize(Renderer::kOriginalWidth, Renderer::kFrameHeight);
		break;
	case Node::kMenu:
		faceSize = FloatSize(Renderer::kOriginalWidth, Renderer::kOriginalHeight);
		break;
	}

	FloatRect faceRectNorm = spotItemTexture.faceRect
	        .normalize(faceSize);

	_gfx.drawTexturedRect2D(faceRectNorm, spotItemTexture.textureRect, *spotItemTexture.texture, transparency);
}

void NodeShaderRenderer::drawSpotItem(SpotItem &spotItem, Face &face, float transparency) {
	for (uint i = 0; i < _spotItemTextures.size(); i++) {
		SpotItemTexture &spotItemTexture = _spotItemTextures[i];
		if (spotItemTexture.spotItemId != spotItem.id()) continue;
		if (spotItemTexture.faceId != face.id) continue;

		drawSpotItemTexture(spotItemTexture, transparency);
	}
}

bool NodeShaderRenderer::areSpotItemsDirty() {
	for (uint i = 0; i < _node.spotItems().size(); i++) {
		SpotItem &spotItem = _node.spotItems()[i];

		bool newDrawn = _state.evaluate(spotItem.condition());
		if (newDrawn != spotItem.drawn()) {
			return true;
		}

		if (newDrawn && spotItem.shouldFade()) {
			uint16 newFadeValue = _state.getVar(spotItem.fadeVariable());
			if (newFadeValue != spotItem.fadeValue()) {
				return true;
			}
		}
	}

	return false;
}

bool NodeShaderRenderer::isFaceVisible(uint faceId) {
	switch (_node.type()) {
	case Node::kFrame:
	case Node::kMenu:
		return true;
	case Node::kCube:
		return _gfx.isCubeFaceVisible(faceId);
	}

	return false;
}

void NodeShaderRenderer::initSpotItem(SpotItem &spotItem) {
	TextureLoader textureLoader(_gfx);
	ResourceDescriptionArray resources = _resourceLoader.listSpotItemImages(_node.room(), spotItem.id());

	for (uint i = 0; i < resources.size(); i++) {
		const ResourceDescription &resource = resources[i];
		ResourceDescription::SpotItemData spotItemData = resource.spotItemData();
		Face &face = _faces[resource.face() - 1];

		// Assume modded spotitems are scaled by the same ratio as the corresponding face
		float faceScaleRatio;
		if (resource.type() == Archive::kModdedSpotItem) {
			faceScaleRatio = face.baseTexture->width / (float)Renderer::kOriginalWidth;
		} else {
			faceScaleRatio = 1.f;
		}

		SpotItemTexture spotItemTexture;
		spotItemTexture.spotItemId = spotItem.id();
		spotItemTexture.faceId     = face.id;
		spotItemTexture.texture    = static_cast<OpenGLTexture *>(textureLoader.load(resource, TextureLoader::kImageFormatJPEG));
		spotItemTexture.faceRect   = spotItemTexture.texture->size()
		        .scale(1 / faceScaleRatio)
		        .translate(FloatPoint(spotItemData.u, spotItemData.v));

		if (_node.type() == Node::kCube)  {
			spotItemTexture.texture->upsideDown = true;
		}

		_spotItemTextures.push_back(spotItemTexture);
	}
}

void NodeShaderRenderer::initSpotItemMenu(SpotItem &spotItem, const Common::Rect &faceRect) {
	Graphics::Surface black;
	black.create(faceRect.width(), faceRect.height(), Texture::getRGBAPixelFormat());

	SpotItemTexture spotItemTexture;
	spotItemTexture.spotItemId = spotItem.id();
	spotItemTexture.faceId     = 0;
	spotItemTexture.faceRect   = FloatRect(faceRect.left, faceRect.top, faceRect.right, faceRect.bottom);
	spotItemTexture.texture    = new OpenGLTexture(black);

	black.free();

	_spotItemTextures.push_back(spotItemTexture);
}

void NodeShaderRenderer::updateSpotItemBitmap(uint16 spotItemId, const Graphics::Surface &surface) {
	for (uint i = 0; i < _spotItemTextures.size(); i++) {
		SpotItemTexture &spotItemTexture = _spotItemTextures[i];
		if (spotItemTexture.spotItemId == spotItemId) {
			spotItemTexture.texture->update(surface);

			_faces[spotItemTexture.faceId].dirty = true;
			break;
		}
	}
}

void NodeShaderRenderer::updateSpotItemTexture(uint16 spotItemId, Texture *texture, const FloatRect &textureRect) {
	assert(texture);

	for (uint i = 0; i < _spotItemTextures.size(); i++) {
		SpotItemTexture &spotItemTexture = _spotItemTextures[i];
		if (spotItemTexture.spotItemId == spotItemId) {
			delete spotItemTexture.texture;

			spotItemTexture.texture     = static_cast<OpenGLTexture *>(texture);
			spotItemTexture.textureRect = textureRect;

			_faces[spotItemTexture.faceId].dirty = true;
			break;
		}
	}
}

void NodeShaderRenderer::clearSpotItemBitmap(uint16 spotItemId) {
	for (uint i = 0; i < _spotItemTextures.size(); i++) {
		SpotItemTexture &spotItemTexture = _spotItemTextures[i];
		if (spotItemTexture.spotItemId == spotItemId) {
			Graphics::Surface black;
			black.create(spotItemTexture.texture->width, spotItemTexture.texture->height, Texture::getRGBAPixelFormat());

			spotItemTexture.texture->update(black);

			_faces[spotItemTexture.faceId].dirty = true;

			black.free();
			break;
		}
	}
}

void NodeShaderRenderer::initEffects() {
	const EffectArray &effects = _node.effects();
	for (uint i = 0; i < effects.size(); i++) {
		Effect *effect = effects[i];
		const Effect::FaceMaskArray &faceMasks = effect->facesMasks();
		for (uint faceId = 0; faceId < faceMasks.size(); faceId++) {
			Effect::FaceMask *faceMask = faceMasks[faceId];
			if (!faceMask) {
				continue;
			}

			OpenGLTexture *faceMaskTexture = new OpenGLTexture(*faceMask->surface);
			assert(!_effectMaskTextures.contains(EffectFace(effect, faceId)));
			_effectMaskTextures[EffectFace(effect, faceId)] = faceMaskTexture;
		}

		if (effect->type() == kEffectShield && !_shieldEffectPattern) {
			ShieldEffect *shieldEffect = static_cast<ShieldEffect *>(effect);
			_shieldEffectPattern = new OpenGLTexture(shieldEffect->pattern());
		}
	}
}

void NodeShaderRenderer::update() {
	EffectArray &effects = _node.effects();
	for (uint i = 0; i < effects.size(); i++) {
		effects[i]->update();
	}
}

void NodeShaderRenderer::draw() {
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

void NodeShaderRenderer::drawFace(Face &face) {
	Common::Rect textureRect = Common::Rect(face.baseTexture->width, face.baseTexture->height);

	glBindFramebuffer(GL_FRAMEBUFFER, face.fbo);
	glViewport(0, 0, textureRect.width(), textureRect.height());

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	_gfx.drawTexturedRect2D(FloatRect::unit(), FloatRect::unit(), *face.baseTexture);

	for (uint i = 0; i < _node.spotItems().size(); i++) {
		SpotItem &spotItem = _node.spotItems()[i];

		bool drawn = _state.evaluate(spotItem.condition());
		if (drawn && spotItem.shouldFade()) {
			uint16 newFadeValue = _state.getVar(spotItem.fadeVariable());
			uint16 drawFadeValue = CLIP<uint16>(newFadeValue, 0, 100);
			drawSpotItem(spotItem, face, drawFadeValue / 100.f);

			spotItem.setFadeValue(newFadeValue);
		} else if (drawn) {
			drawSpotItem(spotItem, face, -1.f);
		}

		spotItem.setDrawn(drawn);
	}

	face.dirty = false;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void NodeShaderRenderer::setupEffectsShader(OpenGL::Shader &shader, uint faceId, const EffectArray &effects) {
	shader.setUniform("faceId",  faceId);
	shader.setUniform("waterEffect",  false);
	shader.setUniform("lavaEffect",   false);
	shader.setUniform("magnetEffect", false);
	shader.setUniform("shieldEffect", false);

	OpenGLTexture *imageTexture = _faces[faceId].assembledTexture;

	shader.setUniform1f("texScale", imageTexture->width / (float) imageTexture->internalWidth);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, imageTexture->id);

	for (uint j = 0; j < effects.size(); j++) {
		Effect *effect = effects[j];

		switch (effect->type()) {
		case kEffectWater: {
			OpenGLTexture *faceMaskTexture = _effectMaskTextures[EffectFace(effect, faceId)];
			if (!faceMaskTexture) {
				break;
			}

			// TODO: Overflow
			uint32 currentTime = g_system->getMillis();
			uint position = (currentTime * _state.getWaterEffectSpeed() / _state.getWaterEffectMaxStep()) % 1000;

			shader.setUniform  ("waterEffect",            true);
			shader.setUniform1f("waterEffectPosition",    position / 1000.f);
			shader.setUniform1f("waterEffectAttenuation", 1 - _state.getWaterEffectAttenuation() / 640.f);
			shader.setUniform1f("waterEffectFrequency",   _state.getWaterEffectFrequency()   / 10.f);
			shader.setUniform1f("waterEffectAmpl",        _state.getWaterEffectAmpl()        / 20.f);
			shader.setUniform1f("waterEffectAmplOffset",  _state.getWaterEffectAmplOffset()  / 255.f);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, faceMaskTexture->id);
			break;
		}
		case kEffectLava: {
			OpenGLTexture *faceMaskTexture = _effectMaskTextures[EffectFace(effect, faceId)];
			if (!faceMaskTexture) {
				break;
			}

			// TODO: Overflow
			uint32 currentTime = g_system->getMillis();
			uint position = (currentTime * _state.getLavaEffectSpeed() / 256) % 1000;

			float ampl = _state.getLavaEffectAmpl() / 10.f;

			shader.setUniform  ("lavaEffect",         true);
			shader.setUniform1f("lavaEffectPosition", position / 1000.f);
			shader.setUniform1f("lavaEffectAmpl",     ampl);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, faceMaskTexture->id);
			break;
		}
		case kEffectMagnet: {
			OpenGLTexture *faceMaskTexture = _effectMaskTextures[EffectFace(effect, faceId)];
			if (!faceMaskTexture) {
				break;
			}

			// TODO: Overflow
			uint32 currentTime = g_system->getMillis();
			uint position = (currentTime * _state.getMagnetEffectSpeed() / 10) % 1000;

			float ampl = (_state.getMagnetEffectUnk1() + _state.getMagnetEffectUnk3())
					/ (float)_state.getMagnetEffectUnk2();

			shader.setUniform  ("magnetEffect",         true);
			shader.setUniform1f("magnetEffectPosition", position / 1000.f);
			shader.setUniform1f("magnetEffectAmpl",     ampl);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, faceMaskTexture->id);
			break;
		}

		case kEffectShield: {
			OpenGLTexture *faceMaskTexture = _effectMaskTextures[EffectFace(effect, faceId)];
			if (!faceMaskTexture) {
				break;
			}

			uint32 currentTime = g_system->getMillis();
			uint position = (currentTime / 4) % 1000;

			float ampl = sin((currentTime % 11520) * 2 * M_PI / 11520) * 1.5 + 2.5;

			shader.setUniform  ("shieldEffect",         true);
			shader.setUniform1f("shieldEffectPosition", position / 1000.f);
			shader.setUniform1f("shieldEffectAmpl",     ampl);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, faceMaskTexture->id);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, _shieldEffectPattern->id);
			break;
		}
		default:
			break;
		}
	}
}

void NodeShaderRenderer::drawFrame(bool menu) {
	assert(!_faces.empty());

	Face &face = _faces[0];

	if (areSpotItemsDirty() || face.dirty) {
		drawFace(face);
	}

	FloatRect sceneViewport;
	if (menu) {
		sceneViewport = _layout.menuViewport();
	} else {
		sceneViewport = _layout.frameViewport();
	}

	_gfx.setViewport(sceneViewport, false);

	_effectsFrameShader->use();
	_effectsFrameShader->setUniform("verOffsetXY",      Math::Vector2d(0.f, 0.f));
	_effectsFrameShader->setUniform("verSizeWH",        Math::Vector2d(1.f, 1.f));
	_effectsFrameShader->setUniform("texOffsetXY",      Math::Vector2d(0.f, 0.f));
	_effectsFrameShader->setUniform("texSizeWH",        Math::Vector2d(1.f, 1.f));
	_effectsFrameShader->setUniform("flipY",            face.assembledTexture->upsideDown);
	_effectsFrameShader->setUniform("texImage",         0);
	_effectsFrameShader->setUniform("texEffect1",       1);
	_effectsFrameShader->setUniform("texEffect2",       2);
	_effectsFrameShader->setUniform("texEffectPattern", 3);
	_effectsFrameShader->setUniform("frame",            true);

	glDepthMask(GL_FALSE);

	setupEffectsShader(*_effectsFrameShader, 0, _node.effects());

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glActiveTexture(GL_TEXTURE0);
	glDepthMask(GL_TRUE);
}

void NodeShaderRenderer::drawCube() {
	assert(_faces.size() == 6);

	if (areSpotItemsDirty()) {
		for (uint i = 0; i < _faces.size(); i++) {
			_faces[i].dirty = true;
		}
	}

	for (uint i = 0; i < _faces.size(); i++) {
		if (_faces[i].dirty && isFaceVisible(i)) {
			drawFace(_faces[i]);
		}
	}

	FloatRect sceneViewport = _layout.frameViewport();
	_gfx.setViewport(sceneViewport, true);

	glDepthMask(GL_FALSE);

	_effectsCubeShader->use();
	_effectsCubeShader->setUniform("mvpMatrix",        _gfx.getMvpMatrix());
	_effectsCubeShader->setUniform("texImage",         0);
	_effectsCubeShader->setUniform("texEffect1",       1);
	_effectsCubeShader->setUniform("texEffect2",       2);
	_effectsCubeShader->setUniform("texEffectPattern", 3);
	_effectsFrameShader->setUniform("frame",           false);

	for (uint faceId = 0; faceId < _faces.size(); faceId++) {
		setupEffectsShader(*_effectsCubeShader, faceId, _node.effects());

		glDrawArrays(GL_TRIANGLE_STRIP, 4 * faceId, 4);
	}

	glActiveTexture(GL_TEXTURE0);
	glDepthMask(GL_TRUE);
}

} // end of namespace Myst3

#endif
