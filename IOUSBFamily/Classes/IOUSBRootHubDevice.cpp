/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2006 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <libkern/OSByteOrder.h>

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBControllerV3.h>

#define super	IOUSBHubDevice
#define self	this

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBRootHubDevice, IOUSBHubDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBRootHubDevice*
IOUSBRootHubDevice::NewRootHubDevice()
{
	IOUSBRootHubDevice *me = new IOUSBRootHubDevice;
	
	if (!me)
		return NULL;
	
	if (!me->init())
	{
		me->release();
		me = NULL;
	}
	
	return me;
}


bool 
IOUSBRootHubDevice::init()
{
    if (!super::init())
        return false;
		
    // allocate our expansion data
    if (!_expansionData)
    {
		_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
		if (!_expansionData)
			return false;
		
		bzero(_expansionData, sizeof(ExpansionData));
    }
	
    return true;
 }



bool
IOUSBRootHubDevice::InitializeCharacteristics()
{
	UInt32			characteristics = kIOUSBHubDeviceIsRootHub;
	
	// since i am the root hub, just check my speed and that will be the bus speed
	if (GetSpeed() == kUSBDeviceSpeedHigh)
		characteristics |= kIOUSBHubDeviceIsOnHighSpeedBus;
		
	SetHubCharacteristics(characteristics);
	return true;
}



bool
IOUSBRootHubDevice::start(IOService *provider)
{
	IOWorkLoop	*wl;
	
	_commandGate = IOCommandGate::commandGate(this, NULL);
	if (!_commandGate)
		return false;
	
	wl = getWorkLoop();
	
	if (!wl || (wl->addEventSource(_commandGate) != kIOReturnSuccess))
	{
		_commandGate->release();
		_commandGate = NULL;
		return false;
	}
	return super::start(provider);
}



void
IOUSBRootHubDevice::stop( IOService *provider )
{
    if ( _commandGate )
    {
		getWorkLoop()->removeEventSource( _commandGate );
        _commandGate->release();
        _commandGate = NULL;
    }
	super::stop(provider);
}



void
IOUSBRootHubDevice::free()
{
    if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
}



IOReturn
IOUSBRootHubDevice::GatedDeviceRequest (OSObject *owner,  void *arg0,  void *arg1,  void *arg2,  void *arg3 )
{
	IOUSBRootHubDevice *me = (IOUSBRootHubDevice*)owner;
	
	if (!me)
		return kIOReturnNotResponding;
	return me->DeviceRequestWorker((IOUSBDevRequest*)arg0, (UInt32)arg1, (UInt32)arg2, (IOUSBCompletion*)arg3);
}



// intercept regular hub requests since the controller simulates the root hub
IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    return DeviceRequest(request, 0, 0, completion);
}



IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
	if (!_commandGate)
		return kIOReturnNotResponding;
		
	if (_myPolicyMaker && (_myPolicyMaker->getPowerState() == kIOUSBHubPowerStateLowPower))
	{
		// this is not usually an issue, but i want to make sure it doesn't become one
		USBLog(5, "IOUSBRootHubDevice[%p]::DeviceRequest - doing a device request while in low power mode - should be OK", this);
	}
	return _commandGate->runAction(GatedDeviceRequest, request, (void*)noDataTimeout, (void*)completionTimeout, completion);
}




IOReturn 
IOUSBRootHubDevice::DeviceRequestWorker(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
	IOReturn	err = 0;
    UInt16		theRequest;
    UInt8		dType, dIndex;

    
    if (!request)
        return(kIOReturnBadArgument);

    theRequest = (request->bRequest << 8) | request->bmRequestType;

    switch (theRequest)
    {
        // Standard Requests
        //
        case kClearDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetDescriptor:
            dType = request->wValue >> 8;
            dIndex = request->wValue & 0x00FF;
            switch (dType) {
                case kUSBDeviceDesc:
                    err = _controller->GetRootHubDeviceDescriptor((IOUSBDeviceDescriptor*)request->pData);
                    request->wLenDone = sizeof(IOUSBDeviceDescriptor);
                    break;

                case kUSBConfDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubConfDescriptor(fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }

                case kUSBStringDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubStringDescriptor((request->wValue & 0x00ff), fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }
                
                default:
                    err = kIOReturnBadArgument;
            }
            break;

        case kGetDeviceStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt16*)(request->pData) = HostToUSBWord(1); // self-powered
                request->wLenDone = 2;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetAddress:
            if (request->wIndex == 0)
                err = _controller->SetHubAddress(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;
                
        case kSetConfiguration:
            if (request->wIndex == 0)
                configuration = request->wValue;
            else
                err = kIOReturnBadArgument;
            break;

        case kSetDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetConfiguration:
            if ((request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt8*)(request->pData) = configuration;
                request->wLenDone = 1;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kClearInterfaceFeature:
        case kClearEndpointFeature:
        case kGetInterface:
        case kGetInterfaceStatus:
        case kGetEndpointStatus:
        case kSetInterfaceFeature:
        case kSetEndpointFeature:
        case kSetDescriptor:
        case kSetInterface:
        case kSyncFrame:
            err = kIOReturnUnsupported;
            break;

        // Class Requests
        //
        case kClearHubFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kClearPortFeature:
            err = _controller->ClearRootHubPortFeature(request->wValue, request->wIndex);
            break;

        case kGetPortState:
            if ((request->wValue == 0) && (request->pData != 0))
                err = _controller->GetRootHubPortState((UInt8 *)request->pData, request->wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubDescriptor:
            if ((request->wValue == ((kUSBHubDescriptorType << 8) + 0)) && (request->pData != 0))
            {
                err = _controller->GetRootHubDescriptor((IOUSBHubDescriptor *)request->pData);
                request->wLenDone = sizeof(IOUSBHubDescriptor);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubStatus((IOUSBHubStatus *)request->pData);
                request->wLenDone = sizeof(IOUSBHubStatus);
            }
            else
                err = kIOReturnBadArgument;
           break;

        case kGetPortStatus:
            if ((request->wValue == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubPortStatus((IOUSBHubPortStatus *)request->pData, request->wIndex);
                request->wLenDone = sizeof(IOUSBHubPortStatus);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubDescriptor:
            if (request->pData != 0)
                err = _controller->SetRootHubDescriptor((OSData *)request->pData);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetPortFeature:
            err = _controller->SetRootHubPortFeature(request->wValue, request->wIndex);
            break;

        default:
            err = kIOReturnBadArgument;

    }
    return(err);
}


bool
IOUSBRootHubDevice::IsRootHub(void)
{
	return true;
}

UInt32
IOUSBRootHubDevice::RequestExtraPower(UInt32 requestedPower)
{
	IOUSBControllerV3	*v3Bus = OSDynamicCast(IOUSBControllerV3, GetBus());
	UInt32				ret = 0;
	
	if (v3Bus)
	{
		ret = v3Bus->AllocateExtraRootHubPortPower(requestedPower);
	}
	USBLog(2, "IOUSBRootHubDevice[%p]::RequestExtraPower - requested (%d) returning (%d)", this, (int)requestedPower, (int)ret);
	return ret;
}



void
IOUSBRootHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
	IOUSBControllerV3	*v3Bus = OSDynamicCast(IOUSBControllerV3, GetBus());
	if (v3Bus)
	{
		USBLog(2, "IOUSBRootHubDevice[%p]::ReturnExtraPower - returning (%d) to controller", this, (int)returnedPower);
		v3Bus->ReturnExtraRootHubPortPower(returnedPower);
	}
	
	return;
}



OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  0);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  1);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  2);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  3);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  4);


