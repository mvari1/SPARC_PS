//-------------------------------------------------------------------
// main.cc  -  H.264 encoder PS firmware
//
// Pipeline:
//   OV5640 (YUV422) → MIPI CSI-2 RX → h264_enc_axi (FPGA) → AXI DMA S2MM → DDR
//   PS: start encoder per frame, poll done, write [size+bitstream] to SD via FatFS
//
// Address map (from Vivado block design):
//   h264_enc_axi AXI-Lite:  0x40000000  (4 KB)
//   AXI DMA AXI-Lite:       0x40400000  (64 KB)
//   MIPI CSI-2 RX AXI-Lite: 0x43C00000  (4 KB)
//   DDR (HP0):               0x00000000 – 0x3FFFFFFF
//
// SD file format  (VIDEO.H26):
//   [4B magic "XK64"][2B width LE][2B height LE][1B qp][1B fps][2B reserved]
//   then for each frame:  [4B frame_size LE][frame_size bytes Annex-B H.264]
//-------------------------------------------------------------------

#include "xparameters.h"
#include "xaxidma.h"
#include "xuartps_hw.h"
#include "xcsiss_hw.h"
#include "xcsi_hw.h"
#include <cstring>
#include <cstdlib>

#include "platform/platform.h"
#include "ov5640/OV5640.h"
#include "ov5640/ScuGicInterruptController.h"
#include "ov5640/PS_GPIO.h"
#include "ov5640/PS_IIC.h"

#include "ff.h"
#include "xil_cache.h"

// ----------------------------------------------------------------
// Hardware base addresses
// ----------------------------------------------------------------
#define H264_BASE       0x40000000u
#define DMA_BASE        0x40400000u
#define MIPI_RX_BASE    0x43C00000u

// H264 AXI-Lite register offsets (see h264_enc_axi.v)
#define H264_CTRL_OFF   0x00u   // WO: bit[0] = start (auto-clears)
#define H264_STAT_OFF   0x04u   // RO: bit[0]=done, bit[1]=busy
#define H264_FRMSZ_OFF  0x08u   // RO: [23:0] last frame byte count

// Bitstream DMA buffer in DDR — placed above PS code/stack
// Sized at 1 MB: ample for one 640×480 H.264 intra frame (~17 KB typical)
#define BS_BUF_ADDR     0x10000000u
#define BS_BUF_SIZE     0x00100000u

// PS peripheral IDs
#define IRPT_CTL_DEVID  XPAR_PS7_SCUGIC_0_DEVICE_ID
#define GPIO_DEVID      XPAR_PS7_GPIO_0_DEVICE_ID
#define GPIO_IRPT_ID    XPAR_PS7_GPIO_0_INTR
#define CAM_I2C_DEVID   XPAR_PS7_I2C_0_DEVICE_ID
#define CAM_I2C_IRPT_ID XPAR_PS7_I2C_0_INTR

// Recording parameters (must match FPGA constants in bs_framer/xk264)
#define RECORD_W        640
#define RECORD_H        480
#define RECORD_QP       28
#define RECORD_FPS      60

using namespace digilent;

// ----------------------------------------------------------------
// H264 encoder register accessors
// ----------------------------------------------------------------
static inline void h264_start(void)
{
    Xil_Out32(H264_BASE + H264_CTRL_OFF, 0x1u);
}

static inline u32 h264_status(void)
{
    return Xil_In32(H264_BASE + H264_STAT_OFF);
}

static inline u32 h264_frmsz(void)
{
    return Xil_In32(H264_BASE + H264_FRMSZ_OFF) & 0x00FFFFFFu;
}

// ----------------------------------------------------------------
// AXI DMA S2MM (device → memory, i.e. encoder bitstream → DDR)
// ----------------------------------------------------------------
static XAxiDma dma_inst;

