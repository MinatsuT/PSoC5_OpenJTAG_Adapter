/*
  OpenJTAG CY7C65215 variant adapter.
  by Minatsu TUKISIMA
 */

#include <project.h>
#include <stdio.h>

/**************************************
 * Macros
 *************************************/
#define VTREF_THRESHOLD (2.9f)
#define MIN(x, y) ((x < y) ? x : y)
#define MAX(x, y) ((x > y) ? x : y)

// USBFS
// ----------------------------------------------------------------------
#define USBFS_DEVICE (0u)
#define IN_EP_NUM (1u)
#define OUT_EP_NUM (2u)
#define EP_SIZE (64u)
#define BUFFER_SIZE (512u)
uint8 InEP_buf[BUFFER_SIZE];
uint8 OutEP_buf[BUFFER_SIZE];
uint16 InEP_buf_idx = 0;
uint16 OutEP_buf_len = 0;
volatile int16 USB_Read_Request_Len = 0;
volatile int16 USB_Write_Request_Len = 0;

// Status LED
// ----------------------------------------------------------------------
typedef enum { OffLine, OnLine, ActIn, ActOut } STATUS;

// Print and debug print
// ----------------------------------------------------------------------
// clang-format off
#define DP_ENABLE 1
#if defined(DP_ENABLE)
char print_buf[256];
#define DP(...) {sprintf(print_buf, __VA_ARGS__);UART_KitProg_PutString(print_buf);}
#else
#define DP(...)
#endif

//#define DP2_ENABLE 1
#if defined(DP2_ENABLE)
#define DP2(...) DP(__VA_ARGS__)
#else
#define DP2(...)
#endif

//#define DP3_ENABLE 1
#if defined(DP3_ENABLE)
#define DP3(...) DP(__VA_ARGS__)
#else
#define DP3(...)
#endif

//#define DP4_ENABLE 1
#if defined(DP4_ENABLE)
#define DP4(...) DP(__VA_ARGS__)
#else
#define DP4(...)
#endif
// clang-format on

/**************************************
 * Variables
 *************************************/
volatile uint16 count = 0;
uint16 last_count = 0;
uint16 PWM_clock_divider;
uint8 tPwr = 255;
char Bin_Buf[17];

char *Tap_Desc[16] = {"TestLogicReset", "RunTestIdle", "Sel-DR", "Cap-DR",   "Shift-DR", "Exit1-DR", "Pause-DR", "Exit2-DR",
                      "Update-DR",      "Sel-IR",      "Cap-IR", "Shift-IR", "Exit1-IR", "Pause-IR", "Exit2-IR", "Update-IR"};

/**************************************
 * Function Prototypes
 *************************************/
void setStatus(STATUS status);
void loop(void);
void init_bit_reversal_table(void);
static void USBFS_push_byte(uint8 b);
static int USBFS_send(void);
static void check_VTref(void);
static void Set_Internal_Power(uint8 on_off);
static char *toBin(uint8 b, int len);
CY_ISR_PROTO(Slow_Tick_ISR);

/**************************************
 * Main
 *************************************/
