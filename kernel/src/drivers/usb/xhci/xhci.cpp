#include "xhci.h"
#include <paging/page.h>
#include <paging/phys_addr_translation.h>
#include <paging/tlb.h>
#include <memory/kmemory.h>
#include <time/ktime.h>
#include <arch/x86/ioapic.h>
#include <interrupts/interrupts.h>
#include <kprint.h>

bool XhciDriver::init(PciDeviceInfo& deviceInfo) {
    kprint("[XHCI] Initialize xHci Driver 3.0\n\n");

    m_xhcBase = xhciMapMmio(deviceInfo.barAddress);

    // Parse the read-only capability register space
    _parseCapabilityRegisters();
    _logCapabilityRegisters();

    // Parse the extended capabilities
    _parseExtendedCapabilityRegisters();

    // Reset the controller
    if (!_resetHostController()) {
        return false;
    }

    // Configure the controller's register spaces
    _configureOperationalRegisters();
    _configureRuntimeRegisters();

    // At this point the controller is all setup so we can start it
    _startHostController();

    // Perform an initial port reset for each port
    for (uint8_t i = 0; i < m_maxPorts; i++) {
        if (_resetPort(i)) {
            kprintInfo("[*] Successfully reset %s port %i\n", _isUSB3Port(i) ? "USB3" : "USB2", i);
        }
    }
    kprint("\n");

    // DEBUG - setup device at port 4
    _setupDevice(4);

    // const uint8_t testPort = 4; // TEST USB DEVICE

    // // After port resets, there will be extreneous port state change events
    // // for ports with connected devices, but without CSC bit set, so we have
    // // to manually iterate the ports with connected devices and set them up.
    // m_eventRing->flushUnprocessedEvents();

    // for (uint8_t port = 0; port < m_maxPorts; ++port) {
    //     auto portRegisterSet = _getPortRegisterSet(port);
    //     XhciPortscRegister portsc;
    //     portRegisterSet.readPortscReg(portsc);

    //     if (portsc.ccs) {
    //         _handleDeviceConnected(port);
    //     }
    // }

    // kstl::vector<XhciTrb_t*> eventTrbs;
    // while (true) {
    //    eventTrbs.clear();
    //     if (m_eventRing->hasUnprocessedEvents()) {
    //         m_eventRing->dequeueEvents(eventTrbs);
    //         _markXhciInterruptCompleted(0);
    //     }

    //     // Process the TRBs
    //     for (size_t i = 0; i < eventTrbs.size(); ++i) {
    //         _processEventRingTrb(eventTrbs[i]);
    //     }
    // }

    // kprint("\n");
    return true;
}

void XhciDriver::_parseCapabilityRegisters() {
    m_capRegs = reinterpret_cast<volatile XhciCapabilityRegisters*>(m_xhcBase);

    m_capabilityRegsLength = m_capRegs->caplength;

    m_maxDeviceSlots = XHCI_MAX_DEVICE_SLOTS(m_capRegs);
    m_maxInterrupters = XHCI_MAX_INTERRUPTERS(m_capRegs);
    m_maxPorts = XHCI_MAX_PORTS(m_capRegs);

    m_isochronousSchedulingThreshold = XHCI_IST(m_capRegs);
    m_erstMax = XHCI_ERST_MAX(m_capRegs);
    m_maxScratchpadBuffers = XHCI_MAX_SCRATCHPAD_BUFFERS(m_capRegs);

    m_64bitAddressingCapability = XHCI_AC64(m_capRegs);
    m_bandwidthNegotiationCapability = XHCI_BNC(m_capRegs);
    m_64ByteContextSize = XHCI_CSZ(m_capRegs);
    m_portPowerControl = XHCI_PPC(m_capRegs);
    m_portIndicators = XHCI_PIND(m_capRegs);
    m_lightResetCapability = XHCI_LHRC(m_capRegs);
    m_extendedCapabilitiesOffset = XHCI_XECP(m_capRegs) * sizeof(uint32_t);

    // Update the base pointer to operational register set
    m_opRegs = reinterpret_cast<volatile XhciOperationalRegisters*>(m_xhcBase + m_capabilityRegsLength);

    // Construct a manager class instance for the doorbell register array
    m_doorbellManager = kstl::SharedPtr<XhciDoorbellManager>(
        new XhciDoorbellManager(m_xhcBase + m_capRegs->dboff)
    );

    // Construct a controller class instance for the runtime register set
    uint64_t runtimeRegisterBase = m_xhcBase + m_capRegs->rtsoff;
    m_runtimeRegisterManager = kstl::SharedPtr<XhciRuntimeRegisterManager>(
        new XhciRuntimeRegisterManager(runtimeRegisterBase, m_maxInterrupters)
    );
}

