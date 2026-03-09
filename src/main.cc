#include "xparameters.h"
#include "xuartps_hw.h"
#include "xcsiss_hw.h"
#include "xcsi_hw.h"

#include "platform/platform.h"
#include "ov5640/OV5640.h"
#include "ov5640/ScuGicInterruptController.h"
#include "ov5640/PS_GPIO.h"
#include "ov5640/AXI_VDMA.h"
#include "ov5640/PS_IIC.h"

#include "ff.h"
#include "xil_cache.h"

#define IRPT_CTL_DEVID 		XPAR_PS7_SCUGIC_0_DEVICE_ID
#define GPIO_DEVID			XPAR_PS7_GPIO_0_DEVICE_ID
#define GPIO_IRPT_ID		XPAR_PS7_GPIO_0_INTR
#define CAM_I2C_DEVID		XPAR_PS7_I2C_0_DEVICE_ID
#define CAM_I2C_IRPT_ID		XPAR_PS7_I2C_0_INTR
#define VDMA_DEVID			XPAR_AXIVDMA_0_DEVICE_ID
#define VDMA_MM2S_IRPT_ID	XPAR_FABRIC_AXI_VDMA_0_MM2S_INTROUT_INTR
#define VDMA_S2MM_IRPT_ID	XPAR_FABRIC_AXI_VDMA_0_S2MM_INTROUT_INTR
#define CAM_I2C_SCLK_RATE	100000

#define DDR_BASE_ADDR		XPAR_DDR_MEM_BASEADDR
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x0A000000)

#define GAMMA_BASE_ADDR     XPAR_AXI_GAMMACORRECTION_0_BASEADDR

#define MIPI_DPHY_BASE  (XPAR_MIPI_CSI2_RX_SUBSYST_0_BASEADDR + 0x1000)
#define MIPI_RX_BASE    XPAR_MIPI_CSI2_RX_SUBSYST_0_BASEADDR

#define XCSI_VC0_LINES_OFFSET  0x48  // Confirm in your PG232 version


using namespace digilent;