static int dma_init(void)
{
    // NOTE: verify XPAR_AXI_DMA_0_DEVICE_ID exists in xparameters.h
    // Vitis generates this name from the IP instance name in the BD.
    XAxiDma_Config *cfg = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID);
    if (!cfg) {
        xil_printf("DMA: LookupConfig failed — check XPAR_AXI_DMA_0_DEVICE_ID\r\n");
        return -1;
    }
    if (XAxiDma_CfgInitialize(&dma_inst, cfg) != XST_SUCCESS) {
        xil_printf("DMA: CfgInitialize failed\r\n");
        return -1;
    }
    if (XAxiDma_HasSg(&dma_inst)) {
        xil_printf("DMA: SG mode detected — use Simple DMA in BD\r\n");
        return -1;
    }
    XAxiDma_IntrDisable(&dma_inst, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    return 0;
}

// Pre-arm S2MM channel for up to max_len bytes.
// Must be called BEFORE h264_start() so DMA is ready when first byte arrives.
static int dma_arm(void *buf, u32 max_len)
{
    Xil_DCacheInvalidateRange((UINTPTR)buf, max_len);
    int rc = XAxiDma_SimpleTransfer(&dma_inst,
                                    (UINTPTR)buf, max_len,
                                    XAXIDMA_DEVICE_TO_DMA);
    if (rc != XST_SUCCESS)
        xil_printf("DMA: SimpleTransfer failed (%d)\r\n", rc);
    return rc;
}

static int dma_busy(void)
{
    return XAxiDma_Busy(&dma_inst, XAXIDMA_DEVICE_TO_DMA);
}

// ----------------------------------------------------------------
// MIPI CSI-2 RX helpers
// ----------------------------------------------------------------
static void mipi_reset_enable(void)
{
    Xil_Out32(MIPI_RX_BASE + XCSI_CCR_OFFSET, 0x00000002u);  // soft reset
    Xil_Out32(MIPI_RX_BASE + XCSI_CCR_OFFSET, 0x00000000u);  // de-assert
    Xil_Out32(MIPI_RX_BASE + XCSI_CCR_OFFSET, 0x00000001u);  // enable
}

static void print_mipi_status(void)
{
    u32 ccr = Xil_In32(MIPI_RX_BASE + XCSI_CCR_OFFSET);
    u32 csr = Xil_In32(MIPI_RX_BASE + XCSI_CSR_OFFSET);
    u32 isr = Xil_In32(MIPI_RX_BASE + XCSI_ISR_OFFSET);
    u32 pcr = Xil_In32(MIPI_RX_BASE + 0x04);

    xil_printf("\r\n=== MIPI CSI-2 RX Status ===\r\n");
    xil_printf(" CCR: 0x%08X  [Enable:%d  SoftRst:%d]\r\n",
               ccr,
               (ccr & XCSI_CCR_COREENB_MASK)   >> XCSI_CCR_COREENB_SHIFT,
               (ccr & XCSI_CCR_SOFTRESET_MASK) >> XCSI_CCR_SOFTRESET_SHIFT);
    xil_printf(" CSR: 0x%08X  ISR: 0x%08X\r\n", csr, isr);
    xil_printf(" PCR: 0x%08X  [MaxLanes:%d  ActLanes:%d]\r\n",
               pcr,
               (pcr & XCSI_PCR_MAXLANES_MASK) >> XCSI_PCR_MAXLANES_SHIFT,
               (pcr & XCSI_PCR_ACTLANES_MASK) >> XCSI_PCR_ACTLANES_SHIFT);

    u32 vc0inf1 = Xil_In32(MIPI_RX_BASE + XCSI_VC0INF1R_OFFSET);
    xil_printf(" VC0 Info1: 0x%08X  [Lines:%u  Bytes/line:%u]\r\n",
               vc0inf1,
               (vc0inf1 & XCSI_VCXINF1R_LINECOUNT_MASK) >> XCSI_VCXINF1R_LINECOUNT_SHIFT,
               (vc0inf1 & XCSI_VCXINF1R_BYTECOUNT_MASK));

    u32 l0infr = Xil_In32(MIPI_RX_BASE + XCSI_L0INFR_OFFSET);
    xil_printf(" Lane0: 0x%08X  [Stop:%d  SoTErr:%d]\r\n",
               l0infr,
               (l0infr & XCSI_LXINFR_STOP_MASK)   ? 1 : 0,
               (l0infr & XCSI_LXINFR_SOTERR_MASK) ? 1 : 0);
}

