/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2017-2022 by Arjan van Vught mailto:info@orangepi-dmx.nl
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
#include <cstring>
#include <cstdlib>

#include "hardware.h"
#include "network.h"
#include "ledblink.h"

#include "display.h"

#include "mdns.h"
#include "mdnsservices.h"

#include "httpd/httpd.h"

#include "artnet4node.h"
#include "artnetparams.h"
#include "storeartnet.h"
#include "artnetmsgconst.h"
#include "artnetrdmresponder.h"

#include "rdmdeviceresponder.h"
#include "rdmpersonality.h"
#include "rdmdeviceparams.h"

#include "dmxmonitor.h"
#include "dmxmonitorparams.h"

#include "configstore.h"

#include "remoteconfig.h"
#include "remoteconfigparams.h"

#include "storemonitor.h"
#include "storenetwork.h"
#include "storeremoteconfig.h"

#include "factorydefaults.h"

#include "firmwareversion.h"
#include "software_version.h"

using namespace artnet;

static constexpr uint32_t portIndexOffset = 0;

int main(int argc, char **argv) {
	Hardware hw;
	Network nw;
	LedBlink lb;
	Display display;
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);

	if (argc < 2) {
		printf("Usage: %s ip_address|interface_name\n", argv[0]);
		return -1;
	}

	hw.Print();
	fw.Print();

	ConfigStore configStore;

	StoreNetwork storeNetwork;

	if (nw.Init(argv[1]) < 0) {
		fprintf(stderr, "Not able to start the network\n");
		return -1;
	}

	nw.Print();

	StoreArtNet storeArtNet(portIndexOffset);
	ArtNetParams artnetParams(&storeArtNet);

	ArtNet4Node node;

	if (artnetParams.Load()) {
		artnetParams.Dump();
		artnetParams.Set(portIndexOffset);
	}

	printf("Art-Net %d Node - Real-time DMX Monitor {4 Universes}\n", node.GetVersion());

	StoreMonitor storeMonitor;
	DMXMonitorParams monitorParams(&storeMonitor);

	DMXMonitor monitor;
	monitor.SetDmxMonitorStore(&storeMonitor);

	if (monitorParams.Load()) {
		monitorParams.Dump();
		monitorParams.Set(&monitor);
	}

	node.SetOutput(&monitor);
	node.SetArtNetStore(&storeArtNet);

	RDMPersonality *pRDMPersonalities[1] = { new  RDMPersonality("Real-time DMX Monitor", &monitor)};
	ArtNetRdmResponder RdmResponder(pRDMPersonalities, 1);

	node.SetRdmUID(RdmResponder.GetUID());
	RdmResponder.Init();

	for (uint32_t nPortIndex = 0; nPortIndex < artnetnode::MAX_PORTS; nPortIndex++) {
		uint32_t nOffset = nPortIndex;
		if (nPortIndex >= portIndexOffset) {
			nOffset = nPortIndex - portIndexOffset;
		} else {
			continue;
		}

		printf(">> nPortIndex=%u, nOffset=%u\n", nPortIndex, nOffset);

		bool bIsSet;
		const auto nAddress = artnetParams.GetUniverse(nOffset, bIsSet);
		const auto portDirection =  artnetParams.GetDirection(nOffset);

		if (portDirection == lightset::PortDir::OUTPUT) {
			node.SetUniverse(nPortIndex, lightset::PortDir::OUTPUT, nAddress);
		} else {
			node.SetUniverse(nPortIndex, lightset::PortDir::DISABLE, nAddress);
		}
	}

	const auto nActivePorts = node.GetActiveOutputPorts();

	MDNS mDns;
	mDns.Start();
	mDns.AddServiceRecord(nullptr, MDNS_SERVICE_CONFIG, 0x2905);
	mDns.AddServiceRecord(nullptr, MDNS_SERVICE_HTTP, 80, mdns::Protocol::TCP, "node=Art-Net 4");
	mDns.Print();

	node.Print();

	HttpDaemon httpDaemon;
	httpDaemon.Start();

	RemoteConfig remoteConfig(remoteconfig::Node::ARTNET, remoteconfig::Output::MONITOR, nActivePorts);

	StoreRemoteConfig storeRemoteConfig;
	RemoteConfigParams remoteConfigParams(&storeRemoteConfig);

	if(remoteConfigParams.Load()) {
		remoteConfigParams.Set(&remoteConfig);
		remoteConfigParams.Dump();
	}

	while (configStore.Flash())
		;

	node.Start();

	for (;;) {
		node.Run();
		mDns.Run();
		httpDaemon.Run();
		remoteConfig.Run();
		configStore.Flash();
	}

	return 0;
}
