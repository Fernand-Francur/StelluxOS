#include "xhci_device_ctx.h"
#include <kprint.h>

void XhciDevice::allocateInputContext(bool use64ByteContexts) {
    // Calculate the input context size based
    // on the capability register parameters.
    uint64_t inputContextSize = use64ByteContexts ? sizeof(XhciInputContext64) : sizeof(XhciInputContext32);

    // Allocate and zero out the input context
    m_inputContext = allocXhciMemory(
        inputContextSize,
        XHCI_INPUT_CONTROL_CONTEXT_ALIGNMENT,
        XHCI_INPUT_CONTROL_CONTEXT_BOUNDARY
    );
}

uint64_t XhciDevice::getInputContextPhysicalBase() {
    return physbase(m_inputContext);
}

void XhciDevice::allocateControlEndpointTransferRing() {
    m_controlEndpointTransferRing = new XhciTransferRing(XHCI_TRANSFER_RING_TRB_COUNT, slotId);
}

XhciInputControlContext32* XhciDevice::getInputControlContext(bool use64ByteContexts) {
    if (use64ByteContexts) {
        XhciInputContext64* inputCtx = static_cast<XhciInputContext64*>(m_inputContext);
        return reinterpret_cast<XhciInputControlContext32*>(&inputCtx->controlContext);
    } else {
        XhciInputContext32* inputCtx = static_cast<XhciInputContext32*>(m_inputContext);
        return &inputCtx->controlContext;
    }
}

XhciSlotContext32* XhciDevice::getInputSlotContext(bool use64ByteContexts) {
    if (use64ByteContexts) {
        XhciInputContext64* inputCtx = static_cast<XhciInputContext64*>(m_inputContext);
        return reinterpret_cast<XhciSlotContext32*>(&inputCtx->deviceContext.slotContext);
    } else {
        XhciInputContext32* inputCtx = static_cast<XhciInputContext32*>(m_inputContext);
        return &inputCtx->deviceContext.slotContext;
    }
}

XhciEndpointContext32* XhciDevice::getInputControlEndpointContext(bool use64ByteContexts) {
    if (use64ByteContexts) {
        XhciInputContext64* inputCtx = static_cast<XhciInputContext64*>(m_inputContext);
        return reinterpret_cast<XhciEndpointContext32*>(&inputCtx->deviceContext.controlEndpointContext);
    } else {
        XhciInputContext32* inputCtx = static_cast<XhciInputContext32*>(m_inputContext);
        return &inputCtx->deviceContext.controlEndpointContext;
    }
}

void printXhciDeviceDescriptor(const XhciDeviceDescriptor *desc) {
    kprint("XHCI Device Descriptor:\n");
    kprint("-------------------------------\n");
    kprint("bLength:              %i\n", desc->bLength);
    kprint("bDescriptorType:      %i\n", desc->bDescriptorType);
    kprint("bcdUsb:               %x\n", desc->bcdUsb);            // Typically printed in hex
    kprint("bDeviceClass:         %i\n", desc->bDeviceClass);
    kprint("bDeviceSubClass:      %i\n", desc->bDeviceSubClass);
    kprint("bDeviceProtocol:      %i\n", desc->bDeviceProtocol);
    kprint("bMaxPacketSize0:      %i\n", desc->bMaxPacketSize0);
    kprint("idVendor:             0x%x\n", desc->idVendor);         // Vendor ID often in hex
    kprint("idProduct:            0x%x\n", desc->idProduct);        // Product ID often in hex
    kprint("bcdDevice:            0x%x\n", desc->bcdDevice);        // Version often in hex
    kprint("iManufacturer:        %i\n", desc->iManufacturer);
    kprint("iProduct:             %i\n", desc->iProduct);
    kprint("iSerialNumber:        %i\n", desc->iSerialNumber);
    kprint("bNumConfigurations:   %i\n", desc->bNumConfigurations);
    kprint("-------------------------------\n");
}

const char* xhciSlotStateToString(uint8_t slotState) {
    switch (slotState) {
    case XHCI_SLOT_STATE_DISABLED_ENABLED: return "Disabled/Enabled";
    case XHCI_SLOT_STATE_DEFAULT: return "Default";
    case XHCI_SLOT_STATE_ADDRESSED: return "Addressed";
    case XHCI_SLOT_STATE_CONFIGURED: return "Configured";
    case XHCI_SLOT_STATE_RESERVED: return "Reserved";
    default:
        return "Undefined";
    }
}

const char* xhciEndpointStateToString(uint8_t epState) {
    switch (epState) {
    case XHCI_ENDPOINT_STATE_DISABLED: return "Disabled";
    case XHCI_ENDPOINT_STATE_RUNNING: return "Running";
    case XHCI_ENDPOINT_STATE_HALTED: return "Halted";
    case XHCI_ENDPOINT_STATE_STOPPED: return "Stopped";
    case XHCI_ENDPOINT_STATE_ERROR: return "Error";
    default:
        return "Undefined";
    };
}
