/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */
#pragma once

#include "inputdevice.h"

namespace vive
{

class VVCORE_EXPORT ConstInputDevice : public InputDevice
{

public:
    ConstInputDevice(const std::string &name);

    bool needsThread() const;
};
}