static void print_h264_status(void)
{
    u32 st = h264_status();
    xil_printf("H264 STATUS: 0x%08X  [done:%d  busy:%d]  FRMSZ: %u\r\n",
               st, st & 1, (st >> 1) & 1, h264_frmsz());
}

// ----------------------------------------------------------------
// SD file header  (12 bytes)
// ----------------------------------------------------------------
static int write_file_header(FIL *fil)
{
    uint8_t hdr[12] = {
        'X', 'K', '6', '4',
        (uint8_t)(RECORD_W & 0xFF), (uint8_t)(RECORD_W >> 8),
        (uint8_t)(RECORD_H & 0xFF), (uint8_t)(RECORD_H >> 8),
        RECORD_QP, RECORD_FPS,
        0, 0
    };
    UINT bw;
    FRESULT res = f_write(fil, hdr, sizeof(hdr), &bw);
    return (res == FR_OK && bw == sizeof(hdr)) ? 0 : -1;
}

// ----------------------------------------------------------------
// Main recording loop
// ----------------------------------------------------------------
static int record_frames(FIL *fil, int n_frames, u8 *bs_buf)
{
    xil_printf("Recording %d frames...\r\n", n_frames);

    for (int i = 0; i < n_frames; i++) {

        // 1. Pre-arm DMA S2MM BEFORE starting encoder
        if (dma_arm(bs_buf, BS_BUF_SIZE) != XST_SUCCESS)
            return -1;

        // 2. Kick encoder for one frame
        h264_start();

        // 3. Poll encoder STATUS.done (bit 0)
        //    At 60fps the encoder has ~16.7ms budget. 10M polls at 50MHz is plenty.
        u32 timeout = 50000000u;
        while (!(h264_status() & 0x1u) && --timeout) {}
        if (!timeout) {
            xil_printf("Encoder timeout on frame %d\r\n", i);
            return -1;
        }

        // 4. Wait for DMA to drain remaining bytes after TLAST
        timeout = 1000000u;
        while (dma_busy() && --timeout) {}

        // 5. Get authoritative byte count from H264 FRMSZ register
        u32 sz = h264_frmsz();
        if (sz == 0 || sz > BS_BUF_SIZE) {
            xil_printf("Bad frame size %u on frame %d\r\n", sz, i);
            continue;
        }

        // 6. Writeback D-cache so f_write sees fresh data
        Xil_DCacheFlushRange((UINTPTR)bs_buf, sz);

        // 7. Write 4-byte little-endian size then frame data
        UINT bw;
        if (f_write(fil, &sz, 4, &bw) != FR_OK || bw != 4) {
            xil_printf("SD size write failed frame %d\r\n", i);
            return -1;
        }
        if (f_write(fil, bs_buf, sz, &bw) != FR_OK || bw != sz) {
            xil_printf("SD data write failed frame %d\r\n", i);
            return -1;
        }
        f_sync(fil);

        xil_printf("Frame %4d: %6u bytes\r\n", i, sz);
    }

    xil_printf("Done. %d frames written.\r\n", n_frames);
    return 0;
}

// ----------------------------------------------------------------
// Minimal CLI helpers
// ----------------------------------------------------------------
static void cli_readline(char *buf, size_t maxlen)
{
    size_t idx = 0;
    while (1) {
        u32 ch = Xil_In32(STDOUT_BASEADDRESS + XUARTPS_FIFO_OFFSET);
        char c = (char)(ch & 0xFF);
        if (c == '\r' || c == '\n') {
            xil_printf("\r\n");
            buf[idx] = '\0';
            return;
        } else if ((c == '\b' || c == 127) && idx > 0) {
            idx--;
            xil_printf("\b \b");
        } else if (idx < maxlen - 1 && c >= 32 && c <= 126) {
            buf[idx++] = c;
            xil_printf("%c", c);
        }
    }
}