void print_mipi_status(void) {
    u32 base = MIPI_RX_BASE;
    u32 ccr = Xil_In32(base + XCSI_CCR_OFFSET);
    u32 csr = Xil_In32(base + XCSI_CSR_OFFSET);
    u32 isr = Xil_In32(base + XCSI_ISR_OFFSET);

    xil_printf("\r\n=== Xilinx MIPI CSI-2 RX Status ===\r\n");
    xil_printf(" CCR (0x00): 0x%08X  [Core Enable:%d  Soft Reset:%d]\r\n",
               ccr,
               (ccr & XCSI_CCR_COREENB_MASK) >> XCSI_CCR_COREENB_SHIFT,
               (ccr & XCSI_CCR_SOFTRESET_MASK) >> XCSI_CCR_SOFTRESET_SHIFT);

    xil_printf(" CSR (0x10): 0x%08X  [PktCnt:%u  SP FIFO Full:%d  NotEmpty:%d  LineBufFull:%d]\r\n",
               csr,
               (csr & XCSI_CSR_PKTCOUNT_MASK) >> XCSI_CSR_PKTCOUNT_SHIFT,
               (csr & XCSI_CSR_SPFIFOFULL_MASK) ? 1 : 0,
               (csr & XCSI_CSR_SPFIFONE_MASK)   ? 1 : 0,
               (csr & XCSI_CSR_SLBF_MASK)       ? 1 : 0);

    xil_printf(" ISR (0x24): 0x%08X\r\n", isr);

    if (isr & XCSI_ISR_FR_MASK)       xil_printf("  * Frame Received\r\n");
    if (isr & XCSI_ISR_VCXFE_MASK)     xil_printf("  * VCx Frame Level Error\r\n");
    if (isr & (1U<<22))                xil_printf("  * Word Count Corruption\r\n");

    u32 pcr = Xil_In32(base + 0x04);
    xil_printf(" PCR (0x04): 0x%08X  [Max Lanes:%d  Active Lanes:%d]\r\n",
               pcr,
               (pcr & XCSI_PCR_MAXLANES_MASK) >> XCSI_PCR_MAXLANES_SHIFT,
               (pcr & XCSI_PCR_ACTLANES_MASK) >> XCSI_PCR_ACTLANES_SHIFT);

    u32 clkinfr = Xil_In32(base + XCSI_CLKINFR_OFFSET);
    xil_printf(" Clock Lane Info (0x3C): 0x%08X  [Stop State:%d]\r\n",
               clkinfr,
               (clkinfr & XCSI_CLKINFR_STOP_MASK) ? 1 : 0);

    u32 l0infr = Xil_In32(base + XCSI_L0INFR_OFFSET);
    u32 l1infr = Xil_In32(base + XCSI_L1INFR_OFFSET);
    xil_printf(" Lane 0 Info (0x40): 0x%08X  [Stop:%d  SkewCalHS:%d  SoTErr:%d  SoTSyncErr:%d]\r\n",
               l0infr,
               (l0infr & XCSI_LXINFR_STOP_MASK) ? 1 : 0,
               (l0infr & XCSI_LXINFR_SKEWCALHS_MASK) ? 1 : 0,
               (l0infr & XCSI_LXINFR_SOTERR_MASK) ? 1 : 0,
               (l0infr & XCSI_LXINFR_SOTSYNCERR_MASK) ? 1 : 0);
    xil_printf(" Lane 1 Info (0x44): 0x%08X  [Stop:%d  SkewCalHS:%d  SoTErr:%d  SoTSyncErr:%d]\r\n",
               l1infr,
               (l1infr & XCSI_LXINFR_STOP_MASK) ? 1 : 0,
               (l1infr & XCSI_LXINFR_SKEWCALHS_MASK) ? 1 : 0,
               (l1infr & XCSI_LXINFR_SOTERR_MASK) ? 1 : 0,
               (l1infr & XCSI_LXINFR_SOTSYNCERR_MASK) ? 1 : 0);

    u32 spktr = Xil_In32(base + XCSI_SPKTR_OFFSET);
    xil_printf(" Short Packet FIFO (0x30): 0x%08X  [VC:%d  DataType:0x%02X  Data:0x%04X]\r\n",
               spktr,
               (spktr & XCSI_SPKTR_VC_MASK) >> XCSI_SPKTR_VC_SHIFT,
               (spktr & XCSI_SPKTR_DT_MASK),
               (spktr & XCSI_SPKTR_DATA_MASK) >> XCSI_SPKTR_DATA_SHIFT);

    u32 vc0inf1 = Xil_In32(base + XCSI_VC0INF1R_OFFSET);
    u32 vc0inf2 = Xil_In32(base + XCSI_VC0INF2R_OFFSET);
    xil_printf(" VC0 Image Info1 (0x60): 0x%08X  [LineCount:%u  ByteCount:%u]\r\n",
               vc0inf1,
               (vc0inf1 & XCSI_VCXINF1R_LINECOUNT_MASK) >> XCSI_VCXINF1R_LINECOUNT_SHIFT,
               (vc0inf1 & XCSI_VCXINF1R_BYTECOUNT_MASK));
    xil_printf(" VC0 Image Info2 (0x64): 0x%08X  [DataType:0x%02X]\r\n",
               vc0inf2,
               (vc0inf2 & XCSI_VCXINF2R_DATATYPE_MASK));

    u32 dphy_base = XPAR_MIPI_CSI2_RX_SUBSYST_0_BASEADDR + 0x1000;
    xil_printf("D-PHY SR: 0x%08X\r\n", Xil_In32(dphy_base + 0x04));
    xil_printf("D-PHY CR: 0x%08X\r\n", Xil_In32(dphy_base + 0x00));
}