void XhciDriver::_logCapabilityRegisters() {
    kprintInfo("===== Capability Registers (0x%llx) =====\n", (uint64_t)m_capRegs);
    kprintInfo("    Length                : %i\n", m_capabilityRegsLength);
    kprintInfo("    Max Device Slots      : %i\n", m_maxDeviceSlots);
    kprintInfo("    Max Interrupters      : %i\n", m_maxInterrupters);
    kprintInfo("    Max Ports             : %i\n", m_maxPorts);
    kprintInfo("    IST                   : %i\n", m_isochronousSchedulingThreshold);
    kprintInfo("    ERST Max Size         : %i\n", m_erstMax);
    kprintInfo("    Scratchpad Buffers    : %i\n", m_maxScratchpadBuffers);
    kprintInfo("    64-bit Addressing     : %i\n", m_64bitAddressingCapability);
    kprintInfo("    Bandwidth Negotiation : %i\n", m_bandwidthNegotiationCapability);
    kprintInfo("    64-byte Context Size  : %i\n", m_64ByteContextSize);
    kprintInfo("    Port Power Control    : %i\n", m_portPowerControl);
    kprintInfo("    Port Indicators       : %i\n", m_portIndicators);
    kprintInfo("    Light Reset Available : %i\n", m_lightResetCapability);
    kprint("\n");
}

void XhciDriver::_parseExtendedCapabilityRegisters() {
    volatile uint32_t* headCapPtr = reinterpret_cast<volatile uint32_t*>(
        m_xhcBase + m_extendedCapabilitiesOffset
    );

    m_extendedCapabilitiesHead = kstl::SharedPtr<XhciExtendedCapability>(
        new XhciExtendedCapability(headCapPtr)
    );

    auto node = m_extendedCapabilitiesHead;
    while (node.get()) {
        if (node->id() == XhciExtendedCapabilityCode::SupportedProtocol) {
            XhciUsbSupportedProtocolCapability cap(node->base());
            // Make the ports zero-based
            uint8_t firstPort = cap.compatiblePortOffset - 1;
            uint8_t lastPort = firstPort + cap.compatiblePortCount - 1;

            if (cap.majorRevisionVersion == 3) {
                for (uint8_t port = firstPort; port <= lastPort; port++) {
                    m_usb3Ports.pushBack(port);
                }
            }
        }
        node = node->next();
    }
}

void XhciDriver::_configureOperationalRegisters() {
    // Establish host controller's supported page size in bytes
    m_hcPageSize = static_cast<uint64_t>(m_opRegs->pagesize & 0xffff) << 12;
    
    // Enable device notifications 
    m_opRegs->dnctrl = 0xffff;

    // Configure the usbconfig field
    m_opRegs->config = static_cast<uint32_t>(m_maxDeviceSlots);

    // Setup device context base address array and scratchpad buffers
    _setupDcbaa();

    // Setup the command ring and write CRCR
    m_commandRing = kstl::SharedPtr<XhciCommandRing>(
        new XhciCommandRing(XHCI_COMMAND_RING_TRB_COUNT)
    );
    m_opRegs->crcr = m_commandRing->getPhysicalBase() | m_commandRing->getCycleBit();
}