static bool parse_hex_u16(const char *s, uint16_t &out)
{
    out = 0;
    if (s[0] == '0' && s[1] == 'x') s += 2;
    while (*s) {
        char c = *s++;
        uint8_t v;
        if      (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return false;
        out = (out << 4) | v;
    }
    return true;
}

static bool parse_hex_u8(const char *s, uint8_t &out)
{
    uint16_t tmp;
    if (!parse_hex_u16(s, tmp) || tmp > 0xFF) return false;
    out = (uint8_t)tmp;
    return true;
}

static void print_menu(void)
{
    xil_printf(
        "\r\n==== H.264 Encoder CLI ====\r\n"
        "r  - Record N frames to VIDEO.H26\r\n"
        "s  - Print H264 + MIPI status\r\n"
        "wr - Write OV5640 register\r\n"
        "rr - Read OV5640 register\r\n"
        "q  - Close file and quit\r\n"
        "> ");
}

// ----------------------------------------------------------------
// main
// ----------------------------------------------------------------
int main(void)
{
    init_platform();
    xil_printf("=== H.264 Encoder  built %s %s ===\r\n", __DATE__, __TIME__);

    // SD card
    FATFS fs;
    if (f_mount(&fs, "0:", 1) != FR_OK) {
        xil_printf("SD mount failed\r\n");
        return -1;
    }

    // Peripherals
    ScuGicInterruptController irpt_ctl(IRPT_CTL_DEVID);
    PS_GPIO<ScuGicInterruptController> gpio(GPIO_DEVID, irpt_ctl, GPIO_IRPT_ID);
    PS_IIC<ScuGicInterruptController> iic(CAM_I2C_DEVID, irpt_ctl, CAM_I2C_IRPT_ID, 100000);

    // AXI DMA
    if (dma_init() != 0) return -1;
    xil_printf("AXI DMA ready\r\n");

    // Camera + MIPI
    OV5640 cam(iic, gpio);
    mipi_reset_enable();
    cam.set_mode(OV5640_cfg::MODE_480P_YUV422_60FPS);
    cam.set_awb(OV5640_cfg::AWB_ADVANCED);
    xil_printf("Camera up: 640x480 YUV422 60fps\r\n");
    print_mipi_status();

    // Open output file and write 12-byte header
    FIL fil;
    if (f_open(&fil, "VIDEO.H26", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        xil_printf("File open failed\r\n");
        return -1;
    }
    if (write_file_header(&fil) != 0) {
        xil_printf("Header write failed\r\n");
        return -1;
    }
    xil_printf("VIDEO.H26 opened, header written\r\n");

    u8 *bs_buf = (u8 *)BS_BUF_ADDR;

    while (1) {
        char cmd[16];
        print_menu();
        cli_readline(cmd, sizeof(cmd));

        if (!strcmp(cmd, "r")) {
            char ns[16];
            xil_printf("Num frames (1–9000): ");
            cli_readline(ns, sizeof(ns));
            int nf = atoi(ns);
            if (nf < 1 || nf > 9000) { xil_printf("Out of range\r\n"); continue; }
            record_frames(&fil, nf, bs_buf);

        } else if (!strcmp(cmd, "s")) {
            print_mipi_status();
            print_h264_status();

        } else if (!strcmp(cmd, "wr")) {
            char line[32];
            uint16_t addr; uint8_t val;
            xil_printf("Register addr (hex): ");
            cli_readline(line, sizeof(line));
            if (!parse_hex_u16(line, addr)) { xil_printf("Bad addr\r\n"); continue; }
            xil_printf("Value (hex): ");
            cli_readline(line, sizeof(line));
            if (!parse_hex_u8(line, val)) { xil_printf("Bad value\r\n"); continue; }
            cam.writeReg(addr, val);
            xil_printf("Wrote 0x%02X to reg 0x%04X\r\n", val, addr);

        } else if (!strcmp(cmd, "rr")) {
            char line[32];
            uint16_t addr; uint8_t val;
            xil_printf("Register addr (hex): ");
            cli_readline(line, sizeof(line));
            if (!parse_hex_u16(line, addr)) { xil_printf("Bad addr\r\n"); continue; }
            cam.readReg(addr, val);
            xil_printf("Reg 0x%04X = 0x%02X\r\n", addr, val);

        } else if (!strcmp(cmd, "q")) {
            break;

        } else {
            xil_printf("Unknown command\r\n");
        }
    }

    f_close(&fil);
    xil_printf("VIDEO.H26 closed. Goodbye.\r\n");
    cleanup_platform();
    return 0;
}
