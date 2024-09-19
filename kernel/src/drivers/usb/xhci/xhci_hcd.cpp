#include "xhci_hcd.h"
#include <acpi/mcfg.h>
#include <time/ktime.h>
#include <kprint.h>

#include "xhci_ext_cap.h"

const char* trbCompletionCodeToString(uint8_t completionCode) {
    switch (completionCode) {
    case XHCI_TRB_COMPLETION_CODE_INVALID:
        return "INVALID";
    case XHCI_TRB_COMPLETION_CODE_SUCCESS:
        return "SUCCESS";
    case XHCI_TRB_COMPLETION_CODE_DATA_BUFFER_ERROR:
        return "DATA_BUFFER_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BABBLE_DETECTED_ERROR:
        return "BABBLE_DETECTED_ERROR";
    case XHCI_TRB_COMPLETION_CODE_USB_TRANSACTION_ERROR:
        return "USB_TRANSACTION_ERROR";
    case XHCI_TRB_COMPLETION_CODE_TRB_ERROR:
        return "TRB_ERROR";
    case XHCI_TRB_COMPLETION_CODE_STALL_ERROR:
        return "STALL_ERROR";
    case XHCI_TRB_COMPLETION_CODE_RESOURCE_ERROR:
        return "RESOURCE_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BANDWIDTH_ERROR:
        return "BANDWIDTH_ERROR";
    case XHCI_TRB_COMPLETION_CODE_NO_SLOTS_AVAILABLE:
        return "NO_SLOTS_AVAILABLE";
    case XHCI_TRB_COMPLETION_CODE_INVALID_STREAM_TYPE:
        return "INVALID_STREAM_TYPE";
    case XHCI_TRB_COMPLETION_CODE_SLOT_NOT_ENABLED:
        return "SLOT_NOT_ENABLED";
    case XHCI_TRB_COMPLETION_CODE_ENDPOINT_NOT_ENABLED:
        return "ENDPOINT_NOT_ENABLED";
    case XHCI_TRB_COMPLETION_CODE_SHORT_PACKET:
        return "SHORT_PACKET";
    case XHCI_TRB_COMPLETION_CODE_RING_UNDERRUN:
        return "RING_UNDERRUN";
    case XHCI_TRB_COMPLETION_CODE_RING_OVERRUN:
        return "RING_OVERRUN";
    case XHCI_TRB_COMPLETION_CODE_VF_EVENT_RING_FULL:
        return "VF_EVENT_RING_FULL";
    case XHCI_TRB_COMPLETION_CODE_PARAMETER_ERROR:
        return "PARAMETER_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BANDWIDTH_OVERRUN:
        return "BANDWIDTH_OVERRUN";
    case XHCI_TRB_COMPLETION_CODE_CONTEXT_STATE_ERROR:
        return "CONTEXT_STATE_ERROR";
    case XHCI_TRB_COMPLETION_CODE_NO_PING_RESPONSE:
        return "NO_PING_RESPONSE";
    case XHCI_TRB_COMPLETION_CODE_EVENT_RING_FULL:
        return "EVENT_RING_FULL";
    case XHCI_TRB_COMPLETION_CODE_INCOMPATIBLE_DEVICE:
        return "INCOMPATIBLE_DEVICE";
    case XHCI_TRB_COMPLETION_CODE_MISSED_SERVICE:
        return "MISSED_SERVICE";
    case XHCI_TRB_COMPLETION_CODE_COMMAND_RING_STOPPED:
        return "COMMAND_RING_STOPPED";
    case XHCI_TRB_COMPLETION_CODE_COMMAND_ABORTED:
        return "COMMAND_ABORTED";
    case XHCI_TRB_COMPLETION_CODE_STOPPED:
        return "STOPPED";
    case XHCI_TRB_COMPLETION_CODE_STOPPED_LENGTH_INVALID:
        return "STOPPED_LENGTH_INVALID";
    case XHCI_TRB_COMPLETION_CODE_STOPPED_SHORT_PACKET:
        return "STOPPED_SHORT_PACKET";
    case XHCI_TRB_COMPLETION_CODE_MAX_EXIT_LATENCY_ERROR:
        return "MAX_EXIT_LATENCY_ERROR";
    default:
        return "UNKNOWN_COMPLETION_CODE";
    }
}

