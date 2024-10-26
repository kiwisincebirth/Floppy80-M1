#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/regs/intctrl.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"
#include "tusb.h"

//#include "fdc.pio.h"
#include "defines.h"
#include "sd_core.h"
#include "fdc.h"
#include "system.h"
#include "video.h"
#include "cli.h"

#define NopDelay() __nop(); __nop(); __nop(); __nop(); __nop();

extern byte g_byVideoMemory[VIDEO_BUFFER_SIZE];
extern word g_wVideoLinesModified[MAX_VIDEO_LINES];
extern BufferType g_bFdcRequest;
extern BufferType g_bFdcResponse;

byte by_memory[0x8000];

byte g_byRtcIntrActive;
byte g_byFdcIntrActive;

//-----------------------------------------------------------------------------
void __not_in_flash_func(ServiceFastReadOperation)(word addr)
{
    byte data;

    if (addr >= 0x8000) // RD from upper 32k memory
    {
        clr_gpio(DIR_PIN);                  // B to A direction
        set_bus_as_output();                // make data pins (D0-D7) outputs
        clr_gpio(DATAB_OE_PIN);             // enable data bus transciever

        // put byte on data bus
        put_byte_on_bus(by_memory[addr-0x8000]);

        clr_gpio(WAIT_PIN);
        while (get_gpio(MREQ_PIN) == 0);

        // turn bus around
        set_gpio(DATAB_OE_PIN);             // disable data bus transciever
        set_bus_as_input();                 // reset data pins (D0-D7) inputs
        set_gpio(DIR_PIN);                  // A to B direction
    }
    else if ((addr >= FDC_RESPONSE_ADDR_START) && (addr <= FDC_RESPONSE_ADDR_STOP))
    {
        addr -= FDC_RESPONSE_ADDR_START;

        if (addr < FDC_CMD_SIZE)
        {
            data = g_bFdcResponse.cmd[addr];
        }
        else
        {
            data = g_bFdcResponse.buf[addr-FDC_CMD_SIZE];
        }

        clr_gpio(DIR_PIN);      // B to A direction
        set_bus_as_output();    // make data pins (D0-D7) outputs
        clr_gpio(DATAB_OE_PIN); // enable data bus transciever

        // put byte on data bus
        put_byte_on_bus(data);

        clr_gpio(WAIT_PIN);
        while (get_gpio(MREQ_PIN) == 0);

        // turn bus around
        set_gpio(DATAB_OE_PIN); // disable data bus transciever
        set_bus_as_input();     // reset data pins (D0-D7) inputs
        set_gpio(DIR_PIN);      // A to B direction
    }
    else
    {
        clr_gpio(WAIT_PIN);
        while (get_gpio(MREQ_PIN) == 0);
    }
}

//-----------------------------------------------------------------------------
void __not_in_flash_func(ServiceFastWriteOperation)(word addr)
{
    byte data;

    if ((addr >= FDC_REQUEST_ADDR_START) && (addr <= FDC_REQUEST_ADDR_STOP))
    {
        // get data byte
        clr_gpio(DATAB_OE_PIN);
        NopDelay();
        data = get_gpio_data_byte();
        set_gpio(DATAB_OE_PIN);

        addr -= FDC_REQUEST_ADDR_START;

        if (addr < FDC_CMD_SIZE)
        {
            g_bFdcRequest.cmd[addr] = data;
        }
        else
        {
            g_bFdcRequest.buf[addr-FDC_CMD_SIZE] = data;
        }
    }
    else if ((addr >= VIDEO_ADDR_START) && (addr <= VIDEO_ADDR_END))
    {
        // get data byte
        clr_gpio(DATAB_OE_PIN);
        NopDelay();
        data = get_gpio_data_byte();
        set_gpio(DATAB_OE_PIN);

        addr -= VIDEO_ADDR_START;
        g_byVideoMemory[addr] = data;
        ++g_wVideoLinesModified[addr/VIDEO_NUM_COLS];
    }
    else if (addr >= 0x8000) // WR to upper 32k memory
    {
        // get data byte
        clr_gpio(DATAB_OE_PIN);
        NopDelay();
        by_memory[addr-0x8000] = get_gpio_data_byte();
        set_gpio(DATAB_OE_PIN);
    }

    clr_gpio(WAIT_PIN);
    while (get_gpio(MREQ_PIN) == 0);
}

