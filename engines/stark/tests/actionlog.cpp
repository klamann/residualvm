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

#include "engines/stark/tests/actionlog.h"

#include "engines/stark/resources/item.h"
#include "engines/stark/resources/level.h"
#include "engines/stark/resources/location.h"

#include "engines/stark/services/gameinterface.h"
#include "engines/stark/services/global.h"
#include "engines/stark/services/services.h"
#include "engines/stark/services/userinterface.h"
#include "engines/stark/services/resourceprovider.h"

#include "common/debug.h"

namespace Stark {
namespace Tests {

Action::~Action() {

}

bool Action::isComplete() {
	return !StarkGameInterface->isAprilWalking()
		&& StarkUserInterface->isInteractive()
		&& StarkGlobal->getCurrent()
		&& !StarkResourceProvider->hasLocationChangeRequest();
}

Assertion::Assertion(uint32 timeout) :
	_timeout(timeout),
	_timeLeftBeforeTimeout(0) {
}

void Assertion::perform() {
	_timeLeftBeforeTimeout = _timeout;
}

bool Assertion::isComplete() {
	if (isVerified()) {
		return true;
	}

	if (Action::isComplete()) {
		_timeLeftBeforeTimeout -= StarkGlobal->getMillisecondsPerGameloop();
	}

	if (_timeLeftBeforeTimeout <= 0) {
		warning("Assertion failed: %s", printActionCall().c_str());
		StarkActionLogger->stop();
	}

	return false;
}

Common::String WalkToAction::printActionCall() const {
	return Common::String::format("walkTo(Math::Vector3d(%ff, %ff, %ff))",
	      _destination.x(), _destination.y(), _destination.z());
}

WalkToAction::WalkToAction(const Math::Vector3d &destination) :
		_destination(destination) {

}

void WalkToAction::perform() {
	StarkGameInterface->walkTo(_destination);
}

Common::String DoAction::printActionCall() const {
	Common::String itemReferenceDescription = _itemRef.describe();
	return Common::String::format("interactWithItem(\"%s\", %d)", itemReferenceDescription.c_str(), _action);
}

DoAction::DoAction(const ResourceReference &itemRef, uint32 action) :
		_itemRef(itemRef),
		_action(action) {

}

DoAction::DoAction(const Common::String &itemRefStr, uint32 action) :
		_itemRef(ResourceReference(itemRefStr)),
		_action(action) {

}

void DoAction::perform() {
	Resources::ItemVisual *item = _itemRef.resolve<Resources::ItemVisual>();
	StarkGameInterface->itemDoAction(item, _action);
}

Common::String DoActionAt::printActionCall() const {
	Common::String itemReferenceDescription = _itemRef.describe();
	return Common::String::format("interactWithItemAt(\"%s\", %d, Common::Point(%d, %d))", itemReferenceDescription.c_str(),
	      _action, _position.x, _position.y);
}

DoActionAt::DoActionAt(const ResourceReference &itemRef, uint32 action, const Common::Point &position) :
		DoAction(itemRef, action),
		_position(position) {
}

DoActionAt::DoActionAt(const Common::String &itemRefStr, uint32 action, const Common::Point &position) :
		DoAction(itemRefStr, action),
		_position(position) {

}

void DoActionAt::perform() {
	Resources::ItemVisual *item = _itemRef.resolve<Resources::ItemVisual>();
	StarkGameInterface->itemDoActionAt(item, _action, _position);
}

AssertLocation::AssertLocation(uint16 level, uint16 location, uint32 timeout) :
		Assertion(timeout),
		_level(level),
		_location(location){
}

Common::String AssertLocation::printActionCall() const {
	return Common::String::format("assertLocation(0x%02x, 0x%02x)", _level, _location);
}

bool AssertLocation::isVerified() {
	Current *current = StarkGlobal->getCurrent();

	return current && current->getLevel()->getIndex() == _level
	       && current->getLocation()->getIndex() == _location;
}

AssertHasInventoryItem::AssertHasInventoryItem(const Common::String &name, uint32 timeout) :
		Assertion(timeout),
		_name(name) {
}

Common::String AssertHasInventoryItem::printActionCall() const {
	return Common::String::format("assertHasInventoryItem(\"%s\")", _name.c_str());
}

bool AssertHasInventoryItem::isVerified() {
	return StarkGlobal->hasInventoryItem(_name);
}

ActionLog::ActionLog() :
		_shouldStartFromBeginning(false),
		_playbackActionIndex(0) {
}

ActionLog::~ActionLog() {
	for (uint i = 0; i < _actions.size(); i++) {
		delete _actions[i];
	}
}

void ActionLog::addAction(Action *action) {
	_actions.push_back(action);
}

void ActionLog::print() const {
	for (uint i = 0; i < _actions.size(); i++) {
		debug("%s", _actions[i]->printActionCall().c_str());
	}
}

void ActionLog::startPlayback() {
	_shouldStartFromBeginning = true;
}

void ActionLog::playbackAction(size_t actionIndex) {
	_playbackActionIndex = actionIndex;
	debug("%s", _actions[actionIndex]->printActionCall().c_str());
	_actions[actionIndex]->perform();
}

void ActionLog::updatePlayback() {
	if (_shouldStartFromBeginning) {
		if (StarkUserInterface->isInteractive()) {
			playbackAction(0);
			_shouldStartFromBeginning = false;
		}
	} else {
		if (!_actions[_playbackActionIndex]->isComplete()) {
			return;
		}

		_playbackActionIndex++;
		if (!isPlaybackComplete()) {
			playbackAction(_playbackActionIndex);
		}
	}
}

bool ActionLog::isPlaybackComplete() const {
	return _playbackActionIndex >= _actions.size();
}

ActionLogger::ActionLogger() :
		_log(nullptr),
		_mode(kModeNoOperation) {

}

ActionLogger::~ActionLogger() {
	clearCurrentLog();
}

void ActionLogger::clearCurrentLog() {
	delete _log;
	_log = nullptr;
	_mode = kModeNoOperation;
}

void ActionLogger::startRecording() {
	clearCurrentLog();

	_mode = kModeRecord;
	_log = new ActionLog();
}

void ActionLogger::startPlayback(ActionLog *record) {
	clearCurrentLog();
	_log = record;

	startPlayback();
}

void ActionLogger::startPlayback() {
	_mode = kModePlay;
	_log->startPlayback();
}

void ActionLogger::update() {
	if (_mode == kModePlay) {
		if (_log->isPlaybackComplete()) {
			_mode = kModeNoOperation;
		} else {
			_log->updatePlayback();
		}
	}
}

void ActionLogger::addAction(Action *action) {
	if (_mode == kModeRecord) {
		_log->addAction(action);
	} else {
		// TODO: Perhaps use references here to avoid dubious memory management
		delete action;
	}
}

void ActionLogger::stop() {
	_mode = kModeNoOperation;
}

void ActionLogger::print() {
	if (_log) {
		_log->print();
	}
}

} // End of namespace Tests
} // End of namespace Stark