void XhciDriver::_logUsbsts() {
    uint32_t status = m_opRegs->usbsts;
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

void XhciDriver::_logOperationalRegisters() {
    kprintInfo("===== Operational Registers (0x%llx) =====\n", (uint64_t)m_opRegs);
    kprintInfo("    usbcmd     : %x\n", m_opRegs->usbcmd);
    kprintInfo("    usbsts     : %x\n", m_opRegs->usbsts);
    kprintInfo("    pagesize   : %x\n", m_opRegs->pagesize);
    kprintInfo("    dnctrl     : %x\n", m_opRegs->dnctrl);
    kprintInfo("    crcr       : %llx\n", m_opRegs->crcr);
    kprintInfo("    dcbaap     : %llx\n", m_opRegs->dcbaap);
    kprintInfo("    config     : %x\n", m_opRegs->config);
    kprint("\n");
}

uint8_t XhciDriver::_getPortSpeed(uint8_t port) {
    auto portRegisterSet = _getPortRegisterSet(port);
    XhciPortscRegister portsc;
    portRegisterSet.readPortscReg(portsc);

    return static_cast<uint8_t>(portsc.portSpeed);
}

const char* XhciDriver::_usbSpeedToString(uint8_t speed) {
    static const char* speedString[7] = {
        "Invalid",
        "Full Speed (12 MB/s - USB2.0)",
        "Low Speed (1.5 Mb/s - USB 2.0)",
        "High Speed (480 Mb/s - USB 2.0)",
        "Super Speed (5 Gb/s - USB3.0)",
        "Super Speed Plus (10 Gb/s - USB 3.1)",
        "Undefined"
    };

    return speedString[speed];
}

void XhciDriver::_configureRuntimeRegisters() {
    // Get the primary interrupter registers
    auto interrupterRegs = m_runtimeRegisterManager->getInterrupterRegisters(0);
    if (!interrupterRegs) {
        kprintError("[*] Failed to retrieve interrupter register set when setting up the event ring!");
        return;
    }

    // Setup the event ring and write to interrupter
    // registers to set ERSTSZ, ERSDP, and ERSTBA.
    m_eventRing = kstl::SharedPtr<XhciEventRing>(
        new XhciEventRing(XHCI_EVENT_RING_TRB_COUNT, interrupterRegs)
    );

    // Clear any pending interrupts for primary interrupter
    _clearIrqFlags(0);
}

bool XhciDriver::_isUSB3Port(uint8_t portNum) {
    for (size_t i = 0; i < m_usb3Ports.size(); ++i) {
        if (m_usb3Ports[i] == portNum) {
            return true;
        }
    }

    return false;
}

XhciPortRegisterManager XhciDriver::_getPortRegisterSet(uint8_t portNum) {
    uint64_t base = reinterpret_cast<uint64_t>(m_opRegs) + (0x400 + (0x10 * portNum));
    return XhciPortRegisterManager(base);
}

void XhciDriver::_setupDcbaa() {
    size_t contextEntrySize = m_64ByteContextSize ? 64 : 32;
    size_t dcbaaSize = contextEntrySize * (m_maxDeviceSlots + 1);

    m_dcbaa = (uint64_t*)allocXhciMemory(dcbaaSize, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY);
    zeromem(m_dcbaa, dcbaaSize);

    /*
    // xHci Spec Section 6.1 (page 404)

    If the Max Scratchpad Buffers field of the HCSPARAMS2 register is > ‘0’, then
    the first entry (entry_0) in the DCBAA shall contain a pointer to the Scratchpad
    Buffer Array. If the Max Scratchpad Buffers field of the HCSPARAMS2 register is
    = ‘0’, then the first entry (entry_0) in the DCBAA is reserved and shall be
    cleared to ‘0’ by software.
    */

    // Initialize scratchpad buffer array if needed
    if (m_maxScratchpadBuffers > 0) {
        uint64_t* scratchpadArray = (uint64_t*)allocXhciMemory(m_maxScratchpadBuffers * sizeof(uint64_t));
        
        // Create scratchpad pages
        for (uint8_t i = 0; i < m_maxScratchpadBuffers; i++) {
            void* scratchpad = allocXhciMemory(PAGE_SIZE, XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT, XHCI_SCRATCHPAD_BUFFERS_BOUNDARY);
            uint64_t scratchpadPhysicalBase = physbase(scratchpad);

            scratchpadArray[i] = scratchpadPhysicalBase;
        }

        uint64_t scratchpadArrayPhysicalBase = physbase(scratchpadArray);

        // Set the first slot in the DCBAA to point to the scratchpad array
        m_dcbaa[0] = scratchpadArrayPhysicalBase;
    }

    // Set DCBAA pointer in the operational registers
    uint64_t dcbaaPhysicalBase = physbase(m_dcbaa);

    m_opRegs->dcbaap = dcbaaPhysicalBase;
}

void XhciDriver::_createDeviceContext(uint8_t slotId) {
    // Determine the size of the device context
    // based on the capability register parameters.
    uint64_t deviceContextSize = m_64ByteContextSize ? sizeof(XhciDeviceContext64) : sizeof(XhciDeviceContext32);

    // Allocate a memory block for the device context
    void* ctx = allocXhciMemory(
        deviceContextSize,
        XHCI_DEVICE_CONTEXT_ALIGNMENT,
        XHCI_DEVICE_CONTEXT_BOUNDARY
    );

    if (!ctx) {
        kprintError("[*] CRITICAL FAILURE: Failed to allocate memory for a device context\n");
        return;
    }

    // Insert the device context's physical address
    // into the Device Context Base Addres Array (DCBAA).
    m_dcbaa[slotId] = physbase(ctx);
}

XhciCommandCompletionTrb_t* XhciDriver::_sendCommand(XhciTrb_t* trb, uint32_t timeoutMs) {
    // Enqueue the TRB
    m_commandRing->enqueue(trb);

    // Ring the command doorbell
    m_doorbellManager->ringCommandDoorbell();

    // Let the host controller process the command
    msleep(timeoutMs);
    
    // Poll the event ring for the command completion event
    kstl::vector<XhciTrb_t*> events;
    if (m_eventRing->hasUnprocessedEvents()) {
        m_eventRing->dequeueEvents(events);
        _clearIrqFlags(0);
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

void XhciDriver::_clearIrqFlags(uint8_t interrupter) {
    // Get the interrupter registers
    XhciInterrupterRegisters* interrupterRegs = m_runtimeRegisterManager->getInterrupterRegisters(interrupter);

    // Clear the interrupt pending bit in the primary interrupter
    interrupterRegs->iman |= ~XHCI_IMAN_INTERRUPT_PENDING;

    // Clear the interrupt pending bit in USBSTS
    m_opRegs->usbsts |= ~XHCI_USBSTS_EINT;
}

uint16_t XhciDriver::_getMaxInitialPacketSize(uint8_t portSpeed) {
    // Calculate initial max packet size for the set device command
    uint16_t initialMaxPacketSize = 0;
    switch (portSpeed) {
    case XHCI_USB_SPEED_LOW_SPEED: initialMaxPacketSize = 8; break;

    case XHCI_USB_SPEED_FULL_SPEED:
    case XHCI_USB_SPEED_HIGH_SPEED: initialMaxPacketSize = 64; break;

    case XHCI_USB_SPEED_SUPER_SPEED:
    case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
    default: initialMaxPacketSize = 512; break;
    }

    return initialMaxPacketSize;
}

bool XhciDriver::_resetHostController() {
    // Make sure we clear the Run/Stop bit
    uint32_t usbcmd = m_opRegs->usbcmd;
    usbcmd &= ~XHCI_USBCMD_RUN_STOP;
    m_opRegs->usbcmd = usbcmd;

    // Wait for the HCHalted bit to be set
    uint32_t timeout = 20;
    while (!(m_opRegs->usbsts & XHCI_USBSTS_HCH)) {
        if (--timeout == 0) {
            kprint("XHCI HC did not halt within %ims\n", timeout);
            return false;
        }

        msleep(1);
    }

    // Set the HC Reset bit
    usbcmd = m_opRegs->usbcmd;
    usbcmd |= XHCI_USBCMD_HCRESET;
    m_opRegs->usbcmd = usbcmd;

    // Wait for this bit and CNR bit to clear
    timeout = 100;
    while (
        m_opRegs->usbcmd & XHCI_USBCMD_HCRESET ||
        m_opRegs->usbsts & XHCI_USBSTS_CNR
    ) {
        if (--timeout == 0) {
            kprint("XHCI HC did not reset within %ims\n", timeout);
            return false;
        }

        msleep(1);
    }

    msleep(50);

    // Check the defaults of the operational registers
    if (m_opRegs->usbcmd != 0)
        return false;

    if (m_opRegs->dnctrl != 0)
        return false;

    if (m_opRegs->crcr != 0)
        return false;

    if (m_opRegs->dcbaap != 0)
        return false;

    if (m_opRegs->config != 0)
        return false;

    return true;
}

void XhciDriver::_startHostController() {
    uint32_t usbcmd = m_opRegs->usbcmd;
    usbcmd |= XHCI_USBCMD_RUN_STOP;
    usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
    usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;

    m_opRegs->usbcmd = usbcmd;

    // Make sure the controller's HCH flag is cleared
    while (m_opRegs->usbsts & XHCI_USBSTS_HCH) {
        msleep(16);
    }
}

bool XhciDriver::_resetPort(uint8_t portNum) {
    XhciPortRegisterManager regset = _getPortRegisterSet(portNum);
    XhciPortscRegister portsc;
    regset.readPortscReg(portsc);

    bool isUsb3Port = _isUSB3Port(portNum);

    if (portsc.pp == 0) {
        portsc.pp = 1;
        regset.writePortscReg(portsc);
        msleep(20);
        regset.readPortscReg(portsc);

        if (portsc.pp == 0) {
            kprintWarn("Port %i: Bad Reset\n", portNum);
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

    int timeout = 100;
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

uint8_t XhciDriver::_enableDeviceSlot() {
    XhciTrb_t enableSlotTrb = XHCI_CONSTRUCT_CMD_TRB(XHCI_TRB_TYPE_ENABLE_SLOT_CMD);
    auto completionTrb = _sendCommand(&enableSlotTrb);

    if (!completionTrb) {
        return 0;
    }

    return completionTrb->slotId;
}

void XhciDriver::_setupDevice(uint8_t port) {
    XhciDevice* device = new XhciDevice();
    device->portRegSet = port;
    device->portNumber = port + 1;
    device->speed = _getPortSpeed(port);

    kprintInfo("Setting up %s device on port %i (portReg:%i)\n", _usbSpeedToString(device->speed), device->portNumber, port);

    device->slotId = _enableDeviceSlot();
    if (!device->slotId) {
        kprintError("[XHCI] Failed to setup device\n");
        delete device;
        return;
    }

    kprintInfo("Device slotId: %i\n", device->slotId);
    _createDeviceContext(device->slotId);

    // Allocate space for a command input context and transfer ring
    device->allocateInputContext(m_64ByteContextSize);
    device->allocateControlEndpointTransferRing();

    // Configure the command input context
    _configureDeviceInputContext(device);

    // Construct the Address Device TRB
    XhciAddressDeviceCommandTrb_t addressDeviceTrb;
    zeromem(&addressDeviceTrb, sizeof(XhciAddressDeviceCommandTrb_t));
    addressDeviceTrb.trbType = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
    addressDeviceTrb.inputContextPhysicalBase = device->getInputContextPhysicalBase();
    addressDeviceTrb.bsr = 1;
    addressDeviceTrb.slotId = device->slotId;

    // Send the Address Device command
    XhciCommandCompletionTrb_t* completionTrb = _sendCommand((XhciTrb_t*)&addressDeviceTrb, 200);
    if (!completionTrb) {
        kprintError("[*] Failed to complete the first Device Address command!\n");
        return;
    }

    if (m_64ByteContextSize) {
        // Sanity-check the actual device context entry in DCBAA
        XhciDeviceContext64* deviceContext = virtbase(m_dcbaa[device->slotId], XhciDeviceContext64);

        kprint("    DeviceContext[slotId=%i] address: 0x%llx slotState: %i epSate: %i maxPacketSize: %i\n    trdp:0x%llx\n",
            device->slotId, deviceContext->slotContext.deviceAddress, deviceContext->slotContext.slotState,
            deviceContext->controlEndpointContext.endpointState, deviceContext->controlEndpointContext.maxPacketSize,
            deviceContext->controlEndpointContext.transferRingDequeuePtr
        );
    } else {
        // Sanity-check the actual device context entry in DCBAA
        XhciDeviceContext32* deviceContext = virtbase(m_dcbaa[device->slotId], XhciDeviceContext32);

        kprint("    DeviceContext[slotId=%i] address: 0x%llx slotState: %i epSate: %i maxPacketSize: %i\n    trdp:0x%llx\n",
            device->slotId, deviceContext->slotContext.deviceAddress, deviceContext->slotContext.slotState,
            deviceContext->controlEndpointContext.endpointState, deviceContext->controlEndpointContext.maxPacketSize,
            deviceContext->controlEndpointContext.transferRingDequeuePtr
        );
    }

    // Buffer to hold the bytes received from GET_DESCRIPTOR command
    uint8_t* descriptorBuffer = (uint8_t*)allocXhciMemory(64, 128, 64);

    // Buffer to hold the bytes received from GET_DESCRIPTOR command
    uint8_t* transferStatusBuffer = (uint8_t*)allocXhciMemory(64, 16, 16);

    // Construct the Setup Stage TRB
    XhciSetupStageTrb_t setupStageTrb;
    zeromem(&setupStageTrb, sizeof(XhciSetupStageTrb_t));

    setupStageTrb.trbType = XHCI_TRB_TYPE_SETUP_STAGE;
    setupStageTrb.requestPacket.bRequestType = 0x80;
    setupStageTrb.requestPacket.bRequest = 6;    // GET_DESCRIPTOR
    setupStageTrb.requestPacket.wValue = 0x0100; // DEVICE
    setupStageTrb.requestPacket.wIndex = 0;
    setupStageTrb.requestPacket.wLength = 8;
    setupStageTrb.trbTransferLength = 8;
    setupStageTrb.interrupterTarget = 0;
    setupStageTrb.trt = 3;
    setupStageTrb.idt = 1;
    setupStageTrb.ioc = 0;

    // Construct the Data Stage TRB
    XhciDataStageTrb_t dataStageTrb;
    zeromem(&dataStageTrb, sizeof(XhciDataStageTrb_t));

    dataStageTrb.trbType = XHCI_TRB_TYPE_DATA_STAGE;
    dataStageTrb.trbTransferLength = 8;
    dataStageTrb.tdSize = 0;
    dataStageTrb.interrupterTarget = 0;
    dataStageTrb.ent = 1;
    dataStageTrb.chain = 1;
    dataStageTrb.dir = 1;
    dataStageTrb.dataBuffer = physbase(descriptorBuffer);

    // Construct the first Event Data TRB
    XhciEventDataTrb_t eventDataTrb;
    zeromem(&eventDataTrb, sizeof(XhciEventDataTrb_t));

    eventDataTrb.trbType = XHCI_TRB_TYPE_EVENT_DATA;
    eventDataTrb.interrupterTarget = 0;
    eventDataTrb.chain = 0;
    eventDataTrb.ioc = 1;
    eventDataTrb.eventData = physbase(transferStatusBuffer);

    // Small delay period between ringing the
    // doorbell and polling the event ring.
    const uint32_t commandDelay = 400;

    auto transferRing = device->getControlEndpointTransferRing();
    transferRing->enqueue((XhciTrb_t*)&setupStageTrb);
    transferRing->enqueue((XhciTrb_t*)&dataStageTrb);
    transferRing->enqueue((XhciTrb_t*)&eventDataTrb);

    kprint("[*] Ringing transfer ring doorbell: %i\n", transferRing->getDoorbellId());
    kprintInfo("   transferRing - pa:0x%llx va:0x%llx\n", transferRing->getPhysicalBase(), transferRing->getVirtualBase());
    m_doorbellManager->ringControlEndpointDoorbell(transferRing->getDoorbellId());

    // Let the host controller process the command
    msleep(commandDelay);

    _logUsbsts();
}

void XhciDriver::_configureDeviceInputContext(XhciDevice* device) {
    XhciInputControlContext32* inputControlContext = device->getInputControlContext(m_64ByteContextSize);
    XhciSlotContext32* slotContext = device->getInputSlotContext(m_64ByteContextSize);
    XhciEndpointContext32* controlEndpointContext = device->getInputControlEndpointContext(m_64ByteContextSize);

    // Enable slot and control endpoint contexts
    inputControlContext->addFlags = (1 << 0) | (1 << 1);
    inputControlContext->dropFlags = 0;

    // Configure the slot context
    slotContext->contextEntries = 1;
    slotContext->speed = device->speed;
    slotContext->rootHubPortNum = device->portNumber;
    slotContext->routeString = 0;
    slotContext->interrupterTarget = 0;

    // Configure the control endpoint context
    controlEndpointContext->endpointState = XHCI_ENDPOINT_STATE_DISABLED;
    controlEndpointContext->endpointType = XHCI_ENDPOINT_TYPE_CONTROL;
    controlEndpointContext->interval = 0;
    controlEndpointContext->errorCount = 3;
    controlEndpointContext->maxPacketSize = _getMaxInitialPacketSize(device->speed);
    controlEndpointContext->transferRingDequeuePtr = device->getControlEndpointTransferRing()->getPhysicalBase();
    controlEndpointContext->dcs = device->getControlEndpointTransferRing()->getCycleBit();
    controlEndpointContext->maxEsitPayloadLo = 0;
    controlEndpointContext->maxEsitPayloadHi = 0;
    controlEndpointContext->averageTrbLength = 8;
}