//-----------------------------------------------------------------------------
void __not_in_flash_func(ServiceFdcReadOperation)(word addr)
{
    byte data;

    switch (addr)
    {
        case 0x37E0: // RTC
        case 0x37E1:
        case 0x37E2:
        case 0x37E3:
            data = 0x3F;

            if (g_FDC.status.byIntrRequest)
            {
                data |= 0x40;
            }

            if (g_byRtcIntrActive)
            {
                data |= 0x80;
                g_byRtcIntrActive = false;

                if (!g_byFdcIntrActive)
                {
                    // deactivate intr
                    clr_gpio(INT_PIN);
                }
            }

            clr_gpio(DIR_PIN);          // B to A direction
            set_bus_as_output();        // make data pins (D0-D7) outputs
            clr_gpio(DATAB_OE_PIN);     // enable data bus transciever

            // put byte on data bus
            put_byte_on_bus(data);

            clr_gpio(WAIT_PIN);
            while (get_gpio(MREQ_PIN) == 0);

            // turn bus around
            set_gpio(DATAB_OE_PIN);     // disable data bus transciever
            set_bus_as_input();         // reset data pins (D0-D7) inputs
            set_gpio(DIR_PIN);          // A to B direction
            break;

        case 0x37EC: // Cmd/Status register
            if (!g_byRtcIntrActive)
            {
                // deactivate intr
                clr_gpio(INT_PIN);
            }

        case 0x37ED: // Track register
        case 0x37EE: // Sector register
        case 0x37EF: // Data register
            data = fdc_read(addr);

            clr_gpio(DIR_PIN);          // B to A direction
            set_bus_as_output();        // make data pins (D0-D7) outputs
            clr_gpio(DATAB_OE_PIN);     // enable data bus transciever

            // put byte on data bus
            put_byte_on_bus(data);

            clr_gpio(WAIT_PIN);
            while (get_gpio(MREQ_PIN) == 0);

            // turn bus around
            set_gpio(DATAB_OE_PIN);     // disable data bus transciever
            set_bus_as_input();         // reset data pins (D0-D7) inputs
            set_gpio(DIR_PIN);          // A to B direction
            break;
    }
}

//-----------------------------------------------------------------------------
void __not_in_flash_func(ServiceFdcWriteOperation)(word addr)
{
    byte data;

    if (addr < 0x8000) // WR to lower 32k memory
    {
        switch (addr)
        {
            case 0x37E0:
            case 0x37E1:
            case 0x37E2:
            case 0x37E3: // drive select
                // get data byte
                clr_gpio(DATAB_OE_PIN);
                NopDelay();
                data = get_gpio_data_byte();
                set_gpio(DATAB_OE_PIN);

                fdc_write_drive_select(data);
                break;

            case 0x37EC: // Cmd/Status register
            case 0x37ED: // Track register
            case 0x37EE: // Sector register
            case 0x37EF: // Data register
                // get data byte
                clr_gpio(DATAB_OE_PIN);
                NopDelay();
                data = get_gpio_data_byte();
                set_gpio(DATAB_OE_PIN);

                fdc_write(addr, data);
                break;
        }
    }

    clr_gpio(WAIT_PIN);
    while (get_gpio(MREQ_PIN) == 0);
}

//-----------------------------------------------------------------------------
void __not_in_flash_func(ServiceFdcMemoryOperation)(word addr)
{
    // wait for RD or WR to go active or MREQ to go inactive
    while ((get_gpio(RD_PIN) != 0) && (get_gpio(WR_PIN) != 0) && (get_gpio(MREQ_PIN) == 0));

    if (get_gpio(RD_PIN) == 0)
    {
        ServiceFdcReadOperation(addr);
    }
    else if (get_gpio(WR_PIN) == 0)
    {
        ServiceFdcWriteOperation(addr);
    }
    else
    {
        clr_gpio(WAIT_PIN);
        while (get_gpio(MREQ_PIN) == 0);
    }
}

//-----------------------------------------------------------------------------
word __not_in_flash_func(get_address)(void)
{
    union {
        byte b[2];
        word w;
    } addr;

    // read low address byte
    clr_gpio(ADDRL_OE_PIN);
    NopDelay();
    addr.b[0] = get_gpio_data_byte();
    set_gpio(ADDRL_OE_PIN);

    // read high address byte
    clr_gpio(ADDRH_OE_PIN);
    NopDelay();
    addr.b[1] = get_gpio_data_byte();
    set_gpio(ADDRH_OE_PIN);

    return addr.w;
}

//-----------------------------------------------------------------------------
void __not_in_flash_func(service_memory)(void)
{
    byte data;
    word addr;

    clr_gpio(WAIT_PIN);

    while (1)
    {
        // wait for MREQ to go inactive
        while (get_gpio(MREQ_PIN) == 0);

        // wait for MREQ to go active
        while (get_gpio(MREQ_PIN) != 0);

        addr = get_address();

        if ((addr >= 0x37E0) && (addr <= 0x37EF)) // activate WAIT_PIN
        {
            set_gpio(WAIT_PIN);
            ServiceFdcMemoryOperation(addr);
        }
        else
        {
            // wait for RD or WR to go active or MREQ to go inactive
            while ((get_gpio(RD_PIN) != 0) && (get_gpio(WR_PIN) != 0) && (get_gpio(MREQ_PIN) == 0));

            if (get_gpio(RD_PIN) == 0)
            {
                ServiceFastReadOperation(addr);
            }
            else if (get_gpio(WR_PIN) == 0)
            {
                ServiceFastWriteOperation(addr);
            }
            else
            {
                clr_gpio(WAIT_PIN);
                while (get_gpio(MREQ_PIN) == 0);
            }
        }
   }
}
