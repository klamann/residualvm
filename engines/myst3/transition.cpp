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

#include "common/events.h"
#include "common/config-manager.h"

#include "engines/myst3/transition.h"
#include "engines/myst3/sound.h"
#include "engines/myst3/state.h"

#include "graphics/colormasks.h"
#include "graphics/surface.h"

namespace Myst3 {

Transition::Transition(Myst3Engine *vm) :
		_vm(vm),
		_frameLimiter(new FrameLimiter(g_system, ConfMan.getInt("engine_speed"))),
		_type(kTransitionNone),
		_sourceScreenshot(nullptr) {

	// Capture a screenshot of the source node
	int durationTicks = computeDuration();
	if (durationTicks) {
		Common::Rect screen = _vm->_layout->screenViewportInt();
		_sourceScreenshot = _vm->_gfx->copyScreenshotToTexture(screen);
	}
}

Transition::~Transition() {
	delete _sourceScreenshot;
	delete _frameLimiter;
}

int Transition::computeDuration() {
	int durationTicks = 30 * (100 - ConfMan.getInt("transition_speed")) / 100;
	if (_type == kTransitionZip) {
		durationTicks >>= 1;
	}

	return durationTicks;
}

void Transition::playSound() {
	if (_vm->_state->getTransitionSound()) {
		_vm->_sound->playEffect(_vm->_state->getTransitionSound(),
				_vm->_state->getTransitionSoundVolume());
	}
	_vm->_state->setTransitionSound(0);
}

void Transition::draw(TransitionType type) {
	_type = type;

	// Play the transition sound
	playSound();

	int durationTicks = computeDuration();

	// Got any transition to draw?
	if (!_sourceScreenshot || type == kTransitionNone || durationTicks == 0) {
		return;
	}

	// Capture a screenshot of the destination node
	_vm->drawFrame(true);

	Common::Rect screen = _vm->_layout->screenViewportInt();
	Texture *targetScreenshot = _vm->_gfx->copyScreenshotToTexture(screen);

	// Compute the start and end frames for the animation
	int startTick = _vm->_state->getTickCount();
	uint endTick = startTick + durationTicks;

	// Draw on the full screen
	FloatRect viewport = _vm->_layout->screenViewport();
	_vm->_gfx->setViewport(viewport, false);

	// Draw each step until completion
	int completion = 0;
	while ((_vm->_state->getTickCount() <= endTick || completion < 100) && !_vm->shouldQuit()) {
		_frameLimiter->startFrame();

		completion = CLIP<int>(100 * (_vm->_state->getTickCount() - startTick) / durationTicks, 0, 100);

		_vm->_gfx->clear();

		drawStep(*targetScreenshot, *_sourceScreenshot, completion);

		_vm->_gfx->flipBuffer();
		_frameLimiter->delayBeforeSwap();
		g_system->updateScreen();
		_vm->_state->updateFrameCounters();

		Common::Event event;
		while (_vm->getEventManager()->pollEvent(event)) {
			// Ignore all the events happening during transitions, so that the view does not move
			// between the initial transition screen shoot and the first frame drawn after the transition.

			// However, keep updating the keyboard state so we don't end up in
			// an unbalanced state where the engine believes keys are still
			// pressed while they are not.
			_vm->processEventForKeyboardState(event);

			if (_vm->_state->hasVarGamePadUpPressed()) {
				_vm->processEventForGamepad(event);
			}
		}
	}

	delete targetScreenshot;
	delete _sourceScreenshot;
	_sourceScreenshot = nullptr;
}

void Transition::drawStep(Texture &targetTexture, Texture &sourceTexture, uint completion) {

	switch (_type) {
	case kTransitionNone:
		break;

	case kTransitionFade:
	case kTransitionZip: {
		_vm->_gfx->drawTexturedRect2D(FloatRect::unit(), FloatRect::unit(), sourceTexture);
		_vm->_gfx->drawTexturedRect2D(FloatRect::unit(), FloatRect::unit(), targetTexture, completion / 100.0);
		break;
	}

	case kTransitionLeftToRight: {
		float transitionX = (100 - completion) / 100.f;
		FloatRect sourceRect(.0f, .0f, transitionX, 1.f);
		FloatRect targetRect(transitionX, .0f, 1.f, 1.f);

		_vm->_gfx->drawTexturedRect2D(sourceRect, sourceRect, sourceTexture);
		_vm->_gfx->drawTexturedRect2D(targetRect, targetRect, targetTexture);
		break;
	}

	case kTransitionRightToLeft: {
		float transitionX = completion / 100.f;
		FloatRect sourceRect(transitionX, .0f, 1.f, 1.f);
		FloatRect targetRect(.0f, .0f, transitionX, 1.f);

		_vm->_gfx->drawTexturedRect2D(sourceRect, sourceRect, sourceTexture);
		_vm->_gfx->drawTexturedRect2D(targetRect, targetRect, targetTexture);
		break;
	}

	}
}

} // End of namespace Myst3
