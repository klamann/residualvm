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

#ifndef STARK_TESTS_ACTION_LOG_H
#define STARK_TESTS_ACTION_LOG_H

#include "common/array.h"
#include "common/rect.h"

#include "math/vector3d.h"

#include "engines/stark/resourcereference.h"

namespace Stark {
namespace Tests {

/**
 * Base class for all the actions
 *
 * An action represents an interaction with the game world as performed by the player.
 * An action can be stored in an action log to be played back at a later point.
 */
class Action {
public:
	virtual ~Action();

	/**
	 * Return a string representing a call to this action as issued in the test suites
	 */
	virtual Common::String printActionCall() const = 0;

	/**
	 * Perform the action by manipulating the game world objects
	 */
	virtual void perform() = 0;

	/**
	 * Check if the action is complete after it has been initiated by a call to perform
	 */
	virtual bool isComplete();
};

/**
 * Base class for all the assertions
 *
 * An assertion verifies that a condition becomes true before a timeout expires.
 */
class Assertion : public Action {
public:
	explicit Assertion(uint32 timeout);

	// Action API
	void perform() override;
	bool isComplete() override;

	/**
	 * Check if the assertion condition is verified
	 */
	virtual bool isVerified() = 0;

protected:
	uint32 _timeout; // ms
	int32 _timeLeftBeforeTimeout; // ms
};

/**
 * Make the playable character walk to a destination point
 * in the currently loaded location.
 */
class WalkToAction : public Action {
public:
	explicit WalkToAction(const Math::Vector3d &destination);

	// Action API
	Common::String printActionCall() const override;
	void perform() override;

protected:
	Math::Vector3d _destination;
};

/**
 * Make the playable character perform an action on an item
 * in the game world.
 */
class DoAction : public Action {
public:
	DoAction(const Common::String &itemRefStr, uint32 action);
	DoAction(const ResourceReference &itemRef, uint32 action);

	// Action API
	Common::String printActionCall() const override;
	void perform() override;

protected:
	ResourceReference _itemRef;
	uint32 _action;
};

/**
 * Make the playable character perform an action on an item
 * in the game world at a specific mouse position.
 */
class DoActionAt : public DoAction {
public:
	DoActionAt(const Common::String &itemRefStr, uint32 action, const Common::Point &position);
	DoActionAt(const ResourceReference &itemRef, uint32 action, const Common::Point &position);

	// Action API
	Common::String printActionCall() const override;
	void perform() override;

protected:
	Common::Point _position;
};

/**
 * Verify the game is currently at a specific location
 */
class AssertLocation : public Assertion {
public:
	AssertLocation(uint16 level, uint16 location, uint32 timeout = 5000);

	// Action API
	Common::String printActionCall() const override;

	// Assertion API
	bool isVerified() override;

protected:
	uint16 _level;
	uint16 _location;
};

/**
 * Verify an item is in the inventory
 */
class AssertHasInventoryItem : public Assertion {
public:
	explicit AssertHasInventoryItem(const Common::String &name, uint32 timeout = 5000);

	// Action API
	Common::String printActionCall() const override;

	// Assertion API
	bool isVerified() override;

protected:
	Common::String _name;
};

/**
 * A log of player actions for a gameplay sequence
 */
class ActionLog {
public:
	ActionLog();
	~ActionLog();

	/** Add an action to the log and transfer ownership to it */
	void addAction(Action *action);

	/** Prints the log to debug output */
	void print() const;

	/** Play the log's actions from the beginning */
	void startPlayback();
	void updatePlayback();

	/** Has the last action of the log finished playing back? */
	bool isPlaybackComplete() const;

private:
	typedef Common::Array<Action *> ActionList;

	void playbackAction(size_t actionIndex);

	ActionList _actions;
	size_t _playbackActionIndex;
	bool _shouldStartFromBeginning;
};

/**
 * The ActionLogger is responsible for managing the state of the action logging feature
 *
 * It is the entry point for recording actions and playing them back.
 */
class ActionLogger {
public:
	ActionLogger();
	~ActionLogger();

	enum Mode {
		kModeRecord,
		kModePlay,
		kModeNoOperation
	};

	/**
	 * Start recording the player actions to a new action log
	 *
	 * The existing action log if any is dropped.
	 */
	void startRecording();

	/**
	 * Start playing the specified record from the beginning
	 *
	 * Transfers ownership of the record to the ActionLogger
	 */
	void startPlayback(ActionLog *record);

	/**
	 * Play the currently loaded action log from the beginning
	 */
	void startPlayback();

	/**
	 * Per frame update hook
	 */
	void update();

	/**
	 * Stop recording or playing back and return to no operation mode.
	 */
	void stop();

	/**
	 * Print the currently loaded action log to the debug output
	 */
	void print();

	/**
	 * Append an action to the current action log
	 *
	 * Has no effect when not recording.
	 */
	void addAction(Action *action);

private:
	void clearCurrentLog();

	Mode _mode;
	ActionLog *_log;
};

} // End of namespace Tests
} // End of namespace Stark

#endif // STARK_TESTS_ACTION_LOG_H