void dumpDeviceContext32(const XhciDeviceContext32* context) {
    // Dump Slot Context
    kprint("\nSlot Context:\n");
    kprint("  Route String:           0x%x\n", context->slotContext.routeString);
    kprint("  Speed:                  0x%x\n", context->slotContext.speed);
    kprint("  Reserved:               0x%x\n", context->slotContext.rz);
    kprint("  MTT:                    0x%x\n", context->slotContext.mtt);
    kprint("  Hub:                    0x%x\n", context->slotContext.hub);
    kprint("  Context Entries:        0x%x\n", context->slotContext.contextEntries);
    kprint("  Max Exit Latency:       0x%x\n", context->slotContext.maxExitLatency);
    kprint("  Root Hub Port Number:   0x%x\n", context->slotContext.rootHubPortNum);
    kprint("  Port Count:             0x%x\n", context->slotContext.portCount);
    kprint("  Parent Hub Slot ID:     0x%x\n", context->slotContext.parentHubSlotId);
    kprint("  Parent Port Number:     0x%x\n", context->slotContext.parentPortNumber);
    kprint("  TT Think Time:          0x%x\n", context->slotContext.ttThinkTime);
    kprint("  Reserved0:              0x%x\n", context->slotContext.rsvd0);
    kprint("  Interrupter Target:     0x%x\n", context->slotContext.interrupterTarget);
    kprint("  Device Address:         0x%x\n", context->slotContext.deviceAddress);
    kprint("  Reserved1:              0x%x\n", context->slotContext.rsvd1);
    kprint("  Slot State:             0x%x\n", context->slotContext.slotState);

    // Reserved fields
    kprint("  Reserved Fields (Slot Context): [ ");
    for (int i = 0; i < 4; ++i) {
        kprint("0x%x ", context->slotContext.rsvdZ[i]);
    }
    kprint("]\n");

    // Dump Control Endpoint Context
    kprint("\nControl Endpoint Context:\n");
    kprint("  Endpoint State:         0x%x\n", context->controlEndpointContext.endpointState);
    kprint("  Mult:                   0x%x\n", context->controlEndpointContext.mult);
    kprint("  Max Primary Streams:    0x%x\n", context->controlEndpointContext.maxPrimaryStreams);
    kprint("  Linear Stream Array:    0x%x\n", context->controlEndpointContext.linearStreamArray);
    kprint("  Interval:               0x%x\n", context->controlEndpointContext.interval);
    kprint("  Max ESIT Payload Hi:    0x%x\n", context->controlEndpointContext.maxEsitPayloadHi);
    kprint("  Error Count:            0x%x\n", context->controlEndpointContext.errorCount);
    kprint("  Endpoint Type:          0x%x\n", context->controlEndpointContext.endpointType);
    kprint("  Host Initiate Disable:  0x%x\n", context->controlEndpointContext.hostInitiateDisable);
    kprint("  Max Burst Size:         0x%x\n", context->controlEndpointContext.maxBurstSize);
    kprint("  Max Packet Size:        0x%x\n", context->controlEndpointContext.maxPacketSize);
    kprint("  Dequeue Cycle State:    0x%llx\n", context->controlEndpointContext.dcs);
    kprint("  TR Dequeue Ptr:         0x%llx\n", context->controlEndpointContext.trDequeuePtrAddressBits);
    kprint("  Average TRB Length:     0x%x\n", context->controlEndpointContext.averageTrbLength);
    kprint("  Max ESIT Payload Lo:    0x%x\n", context->controlEndpointContext.maxEsitPayloadLo);

    // Reserved fields (Control Endpoint Context)
    kprint("  Reserved Fields (Control Endpoint Context): [ ");
    for (int i = 0; i < 0; ++i) {
        kprint("0x%x ", context->controlEndpointContext.rsvd[i]);
    }
    kprint("]\n");

    // Dump each Endpoint Context (ep[0] to ep[29])
    for (int epIndex = 0; epIndex < 30; ++epIndex) {
        if (context->ep[epIndex].endpointState == XHCI_ENDPOINT_STATE_DISABLED) {
            continue;
        }

        kprint("\nEndpoint Context %i:\n", epIndex);
        kprint("  Endpoint State:         0x%x\n", context->ep[epIndex].endpointState);
        kprint("  Mult:                   0x%x\n", context->ep[epIndex].mult);
        kprint("  Max Primary Streams:    0x%x\n", context->ep[epIndex].maxPrimaryStreams);
        kprint("  Linear Stream Array:    0x%x\n", context->ep[epIndex].linearStreamArray);
        kprint("  Interval:               0x%x\n", context->ep[epIndex].interval);
        kprint("  Max ESIT Payload Hi:    0x%x\n", context->ep[epIndex].maxEsitPayloadHi);
        kprint("  Error Count:            0x%x\n", context->ep[epIndex].errorCount);
        kprint("  Endpoint Type:          0x%x\n", context->ep[epIndex].endpointType);
        kprint("  Host Initiate Disable:  0x%x\n", context->ep[epIndex].hostInitiateDisable);
        kprint("  Max Burst Size:         0x%x\n", context->ep[epIndex].maxBurstSize);
        kprint("  Max Packet Size:        0x%x\n", context->ep[epIndex].maxPacketSize);
        kprint("  Dequeue Cycle State:    0x%llx\n", context->ep[epIndex].dcs);
        kprint("  TR Dequeue Ptr:         0x%llx\n", context->ep[epIndex].trDequeuePtrAddressBits);
        kprint("  Average TRB Length:     0x%x\n", context->ep[epIndex].averageTrbLength);
        kprint("  Max ESIT Payload Lo:    0x%x\n", context->ep[epIndex].maxEsitPayloadLo);
    }
}