// Helper: Print VDMA S2MM (write from camera) status
void print_vdma_s2mm_status() {
    u32 base = XPAR_AXIVDMA_0_BASEADDR;

    // Correct offsets from xaxivdma_hw.h and PG020 (S2MM starts at 0x30)
    u32 s2mm_dmacr = Xil_In32(base + 0x30);   // S2MM_VDMACR (Control)
    u32 s2mm_dmasr = Xil_In32(base + 0x34);   // S2MM_VDMASR (Status)
    xil_printf("\r\n=== VDMA S2MM (Camera → DDR) Status ===\r\n");
    xil_printf(" S2MM_VDMACR (Control): 0x%08X\r\n", s2mm_dmacr);
    xil_printf(" S2MM_VDMASR (Status):  0x%08X\r\n", s2mm_dmasr);

    // Interrupt status bits (in SR)
    if (s2mm_dmasr & XAXIVDMA_IXR_FRMCNT_MASK)
        xil_printf(" IOC_Irq: Interrupt on Complete (frame/descriptor finished)\r\n");

    if (s2mm_dmasr & XAXIVDMA_IXR_DELAYCNT_MASK)
        xil_printf("Dly_Irq: Delay interrupt\r\n");

    if (s2mm_dmasr & XAXIVDMA_IXR_ERROR_MASK)
        xil_printf("Err_Irq: Error interrupt active (check error bits below)\r\n");

    // Run / Halted state
    if (!(s2mm_dmacr & XAXIVDMA_CR_RUNSTOP_MASK))
        xil_printf(" WARNING: S2MM channel is HALTED (Run/Stop bit = 0)\r\n");

    // Bonus: show common error flags (bits 4–11 in SR)
    if (s2mm_dmasr & XAXIVDMA_SR_ERR_ALL_MASK) {
        xil_printf("  -> DMA Errors:\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_INTERNAL_MASK) xil_printf("     Internal error\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_FSZ_LESS_MASK) xil_printf("     Frame size LESS than expected\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_LSZ_LESS_MASK) xil_printf("     Line size LESS than expected\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_FSZ_MORE_MASK) xil_printf("     Frame size MORE than expected\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_SLAVE_MASK)    xil_printf("     Slave error\r\n");
        if (s2mm_dmasr & XAXIVDMA_SR_ERR_DECODE_MASK)   xil_printf("     Decode error\r\n");
    }
}

void pipeline_mode_change(AXI_VDMA<ScuGicInterruptController>& vdma_driver,
                          OV5640& cam,
                          VideoOutput& vid,
                          Resolution res,
                          OV5640_cfg::mode_t mode)
{
    xil_printf("\r\n=== Starting mode change to mode %d ===\r\n", mode);

	// 1. Stop everything cleanly
	vdma_driver.resetWrite();
	// 2. Assert CSI-2 RX reset (bit 1 = soft reset)
	XCsiSs_WriteReg(MIPI_RX_BASE, XCSI_CCR_OFFSET, 0x00000002);
	// 3. De-assert reset but do NOT enable yet
	XCsiSs_WriteReg(MIPI_RX_BASE, XCSI_CCR_OFFSET, 0x00000000);
	cam.reset();

	vdma_driver.configureWrite(timing[static_cast<int>(res)].h_active,
								   timing[static_cast<int>(res)].v_active);
	Xil_Out32(GAMMA_BASE_ADDR, 3);
	cam.init();

	vdma_driver.enableWrite();
	XCsiSs_WriteReg(MIPI_RX_BASE, XCSI_CCR_OFFSET, 0x00000001);

	cam.set_mode(mode);
	cam.set_awb(OV5640_cfg::awb_t::AWB_ADVANCED);


	vid.reset();
	vdma_driver.resetRead();
	vid.configure(res);
	vdma_driver.configureRead(timing[static_cast<int>(res)].h_active, timing[static_cast<int>(res)].v_active);

	vid.enable();
	vdma_driver.enableRead();


	print_mipi_status();
	print_vdma_s2mm_status();
}

static void cli_readline(char *buf, size_t maxlen)
{
	size_t idx = 0;
	while (1)
	{
		u32 ch = Xil_In32(STDOUT_BASEADDRESS + XUARTPS_FIFO_OFFSET);
		char c = (char)(ch & 0xFF);

		if (c == '\r' || c == '\n')
		{
			xil_printf("\r\n");
			buf[idx] = '\0';
			return;
		}
		else if ((c == '\b' || c == 127) && idx > 0)
		{
			idx--;
			xil_printf("\b \b");
		}
		else if (idx < maxlen - 1 && c >= 32 && c <= 126)
		{
			buf[idx++] = c;
			xil_printf("%c", c);
		}
	}
}

static bool parse_hex_u16(const char *s, uint16_t &out)
{
	out = 0;
	if (s[0] == '0' && s[1] == 'x')
		s += 2;

	while (*s)
	{
		char c = *s++;
		uint8_t v;
		if (c >= '0' && c <= '9') v = c - '0';
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
	if (!parse_hex_u16(s, tmp) || tmp > 0xFF)
		return false;
	out = (uint8_t)tmp;
	return true;
}

static void cmd_resolution(AXI_VDMA<ScuGicInterruptController>& vdma,
                           OV5640& cam,
                           VideoOutput& vid)
{
	xil_printf(
		"\r\nResolution options:\r\n"
		"  1) 1280x720 @60\r\n"
		"  2) 1920x1080 @15\r\n"
		"  3) 1920x1080 @30\r\n"
		"  4) 640x480 @15\r\n"
		"  5) 1280x720 @15\r\n"
		"  6) test pattern\r\n"
		"> ");

	char line[16];
	cli_readline(line, sizeof(line));

	switch (line[0])
	{
	case '1':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R1280_720_60_PP,
			OV5640_cfg::MODE_720P_1280_720_60fps);
		break;
	case '2':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R1920_1080_60_PP,
			OV5640_cfg::MODE_1080P_1920_1080_15fps);
		break;
	case '3':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R1920_1080_60_PP,
			OV5640_cfg::MODE_1080P_1920_1080_30fps);
		break;
	case '4':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R640_480_60_NN,
			OV5640_cfg::MODE_480P_640_480_15FPS);
		break;
	case '5':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R640_480_60_NN,
			OV5640_cfg::MODE_720P_1280_720_15fps);
		break;
	case '6':
		cam.set_test(OV5640_cfg::TEST_EIGHT_COLOR_BAR);
	    xil_printf("Test pattern enabled (8-color bars).\r\n");
	    break;
	default:
		xil_printf("Invalid selection\r\n");
		return;
	}

	xil_printf("Resolution changed.\r\n");
}

