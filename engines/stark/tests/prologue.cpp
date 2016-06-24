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

#include "engines/stark/tests/prologue.h"

#include "engines/stark/services/resourceprovider.h"
#include "engines/stark/services/services.h"
#include "engines/stark/tests/actionlog.h"

#include "common/debug.h"

namespace Stark {
namespace Tests {

void PrologueTest::setUp() {
	StarkResourceProvider->requestLocationChange(0x08, 0x01);
}

void PrologueTest::test() {
	assertLocation(0x08, 0x01);
	walkTo(Math::Vector3d(47.032837f, 266.101562f, 115.322479f));
	walkTo(Math::Vector3d(294.174164f, 141.807129f, 137.240540f));
	assertLocation(0x08, 0x02);

	interactWithItemAt("(Level idx 8) (Location idx 2) (Layer idx 1) (Item idx 0)", 2, Common::Point(395, 223));
	interactWithItemAt("(Level idx 8) (Location idx 2) (Layer idx 1) (Item idx 0)", 1, Common::Point(405, 225));
	assertHasInventoryItem("Scale");

	interactWithItemAt("(Level idx 8) (Location idx 2) (Layer idx 1) (Item idx 9)", 2, Common::Point(10, -2));
	interactWithItemAt("(Level idx 8) (Location idx 2) (Layer idx 1) (Item idx 0)", 7, Common::Point(602, 257));
	assertLocation(0x08, 0x03);

	interactWithItemAt("(Level idx 8) (Location idx 3) (Layer idx 1) (Item idx 6)", 2, Common::Point(11, 89));
	interactWithItemAt("(Level idx 8) (Location idx 3) (Layer idx 1) (Item idx 17)", 2, Common::Point(24, 1));
}

} // End of namespace Tests
} // End of namespace Stark