void dumpXhciInputContext32(const XhciInputContext32* context) {
    // Dump Input Control Context
    kprint("Input Control Context:\n");
    kprint("  Drop Flags:             0x%x\n", context->controlContext.dropFlags);
    kprint("  Add Flags:              0x%x\n", context->controlContext.addFlags);
    kprint("  Reserved0:              [ ");
    for (int i = 0; i < 5; ++i) {
        kprint("0x%x ", context->controlContext.rsvd0[i]);
    }
    kprint("]\n");
    kprint("  Config Value:           0x%x\n", context->controlContext.configValue);
    kprint("  Interface Number:       0x%x\n", context->controlContext.interfaceNumber);
    kprint("  Alternate Setting:      0x%x\n", context->controlContext.alternateSetting);
    kprint("  Reserved1:              0x%x\n", context->controlContext.rsvd1);

    dumpDeviceContext32(&context->deviceContext);
}

void XhciHcd::init(PciDeviceInfo* deviceInfo) {
    uint64_t xhcBase = xhciMapMmio(deviceInfo->barAddress);

    // Create the host controller context data structure
    m_ctx = kstl::SharedPtr<XhciHcContext>(new XhciHcContext(xhcBase));

    // Create a device context manager
    m_deviceContextManager = kstl::SharedPtr<XhciDeviceContextManager>(new XhciDeviceContextManager());

    // Map which port register sets are USB2 and which are USB3
    _identifyUsb3Ports();

    // Reset the controller's internal state
    if (!resetController()) {
        // TO-DO: handle error appropriately
        return;
    }

    // Configure operational registers
    _configureOperationalRegs();

    // Setup the event ring and interrupter registers
    m_ctx->eventRing = kstl::SharedPtr<XhciEventRing>(
        new XhciEventRing(XHCI_EVENT_RING_TRB_COUNT, &m_ctx->runtimeRegs->ir[0])
    );

    kprint("[XHCI] DCBAAP   : 0x%llx\n", m_ctx->opRegs->dcbaap);
    kprint("[XHCI] CRCR     : 0x%llx\n", m_ctx->opRegs->crcr);
    kprint("[XHCI] ERSTSZ   : %lli\n", m_ctx->runtimeRegs->ir[0].erstsz);
    kprint("[XHCI] ERDP     : 0x%llx\n", m_ctx->runtimeRegs->ir[0].erdp);
    kprint("[XHCI] ERSTBA   : 0x%llx\n", m_ctx->runtimeRegs->ir[0].erstba);
    kprint("\n");

    // Start the controller
    startController();

    // Reset the ports
    resetAllPorts();

    // After port resets, there will be extreneous port state change events
    // for ports with connected devices, but without CSC bit set, so we have
    // to manually iterate the ports with connected devices and set them up.
    m_ctx->eventRing->flushUnprocessedEvents();
    
    // Clear the interrupt pending flags
    clearIrqFlags(0);

    for (uint8_t port = 0; port < m_ctx->getMaxPorts(); ++port) {
        auto portRegisterSet = m_ctx->getPortRegisterSet(port);
        XhciPortscRegister portsc;
        portRegisterSet.readPortscReg(portsc);

        if (portsc.ccs) {
            // Port number has to be 1-indexed
            // in the device setup routine.
            _setupDevice(port + 1);
            
            // For debugging purposes
            break;
        }
    }
}

