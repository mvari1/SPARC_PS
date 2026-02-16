#include "xparameters.h"
#include "xuartps_hw.h"

#include "platform/platform.h"
#include "ov5640/OV5640.h"
#include "ov5640/ScuGicInterruptController.h"
#include "ov5640/PS_GPIO.h"
#include "ov5640/AXI_VDMA.h"
#include "ov5640/PS_IIC.h"

#include "MIPI_D_PHY_RX.h"
#include "MIPI_CSI_2_RX.h"

#define IRPT_CTL_DEVID 		XPAR_PS7_SCUGIC_0_DEVICE_ID
#define GPIO_DEVID			XPAR_PS7_GPIO_0_DEVICE_ID
#define GPIO_IRPT_ID			XPAR_PS7_GPIO_0_INTR
#define CAM_I2C_DEVID		XPAR_PS7_I2C_0_DEVICE_ID
#define CAM_I2C_IRPT_ID		XPAR_PS7_I2C_0_INTR
#define VDMA_DEVID			XPAR_AXIVDMA_0_DEVICE_ID
#define VDMA_MM2S_IRPT_ID	XPAR_FABRIC_AXI_VDMA_0_MM2S_INTROUT_INTR
#define VDMA_S2MM_IRPT_ID	XPAR_FABRIC_AXI_VDMA_0_S2MM_INTROUT_INTR
#define CAM_I2C_SCLK_RATE	100000

#define DDR_BASE_ADDR		XPAR_DDR_MEM_BASEADDR
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x0A000000)

#define GAMMA_BASE_ADDR     XPAR_AXI_GAMMACORRECTION_0_BASEADDR

using namespace digilent;

void pipeline_mode_change(AXI_VDMA<ScuGicInterruptController>& vdma_driver, OV5640& cam, VideoOutput& vid, Resolution res, OV5640_cfg::mode_t mode)
{
	//Bring up input pipeline back-to-front
	{
		vdma_driver.resetWrite();
		MIPI_CSI_2_RX_mWriteReg(XPAR_MIPI_CSI_2_RX_0_S_AXI_LITE_BASEADDR, CR_OFFSET, (CR_RESET_MASK & ~CR_ENABLE_MASK));
		MIPI_D_PHY_RX_mWriteReg(XPAR_MIPI_D_PHY_RX_0_S_AXI_LITE_BASEADDR, CR_OFFSET, (CR_RESET_MASK & ~CR_ENABLE_MASK));
		cam.reset();
	}

	{
		vdma_driver.configureWrite(timing[static_cast<int>(res)].h_active, timing[static_cast<int>(res)].v_active);
		Xil_Out32(GAMMA_BASE_ADDR, 3); // Set Gamma correction factor to 1/1.8
		//TODO CSI-2, D-PHY config here
		cam.init();
	}

	{
		vdma_driver.enableWrite();
		MIPI_CSI_2_RX_mWriteReg(XPAR_MIPI_CSI_2_RX_0_S_AXI_LITE_BASEADDR, CR_OFFSET, CR_ENABLE_MASK);
		MIPI_D_PHY_RX_mWriteReg(XPAR_MIPI_D_PHY_RX_0_S_AXI_LITE_BASEADDR, CR_OFFSET, CR_ENABLE_MASK);
		cam.set_mode(mode);
		cam.set_awb(OV5640_cfg::awb_t::AWB_ADVANCED);
	}

	//Bring up output pipeline back-to-front
	{
		vid.reset();
		vdma_driver.resetRead();
	}

	{
		vid.configure(res);
		vdma_driver.configureRead(timing[static_cast<int>(res)].h_active, timing[static_cast<int>(res)].v_active);
	}

	{
		vid.enable();
		vdma_driver.enableRead();
	}
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
		"  5) 640x480 @30\r\n"
		"  6) 640x480 @60\r\n"
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
			OV5640_cfg::MODE_480P_640_480_30FPS);
		break;
	case '6':
		pipeline_mode_change(vdma, cam, vid,
			Resolution::R640_480_60_NN,
			OV5640_cfg::MODE_480P_640_480_60FPS);
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


static void print_menu()
{
	xil_printf(
		"\r\n==== PCAM CLI ====\r\n"
		"r  - Change resolution\r\n"
		"l  - Liquid lens\r\n"
		"wr - Write OV5640 register\r\n"
		"rr - Read OV5640 register\r\n"
		"q  - Quit\r\n"
		"> ");
}

int main()
{
	init_platform();

	ScuGicInterruptController irpt_ctl(IRPT_CTL_DEVID);
	PS_GPIO<ScuGicInterruptController> gpio(GPIO_DEVID, irpt_ctl, GPIO_IRPT_ID);
	PS_IIC<ScuGicInterruptController> iic(CAM_I2C_DEVID, irpt_ctl, CAM_I2C_IRPT_ID, 100000);

	OV5640 cam(iic, gpio);
	AXI_VDMA<ScuGicInterruptController> vdma(
		VDMA_DEVID, MEM_BASE_ADDR, irpt_ctl,
		VDMA_MM2S_IRPT_ID, VDMA_S2MM_IRPT_ID);

	VideoOutput vid(XPAR_VTC_0_DEVICE_ID, XPAR_VIDEO_DYNCLK_DEVICE_ID);

	pipeline_mode_change(vdma, cam, vid,
		Resolution::R1920_1080_60_PP,
		OV5640_cfg::MODE_1080P_1920_1080_30fps);

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
		else if (!strcmp(cmd, "q"))
			break;
		else
			xil_printf("Unknown command\r\n");
	}

	cleanup_platform();
	return 0;
}
