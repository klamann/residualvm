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

#include "engines/stark/tests/test.h"

#include "engines/stark/tests/actionlog.h"
#include "engines/stark/tests/prologue.h"

#include "common/debug.h"

namespace Stark {
namespace Tests {

ActionLogTest *makeTestByName(const char *name) {

	if (strcmp(name, "prologue") == 0) {
		return new PrologueTest();
	}

	return nullptr;
}

ActionLogTest::ActionLogTest() :
		_log(new ActionLog()) {
}

ActionLogTest::~ActionLogTest() {
	delete _log;
}

ActionLog *ActionLogTest::takeActionLog() {
	ActionLog *log = _log;
	_log = nullptr;
	return log;
}

void ActionLogTest::assertLocation(uint16 level, uint16 location, uint32 timeout) {
	_log->addAction(new AssertLocation(level, location, timeout));
}

void ActionLogTest::walkTo(const Math::Vector3d &destination) {
	_log->addAction(new WalkToAction(destination));
}

void ActionLogTest::interactWithItemAt(const Common::String &itemRefStr, uint32 action, const Common::Point &position) {
	_log->addAction(new DoActionAt(itemRefStr, action, position));
}

void ActionLogTest::assertHasInventoryItem(const Common::String &name, uint32 timeout) {
	_log->addAction(new AssertHasInventoryItem(name, timeout));

}

} // End of namespace Tests
} // End of namespace Stark