bool XhciHcd::resetController() {
    // Make sure we clear the Run/Stop bit
    uint32_t usbcmd = m_ctx->opRegs->usbcmd;
    usbcmd &= ~XHCI_USBCMD_RUN_STOP;
    m_ctx->opRegs->usbcmd = usbcmd;

    // Wait for the HCHalted bit to be set
    uint32_t timeout = 20;
    while (!(m_ctx->opRegs->usbsts & XHCI_USBSTS_HCH)) {
        if (--timeout == 0) {
            kprint("XHCI HC did not halt within %ims\n", timeout);
            return false;
        }

        msleep(1);
    }

    // Set the HC Reset bit
    usbcmd = m_ctx->opRegs->usbcmd;
    usbcmd |= XHCI_USBCMD_HCRESET;
    m_ctx->opRegs->usbcmd = usbcmd;

    // Wait for this bit and CNR bit to clear
    timeout = 100;
    while (
        m_ctx->opRegs->usbcmd & XHCI_USBCMD_HCRESET ||
        m_ctx->opRegs->usbsts & XHCI_USBSTS_CNR
    ) {
        if (--timeout == 0) {
            kprint("XHCI HC did not reset within %ims\n", timeout);
            return false;
        }

        msleep(1);
    }

    msleep(50);

    // Check the defaults of the operational registers
    if (m_ctx->opRegs->usbcmd != 0)
        return false;

    if (m_ctx->opRegs->dnctrl != 0)
        return false;

    if (m_ctx->opRegs->crcr != 0)
        return false;

    if (m_ctx->opRegs->dcbaap != 0)
        return false;

    if (m_ctx->opRegs->config != 0)
        return false;

    return true;
}

void XhciHcd::startController() {
    uint32_t usbcmd = m_ctx->opRegs->usbcmd;
    usbcmd |= XHCI_USBCMD_RUN_STOP;
    usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
    usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;

    m_ctx->opRegs->usbcmd = usbcmd;

    // Make sure the controller's HCH flag is cleared
    while (m_ctx->opRegs->usbsts & XHCI_USBSTS_HCH) {
        msleep(16);
    }
}

bool XhciHcd::resetPort(uint8_t port) {
    XhciPortRegisterManager regset = m_ctx->getPortRegisterSet(port);
    XhciPortscRegister portsc;
    regset.readPortscReg(portsc);

    bool isUsb3Port = m_ctx->isPortUsb3(port);

    if (portsc.pp == 0) {
        portsc.pp = 1;
        regset.writePortscReg(portsc);
        msleep(20);
        regset.readPortscReg(portsc);

        if (portsc.pp == 0) {
            kprintWarn("Port %i: Bad Reset\n", port);
            return false;
        }
    }

    // Clear connect status change bit by writing a '1' to it
    portsc.csc = 1;
    regset.writePortscReg(portsc);

    // Write to the appropriate reset bit
    if (isUsb3Port) {
        portsc.wpr = 1;
    } else {
        portsc.pr = 1;
    }
    portsc.ped = 0;
    regset.writePortscReg(portsc);

    int timeout = 60;
    while (timeout) {
        regset.readPortscReg(portsc);

        // Detect port reset change bit to be set
        if (isUsb3Port && portsc.wrc) {
            break;
        } else if (!isUsb3Port && portsc.prc) {
            break;
        }

        timeout--;
        msleep(1);
    }

    if (timeout > 0) {
        msleep(3);
        regset.readPortscReg(portsc);

        // Check for the port enable/disable bit
        // to be set and indicate 'enabled' state.
        if (portsc.ped) {
            // Clear connect status change bit by writing a '1' to it
            portsc.csc = 1;
            regset.writePortscReg(portsc);
            return true;
        }
    }

    return false; 
}