uint8 work_out_bits;
uint8 work_RTI_count;
int main() {
    CyGlobalIntEnable;

    /* Start components. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);
    AMux_Start();
    Set_Internal_Power(0);
    JTAG_Start();
    UART_KitProg_Start();
    PWM_LED_Start();
    ADC_Start();
    ADC_StartConvert();
    Timer_1_Start();
    isr_1_StartEx(Slow_Tick_ISR);
    PWM_clock_divider = CLK_PWM_GetDividerRegister();

    DP("\n\n----------------------------------------\n");
    DP("Hello, PSoC5LP OpenJTAG Adapter.\n");
    DP("----------------------------------------\n\n");
    DP("'i' - Internal power mode.\n");
    DP("'e' - External power mode.\n");
    DP("'r' - Reset the target by TRST.\n");
    DP("'t' - Signal test for wave form analysis.\n");
    DP("\nExternal power mode.\n");

    for (;;) {
        setStatus(OffLine);
        DP("\nWait for enumeration.\n")
        while (0u == USBFS_GetConfiguration()) {
            check_VTref();
        }
        DP("Enumerated by host.\n");

        // Init JTAG and USB buffers
        JTAG_Reset();
        USB_Read_Request_Len = 0;
        USB_Write_Request_Len = 0;
        InEP_buf_idx = 0;
        OutEP_buf_len = 0;

        setStatus(OnLine);
        USBFS_EnableOutEP(OUT_EP_NUM);
        loop();
        DP("Connection lost.\n");
    }
}

/*
 * Main loop
 */
uint16 read_len;
uint16 to_be_read;
uint16 receive_total;
uint8 cmd, arg;
uint8 work_cur_state;
uint16 CLK_JTAG_div;
uint8 ret;
void loop() {
    for (;;) {
        uint8 rxData = UART_KitProg_GetChar();
        switch (rxData) {
        case 'i':
            Set_Internal_Power(1);
            DP("Internal power mode.\n");
            break;
        case 'e':
            Set_Internal_Power(0);
            DP("External power mode.\n");
            break;
        case 'r':
            DP("Do hard reset.\n");
            JTAG_Cmd = 0x80;
            CyDelay(500);
            JTAG_Cmd = 0x00;
            break;
        case 't':
            DP("Do signal test.\n");
            CLK_JTAG_SetDividerValue(1 << 7);
            uint8 cur_msb_lsb = JTAG_Get_Shift_Dir();
            JTAG_Set_Shift_Dir(MSB_FIRST);
            uint8 test_bits = 0b11100010;
            DP("Shift out and Read 8 Bits [%s], last TMS is LOW.\n", toBin(test_bits, 8));
            ret = JTAG_TAP_Scan(8 - 1, test_bits, 0 /* last TMS is HIGH(1) or LOW(0) */);
            DP("=>[%s]\n", toBin(ret, 8));
            DP("Set LSB First.\n");
            JTAG_Set_Shift_Dir(LSB_FIRST);
            test_bits = 0b11100010;
            DP("Shift out and Read 8 Bits [%s], last TMS is HIGH.\n", toBin(test_bits, 8));
            ret = JTAG_TAP_Scan(8 - 1, test_bits, 1 /* last TMS is HIGH(1) or LOW(0) */);
            DP("=>[%s]\n", toBin(ret, 8));
            JTAG_Set_Shift_Dir(cur_msb_lsb);
            break;
        }

        /* Check if configuration is changed. */
        if (0u != USBFS_IsConfigurationChanged()) {
            DP("USB configuration changed.\n");
            /* Re-enable endpoint when device is configured. */
            if (0u != USBFS_GetConfiguration()) {
                DP("Get USB configration successfully.\n");
                /* Enable OUT endpoint to receive data from host. */
                USBFS_EnableOutEP(OUT_EP_NUM);
            } else {
                return;
            }
        }

        /* Check if data from USB host was received. */
        if (USB_Write_Request_Len != 0) {
            setStatus(ActIn);
            OutEP_buf_len = 0;
            receive_total = 0;
            to_be_read = USB_Write_Request_Len;
            while (OutEP_buf_len < to_be_read) {
                while (USBFS_OUT_BUFFER_FULL != USBFS_GetEPState(OUT_EP_NUM)) {
                }
                read_len = USBFS_GetEPCount(OUT_EP_NUM);
                if (OutEP_buf_len + read_len > BUFFER_SIZE) {
                    DP3("Received %d/%d bytes\n", receive_total + read_len, to_be_read);
                    DP3("read_len(%d) exceeds BUFFER_SIZE(%d), drop the excess bytes");
                    read_len = BUFFER_SIZE - OutEP_buf_len;
                }
                USBFS_ReadOutEP(OUT_EP_NUM, OutEP_buf + OutEP_buf_len, read_len);
                receive_total += read_len;
                OutEP_buf_len += read_len;
                USBFS_EnableOutEP(OUT_EP_NUM); /* Enable OUT endpoint to receive next data from host. */
            }
            DP2("\r<=Received %d/%d bytes", receive_total, to_be_read);
            if (OutEP_buf_len > to_be_read) {
                DP2("Drop %d bytes\n", OutEP_buf_len - to_be_read);
                OutEP_buf_len = to_be_read;
            }
            USB_Write_Request_Len -= to_be_read;

            for (int i = 0; i < OutEP_buf_len; i++) {
                cmd = OutEP_buf[i] & 0x0f;
                arg = OutEP_buf[i] >> 4;
                switch (cmd) {
                case 0: // Set clock divider
                    DP("CMD 0: Set clock divider [%s] ", toBin(arg, 4));
                    CLK_JTAG_div = 1 << ((arg >> 1) + 0);
                    CLK_JTAG_SetDividerValue(CLK_JTAG_div);
                    DP("=>%.1fkHz\n", 76000.0 / CLK_JTAG_div);
                    break;
                case 1: // Set target TAP state
                    DP2("CMD 1: Set target TAP state [%s] ", toBin(arg, 4));
                    DP2("=> %s\n", Tap_Desc[arg]);
                    JTAG_TAP_Move(arg);
                    break;
                case 2: // Get target TAP state
                    DP2("CMD 2: Get target TAP state [%s] ", toBin(arg, 4));
                    ret = JTAG_TAP_Get_State() | ((tPwr != 0) ? (1 << 5) : 0);
                    DP2("=>[%s]\n", toBin(ret, 8));
                    USBFS_push_byte(ret);
                    break;
                case 3: // Software reset target TAP
                    DP("CMD 3: Software reset target TAP\n");
                    JTAG_TAP_Reset();
                    break;
                case 4: // Hardware reset target TAP
                    DP("CMD 4: Hardware reset target TAP\n");
                    JTAG_TAP_Reset();
                    JTAG_Cmd |= 0x80;
                    CyDelay(1);
                    JTAG_Cmd &= ~0x80;
                    JTAG_Reset();
                    break;
                case 5: // Set LSB(1)/MSB(0) mode
                    DP("CMD 5: Set LSB(1)/MSB(0) mode [%s] =>%s\n", toBin(arg, 4), arg ? "LSB" : "MSB");
                    JTAG_Set_Shift_Dir((arg & 1) ? LSB_FIRST : MSB_FIRST);
                    break;
                case 6: // Shift out and Read n Bits
                    DP2("CMD 6: Shift out and Read n Bits [%s] ", toBin(arg, 4));
                    if (!(i + 1 < OutEP_buf_len)) {
                        DP2("=> 2nd byte is not in the OutEP buffer. i=%d read_len=%d\n", i, read_len + 1);
                        break; // 2nd byte is not in the OutEP buffer.
                    }
                    i++;
                    work_out_bits = OutEP_buf[i];
                    ret = JTAG_TAP_Scan(arg >> 1, work_out_bits, arg & 1 /* last TMS is HIGH(1) or LOW(0) */);
                    DP2("%02x ", ret);
                    if (arg & 1) {
                        DP2("LAST\n");
                    }
                    USBFS_push_byte(ret);
                    break;
                case 7: // Run_Test_Idle Loop
                    DP2("CMD 7: Run_Test_Idle Loop [%s] ", toBin(arg, 4));
                    work_cur_state = JTAG_TAP_Get_State() & 0x0f;
                    if (work_cur_state != 1 /* Run_Test_Idle state */) {
                        DP2("=> current state (%d) != 1%d\n", work_cur_state);
                        break;
                    }
                    while (arg > 0) {
                        work_RTI_count = (arg > 8) ? 8 : arg;
                        JTAG_TAP_Scan(work_RTI_count, 0, 0);
                        arg -= work_RTI_count;
                    }
                    DP2("=> done\n");
                    break;
                default:
                    DP2("CMD Unknown: CMD=>%d ARG=%s\n", cmd, toBin(arg, 4));
                    break;
                }
            }

            setStatus(ActOut);
        }

        if (USBFS_send() == -1) {
            DP("break loop.\n");
            return;
        }
        check_VTref();
    }
}

// Check VTref voltage.
float VTref_val = 0.0f;
uint8 new_tPwr;
static void check_VTref() {
    if (last_count != count) {
        last_count = count;
        VTref_val = ADC_CountsTo_Volts(ADC_GetResult32());
        DP("VTref=%6.2f\r", VTref_val);
        new_tPwr = (VTref_val > VTREF_THRESHOLD);
        if (tPwr != new_tPwr) {
            tPwr = new_tPwr;
            CLK_PWM_SetDivider(!tPwr ? 0xffff : PWM_clock_divider);
            DP("\nTarget power is changed to %s.\n", tPwr ? "On" : "Off");
        }
    }
}

// Push 1 byte to InEP buffer.
static void USBFS_push_byte(uint8 b) {
    if (InEP_buf_idx >= BUFFER_SIZE) {
        return;
    }
    InEP_buf[InEP_buf_idx++] = b;
}

// Send InEP buffer.
int16 to_be_sent = 0;
int16 total_sent = 0;
uint16 timeout = 1000;
static int USBFS_send() {
    if (USB_Read_Request_Len == 0) {
        return 0;
    }
    DP3("InEP_buf_idx=%d USB_Read_Request_Len=%d\n", InEP_buf_idx, USB_Read_Request_Len);
    DP3("\n=>Send %d bytes\n", InEP_buf_idx);
    for (int i = 0; i < InEP_buf_idx; i++) {
        DP3(" %02x", InEP_buf[i]);
        if ((i % 16) == 15) {
            DP3("\n");
        }
    }
    DP3("\n");

    to_be_sent = 0;
    total_sent = 0;
    timeout = 1000;
    do {
        to_be_sent = MIN(InEP_buf_idx - total_sent, EP_SIZE);
        DP3("Try to send %d\n", to_be_sent);
        while (USBFS_IN_BUFFER_EMPTY != USBFS_GetEPState(IN_EP_NUM)) {
            CyDelay(1);
            if (--timeout == 0) {
                DP("USBFS_send() timeout.\n");
                return 0;
            }
        }
        USBFS_LoadInEP(IN_EP_NUM, InEP_buf + total_sent, to_be_sent);
        total_sent += to_be_sent;
        DP3("Sent %d (%d/%d)\n", to_be_sent, total_sent, InEP_buf_idx);
    } while (to_be_sent == EP_SIZE);
    InEP_buf_idx = 0;
    USB_Read_Request_Len = 0;
    return 0;
}

/* Set host status, and control on-board. */
#define PWM_HIGH (10u)
void setStatus(STATUS status) {
    switch (status) {
    case OffLine:
        PWM_LED_WriteCompare(0);
        break;
    case OnLine:
        PWM_LED_WriteCompare(PWM_HIGH);
        break;
    case ActIn:
        PWM_LED_WriteCompare(0);
        break;
    case ActOut:
        PWM_LED_WriteCompare(PWM_HIGH);
        break;
    }
}

static void Set_Internal_Power(uint8 on_off) {
    if (on_off) {
        VDAC_3v3_Start();
        AMux_Select(1);
    } else {
        AMux_Select(0);
        VDAC_3v3_Stop();
    }
}

static char *toBin(uint8 b, int len) {
    for (int i = 0; i < len; i++) {
        Bin_Buf[i] = (b & (1 << (len - 1 - i))) ? '1' : '0';
    }
    Bin_Buf[len] = '\0';
    return Bin_Buf;
}

/**************************************
 * USBFS Vendor Request Callbacks
 *************************************/
#define JTAG_ENABLE 0xD0  /* bRequest: enable JTAG */
#define JTAG_DISABLE 0xD1 /* bRequest: disable JTAG */
#define JTAG_READ 0xD2    /* bRequest: read buffer */
#define JTAG_WRITE 0xD3   /* bRequest: write buffer */
#define USB_TIMEOUT 100
uint16 wValue, wIndex;
uint8 USBFS_HandleVendorRqst_Callback() {
    uint8 requestHandled = USBFS_FALSE;

    wValue = CY_GET_REG16(USBFS_wValue);
    wIndex = CY_GET_REG16(USBFS_wIndex);

    /* Based on the bRequest sent, perform the proper action */
    switch (CY_GET_REG8(USBFS_bRequest)) {
    case JTAG_ENABLE:
        requestHandled = USBFS_InitNoDataControlTransfer();
        break;
    case JTAG_DISABLE:
        requestHandled = USBFS_InitNoDataControlTransfer();
        break;
    case JTAG_READ:
        // Here, wValue indicates the size of the next bulk read.
        USB_Read_Request_Len += wValue;
        requestHandled = USBFS_InitNoDataControlTransfer();
        break;
    case JTAG_WRITE:
        // Here, wValue indicates the size of the next bulk write.
        USB_Write_Request_Len += wValue;
        requestHandled = USBFS_InitNoDataControlTransfer();
        break;
    }
    return (requestHandled);
}

/**************************************
 * Interrupt handler
 *************************************/
CY_ISR(Slow_Tick_ISR) {
    Timer_1_STATUS;
    count++;
}

/* [] END OF FILE */