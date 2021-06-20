/**
 * @file dmxsender.cpp
 *
 */
/* Copyright (C) 2017-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <cstdint>
#include <cassert>

#include "dmxsend.h"
#include "dmx.h"

#include "debug.h"

using namespace dmx;

void DMXSend::Start(__attribute__((unused)) uint32_t nPortIndex) {
	DEBUG_ENTRY

	if (m_bIsStarted) {
		DEBUG_EXIT
		return;
	}

	m_bIsStarted = true;

	SetPortDirection(0, PortDirection::OUTP, true);
	DEBUG_EXIT
}

void DMXSend::Stop(__attribute__((unused)) uint32_t nPortIndex) {
	DEBUG_ENTRY

	if (!m_bIsStarted) {
		DEBUG_EXIT
		return;
	}

	m_bIsStarted = false;

	SetPortDirection(0, PortDirection::OUTP, false);
	DEBUG_EXIT
}

void DMXSend::SetData(__attribute__((unused)) uint32_t nPortIndexId, const uint8_t *pData, uint32_t nLength) {
	assert(pData != nullptr);
//	assert(nLength != 0);

	if (__builtin_expect((nLength == 0), 0)) {
//		DEBUG_EXIT
		return;
	}

	SetSendDataWithoutSC(pData, nLength);
}