static void cmd_reg_write(OV5640& cam)
{
	char line[32];
	uint16_t addr;
	uint8_t val;

	xil_printf("Register address (hex): ");
	cli_readline(line, sizeof(line));
	if (!parse_hex_u16(line, addr))
	{
		xil_printf("Invalid hex\r\n");
		return;
	}

	xil_printf("Register value (hex): ");
	cli_readline(line, sizeof(line));
	if (!parse_hex_u8(line, val))
	{
		xil_printf("Invalid hex\r\n");
		return;
	}

	cam.writeReg(addr, val);
	xil_printf("Wrote 0x%02X to 0x%04X\r\n", val, addr);
}


static void cmd_reg_read(OV5640& cam)
{
	char line[32];
	uint16_t addr;
	uint8_t val;

	xil_printf("Register address (hex): ");
	cli_readline(line, sizeof(line));
	if (!parse_hex_u16(line, addr))
	{
		xil_printf("Invalid hex\r\n");
		return;
	}

	cam.readReg(addr, val);
	xil_printf("0x%04X = 0x%02X\r\n", addr, val);
}


static void cmd_liquid_lens(OV5640& cam)
{
	char line[16];
	uint8_t val;

	xil_printf("Liquid lens value (hex): ");
	cli_readline(line, sizeof(line));
	if (!parse_hex_u8(line, val))
	{
		xil_printf("Invalid hex\r\n");
		return;
	}

	cam.writeRegLiquid(val);
	xil_printf("Liquid lens set to 0x%02X\r\n", val);
}

