/**
 * @file storerdmsubdevices.h
 *
 */
/* Copyright (C) 2020 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef STORERDMSUBDEVICES_H_
#define STORERDMSUBDEVICES_H_

#include "rdmsubdevicesparams.h"

#include "configstore.h"

class StoreRDMSubDevices final: public RDMSubDevicesParamsStore {
public:
	StoreRDMSubDevices();

	void Update(const struct TRDMSubDevicesParams *pRDMSubDevicesParams) override {
		ConfigStore::Get()->Update(configstore::Store::RDMSUBDEVICES, pRDMSubDevicesParams, sizeof(struct TRDMSubDevicesParams));
	}

	void Copy(struct TRDMSubDevicesParams *pRDMSubDevicesParams) override {
		ConfigStore::Get()->Copy(configstore::Store::RDMSUBDEVICES, pRDMSubDevicesParams, sizeof(struct TRDMSubDevicesParams));
	}

	static StoreRDMSubDevices *Get() {
		return s_pThis;
	}

private:
	static StoreRDMSubDevices *s_pThis;
};

#endif /* STORERDMSUBDEVICES_H_ */