void XhciHcd::resetAllPorts() {
    for (uint8_t i = 0; i < m_ctx->getMaxPorts(); i++) {
        if (resetPort(i)) {
            kprintInfo("[*] Successfully reset %s port %i\n", m_ctx->isPortUsb3(i) ? "USB3" : "USB2", i);
        } else {
            kprintWarn("[*] Failed to reset %s port %i\n", m_ctx->isPortUsb3(i) ? "USB3" : "USB2", i);
        }
    }
    kprint("\n");
}

void XhciHcd::clearIrqFlags(uint8_t interrupter) {
    // Get the interrupter registers
    auto interrupterRegs = &m_ctx->runtimeRegs->ir[interrupter];

    // Clear the interrupt pending bit in the primary interrupter
    interrupterRegs->iman |= ~XHCI_IMAN_INTERRUPT_PENDING;

    // Clear the interrupt pending bit in USBSTS
    m_ctx->opRegs->usbsts |= ~XHCI_USBSTS_EINT;
}

XhciCommandCompletionTrb_t* XhciHcd::sendCommand(XhciTrb_t* trb) {
    // Small delay period between ringing the
    // doorbell and polling the event ring.
    const uint32_t commandDelay = 40;

    // Enqueue the TRB
    m_ctx->commandRing->enqueue(trb);

    // Ring the command doorbell
    m_ctx->doorbellManager->ringCommandDoorbell();

    // Let the host controller process the command
    msleep(commandDelay);
    
    // Poll the event ring for the command completion event
    kstl::vector<XhciTrb_t*> events;
    if (m_ctx->eventRing->hasUnprocessedEvents()) {
        m_ctx->eventRing->dequeueEvents(events);
        clearIrqFlags(0);
    }

    XhciCommandCompletionTrb_t* completionTrb = nullptr;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i]->trbType == XHCI_TRB_TYPE_CMD_COMPLETION_EVENT) {
            completionTrb = reinterpret_cast<XhciCommandCompletionTrb_t*>(events[i]);
            break;   
        }
    }

    if (!completionTrb) {
        kprintError("[*] Failed to find completion TRB for command %i\n", trb->trbType);
        return nullptr;
    }

    if (completionTrb->completionCode != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
        kprintError("[*] Command TRB failed with error: %s\n", trbCompletionCodeToString(completionTrb->completionCode));
        return nullptr;
    }

    return completionTrb;
}

void XhciHcd::_logUsbsts() {
    uint32_t status = m_ctx->opRegs->usbsts;
    kprint("===== USBSTS =====\n");
    if (status & XHCI_USBSTS_HCH) kprint("    Host Controlled Halted\n");
    if (status & XHCI_USBSTS_HSE) kprint("    Host System Error\n");
    if (status & XHCI_USBSTS_EINT) kprint("    Event Interrupt\n");
    if (status & XHCI_USBSTS_PCD) kprint("    Port Change Detect\n");
    if (status & XHCI_USBSTS_SSS) kprint("    Save State Status\n");
    if (status & XHCI_USBSTS_RSS) kprint("    Restore State Status\n");
    if (status & XHCI_USBSTS_SRE) kprint("    Save/Restore Error\n");
    if (status & XHCI_USBSTS_CNR) kprint("    Controller Not Ready\n");
    if (status & XHCI_USBSTS_HCE) kprint("    Host Controller Error\n");
    kprint("\n");
}