// Polls for completed frames and writes to file
static void cmd_capture(AXI_VDMA<ScuGicInterruptController>& vdma, FIL& fil) {
    const int num_frames = 10;  // Change this to capture more/less frames

    u32 prev_cnt = vdma.getCompletedFrameCntWrite();
    xil_printf("Starting capture of %d frames...\r\n", num_frames);

    for (int i = 0; i < num_frames; i++) {
        // Poll until a new frame is completed
        while (vdma.getCompletedFrameCntWrite() == prev_cnt) {}

        prev_cnt++;
        u32 frame_index = (prev_cnt - 1) % vdma.getNumBuffers();
        u8* frame_ptr = (u8*)vdma.getWriteBufferAddr(frame_index);
        u32 frame_size = vdma.getWriteFrameSize();

        // Flush cache for coherency (ensure CPU sees latest DMA data)
        Xil_DCacheFlushRange((INTPTR)frame_ptr, frame_size);

        UINT bw;
        FRESULT res = f_write(&fil, frame_ptr, frame_size, &bw);
        if (res != FR_OK || bw != frame_size) {
            xil_printf("Write failed for frame %d (res=%d, bw=%u)\r\n", i, res, bw);
            return;
        }
        f_sync(&fil);  // Flush to SD
        xil_printf("Captured frame %d (%u bytes) to video.raw\r\n", i, frame_size);
    }

    xil_printf("Capture complete. File size updated on SD.\r\n");
}

static void print_menu()
{
	xil_printf(
		"\r\n==== PCAM CLI ====\r\n"
		"r  - Change resolution\r\n"
		"l  - Liquid lens\r\n"
		"wr - Write OV5640 register\r\n"
		"rr - Read OV5640 register\r\n"
		"c  - Capture 10 frames to video.raw\r\n"
		"q  - Quit\r\n"
		"> ");
}

int main()
{
	init_platform();

	xil_printf("=== Running 2-LANE MIPI BUILD - built %s %s ===\r\n", __DATE__, __TIME__);

	FATFS fs;
	FRESULT res = f_mount(&fs, "0:", 1);
	if (res != FR_OK) { 
		xil_printf("Mount failed\r\n"); 
		return -1; 
	}

	ScuGicInterruptController irpt_ctl(IRPT_CTL_DEVID);
	PS_GPIO<ScuGicInterruptController> gpio(GPIO_DEVID, irpt_ctl, GPIO_IRPT_ID);
	PS_IIC<ScuGicInterruptController> iic(CAM_I2C_DEVID, irpt_ctl, CAM_I2C_IRPT_ID, 100000);

	OV5640 cam(iic, gpio);
	AXI_VDMA<ScuGicInterruptController> vdma(
		VDMA_DEVID, MEM_BASE_ADDR, irpt_ctl,
		VDMA_MM2S_IRPT_ID, VDMA_S2MM_IRPT_ID);

	VideoOutput vid(XPAR_VTC_0_DEVICE_ID, XPAR_VIDEO_DYNCLK_DEVICE_ID);


	pipeline_mode_change(vdma, cam, vid,
		Resolution::R640_480_60_NN,
		OV5640_cfg::MODE_480P_640_480_15FPS);

	FIL fil;
	res = f_open(&fil, "video.raw", FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK) { 
		xil_printf("Open failed\r\n"); 
		return -1; 
	}


	while (1)
	{
		char cmd[16];
		print_menu();
		cli_readline(cmd, sizeof(cmd));

		if (!strcmp(cmd, "r"))
			cmd_resolution(vdma, cam, vid);
		else if (!strcmp(cmd, "l"))
			cmd_liquid_lens(cam);
		else if (!strcmp(cmd, "wr"))
			cmd_reg_write(cam);
		else if (!strcmp(cmd, "rr"))
			cmd_reg_read(cam);
		else if (!strcmp(cmd, "c")) 
			cmd_capture(vdma, fil);
		else if (!strcmp(cmd, "q"))
			break;
		else
			xil_printf("Unknown command\r\n");
	}

	cleanup_platform();
	return 0;
}
