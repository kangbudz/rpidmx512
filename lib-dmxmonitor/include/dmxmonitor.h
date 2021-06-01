/**
 * @file dmxmonitor.h
 *
 */
/* Copyright (C) 2016-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef DMXMONITOR_H_
#define DMXMONITOR_H_

#include <cstdint>

#include "lightset.h"

namespace dmxmonitor {
enum class Format {
	HEX, PCT, DEC,
};
namespace output {
namespace hdmi {
static constexpr char MAX_PORTS = 1;
}  // namespace hdmi
namespace text {
static constexpr char MAX_PORTS = 4;
}  // namespace text
}  // namespace output
}  // namespace dmxmonitor

class DMXMonitor: public LightSet {
public:
	DMXMonitor();
	~DMXMonitor() override {}

	void Start(uint32_t nPortIndex) override;
	void Stop(uint32_t nPortIndex) override;

	void SetData(uint32_t nPortIndex, const uint8_t *pData, uint32_t nLength) override;

	bool SetDmxStartAddress(uint16_t nDmxStartAddress) override;
	uint16_t GetDmxStartAddress() override;

	uint16_t GetDmxFootprint() override;

	void SetFormat(dmxmonitor::Format tFormat) {
		m_tFormat = tFormat;
	}

	dmxmonitor::Format GetFormat() const {
		return m_tFormat;
	}

	void Cls();

#if defined (__linux__) || defined (__CYGWIN__) || defined(__APPLE__)
	void SetMaxDmxChannels(uint16_t nMaxChannels);

private:
	void DisplayDateTime(uint32_t nPortId, const char *pString);
#endif

private:
	void Update();

private:
	dmxmonitor::Format m_tFormat = dmxmonitor::Format::HEX;
	uint32_t m_nSlots { 0 };
#if defined (__linux__) || defined (__CYGWIN__) || defined(__APPLE__)
	enum {
		DMX_DEFAULT_MAX_CHANNELS = 32,
	};
	bool m_bIsStarted[dmxmonitor::output::text::MAX_PORTS];
	uint16_t m_nDmxStartAddress {lightset::Dmx::START_ADDRESS_DEFAULT};
	uint16_t m_nMaxChannels { DMX_DEFAULT_MAX_CHANNELS};
#else
	bool m_bIsStarted { false };
	uint8_t m_Data[512];
#endif
};

#endif /* DMXMONITOR_H_ */