void XhciHcd::_identifyUsb3Ports() {
    auto node = m_ctx->extendedCapabilitiesHead;
    while (node.get()) {
        if (node->id() == XhciExtendedCapabilityCode::SupportedProtocol) {
            XhciUsbSupportedProtocolCapability cap(node->base());
            // Make the ports zero-based
            uint8_t firstPort = cap.compatiblePortOffset - 1;
            uint8_t lastPort = firstPort + cap.compatiblePortCount - 1;

            if (cap.majorRevisionVersion == 3) {
                for (uint8_t port = firstPort; port <= lastPort; port++) {
                    m_ctx->usb3Ports.pushBack(port);
                }
            }
        }

        node = node->next();
    }
}

void XhciHcd::_configureOperationalRegs() {
    // Enable device notifications 
    m_ctx->opRegs->dnctrl = 0xffff;

    // Configure the usbconfig field
    m_ctx->opRegs->config = static_cast<uint32_t>(m_ctx->getMaxDeviceSlots());

    // Allocate the set the DCBAA pointer
    m_deviceContextManager->allocateDcbaa(m_ctx.get());

    // Write the CRCR register
    m_ctx->opRegs->crcr = m_ctx->commandRing->getPhysicalBase();
}

void XhciHcd::_setupDevice(uint8_t port) {
    auto portRegisterSet = m_ctx->getPortRegisterSet(port);
    XhciPortscRegister portsc;
    portRegisterSet.readPortscReg(portsc);

    kprintInfo("Port State Change Event on port %i: ", port);
    kprint("%s device ATTACHED with speed ", m_ctx->isPortUsb3(port) ? "USB3" : "USB2");

    uint8_t portSpeed = portsc.portSpeed;
    switch (portSpeed) {
    case XHCI_USB_SPEED_FULL_SPEED: kprint("Full Speed (12 MB/s - USB2.0)\n"); break;
    case XHCI_USB_SPEED_LOW_SPEED: kprint("Low Speed (1.5 Mb/s - USB 2.0)\n"); break;
    case XHCI_USB_SPEED_HIGH_SPEED: kprint("High Speed (480 Mb/s - USB 2.0)\n"); break;
    case XHCI_USB_SPEED_SUPER_SPEED: kprint("Super Speed (5 Gb/s - USB3.0)\n"); break;
    case XHCI_USB_SPEED_SUPER_SPEED_PLUS: kprint("Super Speed Plus (10 Gb/s - USB 3.1)\n"); break;
    default: kprint("Undefined\n"); break;
    }

    auto device = new XhciDevice(m_ctx.get(), port);
    device->slotId = _enableSlot();
    if (!device->slotId) {
        kprint("[XHCI] Failed to allocate a slot for device on port %i\n", port);
        return;
    }

    device->setupTransferRing();
    device->setupAddressDeviceCtx(portSpeed);

    // Allocate the output device context entry in the DCBAA slot
    m_deviceContextManager->allocateDeviceContext(m_ctx.get(), device->slotId);

    // Send the Address Device command
    if (!_addressDevice(device)) {
        return;
    }
}

uint8_t XhciHcd::_enableSlot() {
    XhciTrb_t enableSlotTrb = XHCI_CONSTRUCT_CMD_TRB(XHCI_TRB_TYPE_ENABLE_SLOT_CMD);
    auto completionTrb = sendCommand(&enableSlotTrb);

    if (!completionTrb) {
        return 0;
    }

    return completionTrb->slotId;
}

bool XhciHcd::_addressDevice(XhciDevice* device) {
    // Construct the Address Device TRB
    XhciAddressDeviceCommandTrb_t addressDeviceTrb;
    zeromem(&addressDeviceTrb, sizeof(XhciAddressDeviceCommandTrb_t));
    addressDeviceTrb.trbType = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
    addressDeviceTrb.inputContextPhysicalBase = device->getInputContextPhysicalBase();
    addressDeviceTrb.bsr = 0;
    addressDeviceTrb.slotId = device->slotId;

    // Send the AddressDevice command
    auto completionTrb = sendCommand((XhciTrb_t*)&addressDeviceTrb);
    if (!completionTrb || completionTrb->completionCode != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
        kprintError("[XHCI] Failed to complete the first Device Address command\n");
        return false;
    }

    kprintInfo("[*] Successfully issued the first Device Address command!\n");
    msleep(200);

    return true;
}
