/*
 * OV5640.h
 *
 *  Created on: May 26, 2016
 *      Author: Elod
 */

#ifndef OV5640_H_
#define OV5640_H_

#include <sstream>
#include <iostream>
#include <cstdio>
#include <climits>

#include "I2C_Client.h"
#include "GPIO_Client.h"
#include "../hdmi/VideoOutput.h"

#define SIZEOF_ARRAY(x) sizeof(x)/sizeof(x[0])
#define MAP_ENUM_TO_CFG(en, cfg) en, cfg, SIZEOF_ARRAY(cfg)

namespace digilent {

typedef enum {OK=0, ERR_LOGICAL, ERR_GENERAL} Errc;

namespace OV5640_cfg {
	using config_word_t = struct { uint16_t addr; uint8_t data; } ;
	using mode_t = enum { MODE_480P_640_480_15FPS = 0, MODE_480P_640_480_30FPS, MODE_480P_640_480_60FPS, MODE_720P_1280_720_60fps, MODE_1080P_1920_1080_15fps,
		MODE_1080P_1920_1080_30fps, MODE_1080P_1920_1080_30fps_336M_MIPI,
		MODE_1080P_1920_1080_30fps_336M_1LANE_MIPI, MODE_END } ;
	using config_modes_t = struct { mode_t mode; config_word_t const* cfg; size_t cfg_size; };
	using test_t = enum { TEST_DISABLED = 0, TEST_EIGHT_COLOR_BAR, TEST_END };
	using awb_t = enum { AWB_DISABLED = 0, AWB_SIMPLE, AWB_ADVANCED, AWB_END };
	using config_awb_t = struct { awb_t awb; config_word_t const* cfg; size_t cfg_size; };
	using isp_format_t = enum { ISP_RAW = 0, ISP_RGB, ISP_END };
	uint16_t const OV5640_REG_PRE_ISP_TEST_SET1 = 0x503D;
	uint16_t const OV5640_FORMAT_MUX_CONTROL = 0x501f;

	config_word_t const cfg_advanced_awb_[] =
	{
		// Enable Advanced AWB
		{0x3406 ,0x00},
		{0x5192 ,0x04},
		{0x5191 ,0xf8},
		{0x518d ,0x26},
		{0x518f ,0x42},
		{0x518e ,0x2b},
		{0x5190 ,0x42},
		{0x518b ,0xd0},
		{0x518c ,0xbd},
		{0x5187 ,0x18},
		{0x5188 ,0x18},
		{0x5189 ,0x56},
		{0x518a ,0x5c},
		{0x5186 ,0x1c},
		{0x5181 ,0x50},
		{0x5184 ,0x20},
		{0x5182 ,0x11},
		{0x5183 ,0x00},
		{0x5001 ,0x03}
	};

	config_word_t const cfg_simple_awb_[] =
	{
		// Disable Advanced AWB
		{0x518d ,0x00},
		{0x518f ,0x20},
		{0x518e ,0x00},
		{0x5190 ,0x20},
		{0x518b ,0x00},
		{0x518c ,0x00},
		{0x5187 ,0x10},
		{0x5188 ,0x10},
		{0x5189 ,0x40},
		{0x518a ,0x40},
		{0x5186 ,0x10},
		{0x5181 ,0x58},
		{0x5184 ,0x25},
		{0x5182 ,0x11},

		// Enable simple AWB
		{0x3406 ,0x00},
		{0x5183 ,0x80},
		{0x5191 ,0xff},
		{0x5192 ,0x00},
		{0x5001 ,0x03}
	};

	config_word_t const cfg_disable_awb_[] =
	{
		{0x5001 ,0x02}
	};

	config_word_t const cfg_480p_15fps_[] =
	{// 640 x 480 binned, RAW10, MIPISCLK=~372M (adjusted), SCLK=24MHz, PCLK=~74.25MHz (to match demo CSI RX lane rate ~371 Mbps/lane)

		{0x3035, 0x11}, // Changed from 0x21: system clk /1 (doubles base PCLK from 28MHz to 56MHz), MIPI /1

		{0x3a02, 0x1e}, // Changed from 0x0b: 60Hz max exposure high byte, scaled to ~3*new VTS=7833 (0x1e99) for night mode ~5fps min
		{0x3a03, 0x99}, // Changed from 0x88: 60Hz max exposure low byte
		{0x3a14, 0x1e}, // Changed from 0x0b: 50Hz max exposure high byte, same scaling
		{0x3a15, 0x99}, // Changed from 0x88: 50Hz max exposure low byte

		{0x3036, 0x5d}, // Changed from 0x46: PLL multiplier =93 (70 * (74.25/56) ≈93) to boost PCLK from 56MHz to ~74.25MHz
		{0x3037, 0x13}, // PLL root divide - /2, PLL pre-divider = 3
		{0x3108, 0x01}, // CLK /2

		//[3:0]=1111 MD2P,MD2N,MCP,MCN input
		{0x3017, 0xff},
		//[7:2]=1111111 MD1P,MD1N, D3:0 input
		{0x3018, 0xff},

		//[6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
		{0x3034, 0x1A},

		// Aplication Note
		{0x3630, 0x36},
		{0x3633, 0x12},
		{0x3731, 0x12},
		{0x3600, 0x08},
		{0x3636, 0x03},
		{0x3634, 0x40},
		{0x4001, 0x02}, // BLC start from line 2
		{0x4005, 0x1a}, // BLC always update
		{0x3000, 0x00}, // enable blocks
		{0x3004, 0xff}, // enable clocks
		{0x302e, 0x00},

		{0x3a0f, 0x30}, // stable range in high
		{0x3a10, 0x28}, // stable range in low
		{0x3a1b, 0x30}, // stable range out high
		{0x3a1e, 0x26}, // stable range out low
		{0x3a11, 0x60}, // fast zone high
		{0x3a1f, 0x14}, // fast zone low

		// Lens correction
		{0x5800, 0x23},
		{0x5801, 0x14},
		{0x5802, 0x0f},
		{0x5803, 0x0f},
		{0x5804, 0x12},
		{0x5805, 0x26},
		{0x5806, 0x0c},
		{0x5807, 0x08},
		{0x5808, 0x05},
		{0x5809, 0x05},
		{0x580a, 0x08},
		{0x580b, 0x0d},
		{0x580c, 0x08},
		{0x580d, 0x03},
		{0x580e, 0x00},
		{0x580f, 0x00},
		{0x5810, 0x03},
		{0x5811, 0x09},
		{0x5812, 0x07},
		{0x5813, 0x03},
		{0x5814, 0x00},
		{0x5815, 0x01},
		{0x5816, 0x03},
		{0x5817, 0x08},
		{0x5818, 0x0d},
		{0x5819, 0x08},
		{0x581a, 0x05},
		{0x581b, 0x06},
		{0x581c, 0x08},
		{0x581d, 0x0e},
		{0x581e, 0x29},
		{0x581f, 0x17},
		{0x5820, 0x11},
		{0x5821, 0x11},
		{0x5822, 0x15},
		{0x5823, 0x28},
		{0x5824, 0x46},
		{0x5825, 0x26},
		{0x5826, 0x08},
		{0x5827, 0x26},
		{0x5828, 0x64},
		{0x5829, 0x26},
		{0x582a, 0x24},
		{0x582b, 0x22},
		{0x582c, 0x24},
		{0x582d, 0x24},
		{0x582e, 0x06},
		{0x582f, 0x22},
		{0x5830, 0x40},
		{0x5831, 0x42},
		{0x5832, 0x24},
		{0x5833, 0x26},
		{0x5834, 0x24},
		{0x5835, 0x22},
		{0x5836, 0x22},
		{0x5837, 0x26},
		{0x5838, 0x44},
		{0x5839, 0x24},
		{0x583a, 0x26},
		{0x583b, 0x28},
		{0x583c, 0x42},
		{0x583d, 0xce}, // lenc BR offset

		{0x5180, 0xff}, // AWB B block
		{0x5181, 0xf2}, // AWB control
		{0x5182, 0x00}, // [7:4] max local counter, [3:0] max fast counter
		{0x5183, 0x14}, // AWB advanced
		{0x5184, 0x25},
		{0x5185, 0x24},
		{0x5186, 0x09},
		{0x5187, 0x09},
		{0x5188, 0x09},
		{0x5189, 0x75},
		{0x518a, 0x54},
		{0x518b, 0xe0},
		{0x518c, 0xb2},
		{0x518d, 0x42},
		{0x518e, 0x3d},
		{0x518f, 0x56},
		{0x5190, 0x46},
		{0x5191, 0xf8}, // AWB top limit
		{0x5192, 0x04}, // AWB bottom limit
		{0x5193, 0x70}, // red limit
		{0x5194, 0xf0}, // green limit
		{0x5195, 0xf0}, // blue limit
		{0x5196, 0x03}, // AWB control
		{0x5197, 0x01}, // local limit
		{0x5198, 0x04},
		{0x5199, 0x12},
		{0x519a, 0x04},
		{0x519b, 0x00},
		{0x519c, 0x06},
		{0x519d, 0x82},
		{0x519e, 0x38}, // AWB control

		// Windowing / readout area (crop from full array, common for VGA)
		//[3:0]=0 X address start high byte
		{0x3800, 0x00},
		//[7:0]=0 X address start low byte
		{0x3801, 0x00},
		//[2:0]=0 Y address start high byte
		{0x3802, 0x00},
		//[7:0]=0 Y address start low byte
		{0x3803, 0x04}, //FIX ME potentially

		// Added: X address end high/low (full array width)
		{0x3804, 0x0a}, // HW=2623 (0x0a3f)
		{0x3805, 0x3f},

		// Added: Y address end high/low (full array height adjusted for VGA)
		{0x3806, 0x07}, // VH=1947 (0x079b)
		{0x3807, 0x9b},

		// Added: Output width (DVPHO=640)
		{0x3808, 0x02},
		{0x3809, 0x80},

		// Added: Output height (DVPVO=480)
		{0x380a, 0x01},
		{0x380b, 0xe0},

		//HTS line exposure time in # of pixels
		{0x380c, 0x07}, // Added: HTS=1896 (0x0768), standard for VGA binning
		{0x380d, 0x68},

		//VTS frame exposure time in # lines
		{0x380e, 0x0a}, // Changed/Added: VTS=2611 (0x0a33) for 15fps at PCLK~74.25MHz (VTS ≈ PCLK / (HTS * fps) ≈74.25e6 / (1896 * 15) ≈2611)
		{0x380f, 0x33},

		// Added: Vertical offset (common for VGA)
		{0x3813, 0x06}, // V offset=6

		//[7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
		{0x4300, 0x00},
		//[2:0]=0x3 Format select ISP RAW (DPC) -- Corrected from 0x03: Should be 0x01 for ISP RAW output
		{0x501f, 0x01},

	};

	config_word_t const cfg_480p_30fps_[] =
	{
		// Initial setup (common)
		{0x3103, 0x11}, // System clock from pad
		{0x3008, 0x82}, // Software reset (delay 5ms)
		{0x3008, 0x42}, // Software power down
		{0x3103, 0x03}, // System clock from PLL
		{0x3017, 0xff}, // IO output enable
		{0x3018, 0xff},
		{0x3034, 0x1a}, // 10-bit mode
		{0x3037, 0x13}, // PLL pre-div
		{0x3108, 0x01}, // Clock dividers
		{0x3630, 0x36},
		{0x3631, 0x0e},
		{0x3632, 0xe2},
		{0x3633, 0x12},
		{0x3621, 0xe0},
		{0x3704, 0xa0},
		{0x3703, 0x5a},
		{0x3715, 0x78},
		{0x3717, 0x01},
		{0x370b, 0x60},
		{0x3705, 0x1a},
		{0x3905, 0x02},
		{0x3906, 0x10},
		{0x3901, 0x0a},
		{0x3731, 0x12},
		{0x3600, 0x08}, // VCM
		{0x3601, 0x33},
		{0x302d, 0x60},
		{0x3620, 0x52},
		{0x371b, 0x20},
		{0x471c, 0x50},
		{0x3a13, 0x43}, // Pre-gain
		{0x3a18, 0x00}, // Gain ceiling
		{0x3a19, 0xf8},
		{0x3635, 0x13},
		{0x3636, 0x03},
		{0x3634, 0x40},
		{0x3622, 0x01},

		// VGA 30 FPS timings (PCLK 56MHz)
		{0x3035, 0x11}, // PLL
		{0x3036, 0x46}, // PLL
		{0x3c07, 0x08}, // Light meter threshold
		{0x3820, 0x41}, // ISP flip on
		{0x3821, 0x07}, // ISP mirror on, H bin on
		{0x3814, 0x31}, // X inc
		{0x3815, 0x31}, // Y inc
		{0x3800, 0x00}, // HS
		{0x3801, 0x00},
		{0x3802, 0x00}, // VS
		{0x3803, 0x04},
		{0x3804, 0x0a}, // HW
		{0x3805, 0x3f},
		{0x3806, 0x07}, // VH
		{0x3807, 0x9b},
		{0x3808, 0x02}, // DVPHO (640)
		{0x3809, 0x80},
		{0x380a, 0x01}, // DVPVO (480)
		{0x380b, 0xe0},
		{0x380c, 0x07}, // HTS (1896)
		{0x380d, 0x68},
		{0x380e, 0x03}, // VTS (984)
		{0x380f, 0xd8},
		{0x3813, 0x06}, // V offset
		{0x3618, 0x00},
		{0x3612, 0x29},
		{0x3709, 0x52},
		{0x370c, 0x03},
		{0x3a02, 0x17}, // 60Hz max exp (night mode 5 FPS)
		{0x3a03, 0x10},
		{0x3a14, 0x17}, // 50Hz max exp
		{0x3a15, 0x10},
		{0x4004, 0x02}, // BLC lines
		{0x3002, 0x1c}, // Reset FIFOs
		{0x3006, 0xc3}, // Disable JPEG clocks
		{0x4713, 0x03}, // JPEG mode 3
		{0x4407, 0x04}, // Quant scale
		{0x460b, 0x35},
		{0x460c, 0x22},
		{0x4837, 0x22}, // CLK div
		{0x3824, 0x02}, // CLK div
		{0x5001, 0xa3}, // ISP on (AWB, etc.)
		{0x3503, 0x00}, // AEC/AGC on

		// Change to RAW Bayer (key for demo pipeline)
		{0x4300, 0x00}, // Format control for RAW
		{0x501f, 0x03}, // ISP RAW

		// Enable streaming
		{0x3008, 0x02}
	};

	config_word_t const cfg_480p_60fps_[] =
	{
		// Initial setup (common)
		{0x3103, 0x11}, // System clock from pad
		{0x3008, 0x82}, // Software reset (delay 5ms)
		{0x3008, 0x42}, // Software power down
		{0x3103, 0x03}, // System clock from PLL
		{0x3017, 0xff}, // IO output enable
		{0x3018, 0xff},
		{0x3034, 0x1a}, // 10-bit mode
		{0x3037, 0x13}, // PLL pre-div
		{0x3108, 0x01}, // Clock dividers
		{0x3630, 0x36},
		{0x3631, 0x0e},
		{0x3632, 0xe2},
		{0x3633, 0x12},
		{0x3621, 0xe0},
		{0x3704, 0xa0},
		{0x3703, 0x5a},
		{0x3715, 0x78},
		{0x3717, 0x01},
		{0x370b, 0x60},
		{0x3705, 0x1a},
		{0x3905, 0x02},
		{0x3906, 0x10},
		{0x3901, 0x0a},
		{0x3731, 0x12},
		{0x3600, 0x08}, // VCM
		{0x3601, 0x33},
		{0x302d, 0x60},
		{0x3620, 0x52},
		{0x371b, 0x20},
		{0x471c, 0x50},
		{0x3a13, 0x43}, // Pre-gain
		{0x3a18, 0x00}, // Gain ceiling
		{0x3a19, 0xf8},
		{0x3635, 0x13},
		{0x3636, 0x03},
		{0x3634, 0x40},
		{0x3622, 0x01},

		// VGA 60 FPS timings (PCLK ~MHz)
		{0x3035, 0x21}, // PLL
		{0x3036, 0x46}, // PLL
		{0x3c07, 0x08}, // Light meter threshold
		{0x3820, 0x41}, // ISP flip on
		{0x3821, 0x07}, // ISP mirror on, H bin on
		{0x3814, 0x31}, // X inc
		{0x3815, 0x31}, // Y inc
		{0x3800, 0x00}, // HS
		{0x3801, 0x00},
		{0x3802, 0x00}, // VS
		{0x3803, 0x04},
		{0x3804, 0x0a}, // HW
		{0x3805, 0x3f},
		{0x3806, 0x07}, // VH
		{0x3807, 0x9b},
		{0x3808, 0x02}, // DVPHO (640)
		{0x3809, 0x80},
		{0x380a, 0x01}, // DVPVO (480)
		{0x380b, 0xe0},
		{0x380c, 0x07}, // HTS (1896)
		{0x380d, 0x68},
		{0x380e, 0x03}, // VTS (984)
		{0x380f, 0xd8},
		{0x3813, 0x06}, // V offset
		{0x3618, 0x00},
		{0x3612, 0x29},
		{0x3709, 0x52},
		{0x370c, 0x03},
		{0x3a02, 0x0b}, // 60Hz max exp (night mode 5 FPS)
		{0x3a03, 0x88},
		{0x3a14, 0x0b}, // 50Hz max exp
		{0x3a15, 0x88},
		{0x4004, 0x02}, // BLC lines
		{0x3002, 0x1c}, // Reset FIFOs
		{0x3006, 0xc3}, // Disable JPEG clocks
		{0x4713, 0x03}, // JPEG mode 3
		{0x4407, 0x04}, // Quant scale
		{0x460b, 0x35},
		{0x460c, 0x22},
		{0x4837, 0x22}, // CLK div
		{0x3824, 0x02}, // CLK div
		{0x5001, 0xa3}, // ISP on (AWB, etc.)
		{0x3503, 0x00}, // AEC/AGC on

		// Change to RAW Bayer (key for demo pipeline)
		{0x4300, 0x00}, // Format control for RAW
		{0x501f, 0x03}, // ISP RAW

		// Enable streaming
		{0x3008, 0x02}
	};

	config_word_t const cfg_720p_60fps_[] =
	{//1280 x 720 binned, RAW10, MIPISCLK=280M, SCLK=56Mz, PCLK=56M
		//PLL1 configuration
		//[7:4]=0010 System clock divider /2, [3:0]=0001 Scale divider for MIPI /1
		{0x3035, 0x21},
		//[7:0]=70 PLL multiplier
		{0x3036, 0x46},
		//[4]=0 PLL root divider /1, [3:0]=5 PLL pre-divider /1.5
		{0x3037, 0x05},
		//[5:4]=01 PCLK root divider /2, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
		{0x3108, 0x11},

		//[6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
		{0x3034, 0x1A},

		//[3:0]=0 X address start high byte
		{0x3800, (0 >> 8) & 0x0F},
		//[7:0]=0 X address start low byte
		{0x3801, 0 & 0xFF},
		//[2:0]=0 Y address start high byte
		{0x3802, (8 >> 8) & 0x07},
		//[7:0]=0 Y address start low byte
		{0x3803, 8 & 0xFF},

		//[3:0] X address end high byte
		{0x3804, (2619 >> 8) & 0x0F},
		//[7:0] X address end low byte
		{0x3805, 2619 & 0xFF},
		//[2:0] Y address end high byte
		{0x3806, (1947 >> 8) & 0x07},
		//[7:0] Y address end low byte
		{0x3807, 1947 & 0xFF},

		//[3:0]=0 timing hoffset high byte
		{0x3810, (0 >> 8) & 0x0F},
		//[7:0]=0 timing hoffset low byte
		{0x3811, 0 & 0xFF},
		//[2:0]=0 timing voffset high byte
		{0x3812, (0 >> 8) & 0x07},
		//[7:0]=0 timing voffset low byte
		{0x3813, 0 & 0xFF},

		//[3:0] Output horizontal width high byte
		{0x3808, (1280 >> 8) & 0x0F},
		//[7:0] Output horizontal width low byte
		{0x3809, 1280 & 0xFF},
		//[2:0] Output vertical height high byte
		{0x380a, (720 >> 8) & 0x7F},
		//[7:0] Output vertical height low byte
		{0x380b, 720 & 0xFF},

		//HTS line exposure time in # of pixels
		{0x380c, (1896 >> 8) & 0x1F},
		{0x380d, 1896 & 0xFF},
		//VTS frame exposure time in # lines
		{0x380e, (984 >> 8) & 0xFF},
		{0x380f, 984 & 0xFF},

		//[7:4]=0x3 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
		{0x3814, 0x31}, 
		//[7:4]=0x3 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
		{0x3815, 0x31},

		//[2]=0 ISP mirror, [1]=0 sensor mirror, [0]=1 horizontal binning
		{0x3821, 0x01},

		//little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
		{0x4837, 36}, // 1/56M*2

		//Undocumented anti-green settings
		{0x3618, 0x00}, // Removes vertical lines appearing under bright light
		{0x3612, 0x59},
		{0x3708, 0x64},
		{0x3709, 0x52},
		{0x370c, 0x03},

		//[7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
		{0x4300, 0x00},
		//[2:0]=0x3 Format select ISP RAW (DPC)
		{0x501f, 0x03}

	};
	config_word_t const cfg_1080p_15fps_[] =
	{//1920 x 1080 @ 15 fps, RAW10, MIPISCLK=210, SCLK=42MHz, PCLK=42M
		// PLL1 configuration
		// [7:4]=0100 System clock divider /4, [3:0]=0001 Scale divider for MIPI /1
		{0x3035, 0x41},
		// [7:0]=105 PLL multiplier
		{0x3036, 0x69},
		// [4]=0 PLL root divider /1, [3:0]=5 PLL pre-divider /1.5
		{0x3037, 0x05},
		// [5:4]=01 PCLK root divider /2, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
		{0x3108, 0x11},

		// [6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
		{0x3034, 0x1A},

		// [3:0]=0 X address start high byte
		{0x3800, (336 >> 8) & 0x0F},
		// [7:0]=0 X address start low byte
		{0x3801, 336 & 0xFF},
		// [2:0]=0 Y address start high byte
		{0x3802, (426 >> 8) & 0x07},
		// [7:0]=0 Y address start low byte
		{0x3803, 426 & 0xFF},

		// [3:0] X address end high byte
		{0x3804, (2287 >> 8) & 0x0F},
		// [7:0] X address end low byte
		{0x3805, 2287 & 0xFF},
		// [2:0] Y address end high byte
		{0x3806, (1529 >> 8) & 0x07},
		// [7:0] Y address end low byte
		{0x3807, 1529 & 0xFF},

		// [3:0]=0 timing hoffset high byte
		{0x3810, (16 >> 8) & 0x0F},
		// [7:0]=0 timing hoffset low byte
		{0x3811, 16 & 0xFF},
		// [2:0]=0 timing voffset high byte
		{0x3812, (12 >> 8) & 0x07},
		// [7:0]=0 timing voffset low byte
		{0x3813, 12 & 0xFF},

		// [3:0] Output horizontal width high byte
		{0x3808, (1920 >> 8) & 0x0F},
		// [7:0] Output horizontal width low byte
		{0x3809, 1920 & 0xFF},
		// [2:0] Output vertical height high byte
		{0x380a, (1080 >> 8) & 0x7F},
		// [7:0] Output vertical height low byte
		{0x380b, 1080 & 0xFF},

		// HTS line exposure time in # of pixels Tline=HTS/sclk
		{0x380c, (2500 >> 8) & 0x1F},
		{0x380d, 2500 & 0xFF},
		// VTS frame exposure time in # lines
		{0x380e, (1120 >> 8) & 0xFF},
		{0x380f, 1120 & 0xFF},

		// [7:4]=0x1 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
		{0x3814, 0x11},
		// [7:4]=0x1 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
		{0x3815, 0x11},

		// [2]=0 ISP mirror, [1]=0 sensor mirror, [0]=0 no horizontal binning
		{0x3821, 0x00},

		// little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
		{0x4837, 48}, // 1/42M*2

		// Undocumented anti-green settings
		{0x3618, 0x00}, // Removes vertical lines appearing under bright light
		{0x3612, 0x59},
		{0x3708, 0x64},
		{0x3709, 0x52},
		{0x370c, 0x03},

		// [7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
		{0x4300, 0x00},
		// [2:0]=0x3 Format select ISP RAW (DPC)
		{0x501f, 0x03}
	};
	config_word_t const cfg_1080p_30fps_[] =
	{//1920 x 1080 @ 30fps, RAW10, MIPISCLK=420, SCLK=84MHz, PCLK=84M
		//PLL1 configuration
		//[7:4]=0010 System clock divider /2, [3:0]=0001 Scale divider for MIPI /1
		{0x3035, 0x21}, // 30fps setting
		//[7:0]=105 PLL multiplier
		{0x3036, 0x69},
		//[4]=0 PLL root divider /1, [3:0]=5 PLL pre-divider /1.5
		{0x3037, 0x05},
		//[5:4]=01 PCLK root divider /2, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
		{0x3108, 0x11},

		//[6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
		{0x3034, 0x1A},

		//[3:0]=0 X address start high byte
		{0x3800, (336 >> 8) & 0x0F},
		//[7:0]=0 X address start low byte
		{0x3801, 336 & 0xFF},
		//[2:0]=0 Y address start high byte
		{0x3802, (426 >> 8) & 0x07},
		//[7:0]=0 Y address start low byte
		{0x3803, 426 & 0xFF},

		//[3:0] X address end high byte
		{0x3804, (2287 >> 8) & 0x0F},
		//[7:0] X address end low byte
		{0x3805, 2287 & 0xFF},
		//[2:0] Y address end high byte
		{0x3806, (1529 >> 8) & 0x07},
		//[7:0] Y address end low byte
		{0x3807, 1529 & 0xFF},

		//[3:0]=0 timing hoffset high byte
		{0x3810, (16 >> 8) & 0x0F},
		//[7:0]=0 timing hoffset low byte
		{0x3811, 16 & 0xFF},
		//[2:0]=0 timing voffset high byte
		{0x3812, (12 >> 8) & 0x07},
		//[7:0]=0 timing voffset low byte
		{0x3813, 12 & 0xFF},

		//[3:0] Output horizontal width high byte
		{0x3808, (1920 >> 8) & 0x0F},
		//[7:0] Output horizontal width low byte
		{0x3809, 1920 & 0xFF},
		//[2:0] Output vertical height high byte
		{0x380a, (1080 >> 8) & 0x7F},
		//[7:0] Output vertical height low byte
		{0x380b, 1080 & 0xFF},

		//HTS line exposure time in # of pixels Tline=HTS/sclk
		{0x380c, (2500 >> 8) & 0x1F},
		{0x380d, 2500 & 0xFF},
		//VTS frame exposure time in # lines
		{0x380e, (1120 >> 8) & 0xFF},
		{0x380f, 1120 & 0xFF},

		//[7:4]=0x1 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
		{0x3814, 0x11},
		//[7:4]=0x1 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
		{0x3815, 0x11},

		//[2]=0 ISP mirror, [1]=0 sensor mirror, [0]=0 no horizontal binning
		{0x3821, 0x00},

		//little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
		{0x4837, 24}, // 1/84M*2

		//Undocumented anti-green settings
		{0x3618, 0x00}, // Removes vertical lines appearing under bright light
		{0x3612, 0x59},
		{0x3708, 0x64},
		{0x3709, 0x52},
		{0x370c, 0x03},

		//[7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
		{0x4300, 0x00},
		//[2:0]=0x3 Format select ISP RAW (DPC)
		{0x501f, 0x03}
	};
	config_word_t const cfg_1080p_30fps_336M_mipi_[] =
		{//1920 x 1080 @ 30fps, RAW10, MIPISCLK=672, SCLK=67.2MHz, PCLK=134.4M
			//PLL1 configuration
			//[7:4]=0001 System clock divider /1, [3:0]=0001 Scale divider for MIPI /1
			{0x3035, 0x11}, // 30fps setting
			//[7:0]=84 PLL multiplier
			{0x3036, 0x54},
			//[4]=1 PLL root divider /2, [3:0]=5 PLL pre-divider /1.5
			{0x3037, 0x15},
		//[5:4]=00 PCLK root divider /1, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
			{0x3108, 0x01},

			//[6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
			{0x3034, 0x1A},

			//[3:0]=0 X address start high byte
			{0x3800, (336 >> 8) & 0x0F},
			//[7:0]=0 X address start low byte
			{0x3801, 336 & 0xFF},
			//[2:0]=0 Y address start high byte
			{0x3802, (426 >> 8) & 0x07},
			//[7:0]=0 Y address start low byte
			{0x3803, 426 & 0xFF},

			//[3:0] X address end high byte
			{0x3804, (2287 >> 8) & 0x0F},
			//[7:0] X address end low byte
			{0x3805, 2287 & 0xFF},
			//[2:0] Y address end high byte
			{0x3806, (1529 >> 8) & 0x07},
			//[7:0] Y address end low byte
			{0x3807, 1529 & 0xFF},

			//[3:0]=0 timing hoffset high byte
			{0x3810, (16 >> 8) & 0x0F},
			//[7:0]=0 timing hoffset low byte
			{0x3811, 16 & 0xFF},
			//[2:0]=0 timing voffset high byte
			{0x3812, (12 >> 8) & 0x07},
			//[7:0]=0 timing voffset low byte
			{0x3813, 12 & 0xFF},

			//[3:0] Output horizontal width high byte
			{0x3808, (1920 >> 8) & 0x0F},
			//[7:0] Output horizontal width low byte
			{0x3809, 1920 & 0xFF},
			//[2:0] Output vertical height high byte
			{0x380a, (1080 >> 8) & 0x7F},
			//[7:0] Output vertical height low byte
			{0x380b, 1080 & 0xFF},

			//HTS line exposure time in # of pixels Tline=HTS/sclk
			{0x380c, (2500 >> 8) & 0x1F},
			{0x380d, 2500 & 0xFF},
			//VTS frame exposure time in # lines
			{0x380e, (1120 >> 8) & 0xFF},
			{0x380f, 1120 & 0xFF},

			//[7:4]=0x1 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
			{0x3814, 0x11},
			//[7:4]=0x1 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
			{0x3815, 0x11},

			//[2]=0 ISP mirror, [1]=0 sensor mirror, [0]=0 no horizontal binning
			{0x3821, 0x00},

			//little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
			{0x4837, 14}, // 1/84M*2

			//Undocumented anti-green settings
			{0x3618, 0x00}, // Removes vertical lines appearing under bright light
			{0x3612, 0x59},
			{0x3708, 0x64},
			{0x3709, 0x52},
			{0x370c, 0x03},

			//[7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
			{0x4300, 0x00},
			//[2:0]=0x3 Format select ISP RAW (DPC)
			{0x501f, 0x03}
		};
config_word_t const cfg_1080p_30fps_336M_1lane_mipi_[] =
	{//1920 x 1080 @ 30fps, RAW10, MIPISCLK=672, SCLK=67.2MHz, PCLK=134.4M
		//PLL1 configuration
		//[7:4]=0001 System clock divider /1, [3:0]=0001 Scale divider for MIPI /1
		{0x3035, 0x11}, // 30fps setting
		//[7:0]=84 PLL multiplier
		{0x3036, 0x54},
		//[4]=1 PLL root divider /2, [3:0]=5 PLL pre-divider /1.5
		{0x3037, 0x15},
		//[5:4]=00 PCLK root divider /1, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
		{0x3108, 0x01},

		//[6:4]=001 PLL charge pump, [3:0]=1010 MIPI 10-bit mode
		{0x3034, 0x1A},

		//[7:5]=001 One lane mode, [4]=0 MIPI HS TX no power down, [3]=0 MIPI LP RX no power down, [2]=1 MIPI enable, [1:0]=10 Debug mode; Default=0x58
		{0x300e, 0x25},

		//[3:0]=0 X address start high byte
		{0x3800, (336 >> 8) & 0x0F},
		//[7:0]=0 X address start low byte
		{0x3801, 336 & 0xFF},
		//[2:0]=0 Y address start high byte
		{0x3802, (426 >> 8) & 0x07},
		//[7:0]=0 Y address start low byte
		{0x3803, 426 & 0xFF},

		//[3:0] X address end high byte
		{0x3804, (2287 >> 8) & 0x0F},
		//[7:0] X address end low byte
		{0x3805, 2287 & 0xFF},
		//[2:0] Y address end high byte
		{0x3806, (1529 >> 8) & 0x07},
		//[7:0] Y address end low byte
		{0x3807, 1529 & 0xFF},

		//[3:0]=0 timing hoffset high byte
		{0x3810, (16 >> 8) & 0x0F},
		//[7:0]=0 timing hoffset low byte
		{0x3811, 16 & 0xFF},
		//[2:0]=0 timing voffset high byte
		{0x3812, (12 >> 8) & 0x07},
		//[7:0]=0 timing voffset low byte
		{0x3813, 12 & 0xFF},

		//[3:0] Output horizontal width high byte
		{0x3808, (1920 >> 8) & 0x0F},
		//[7:0] Output horizontal width low byte
		{0x3809, 1920 & 0xFF},
		//[2:0] Output vertical height high byte
		{0x380a, (1080 >> 8) & 0x7F},
		//[7:0] Output vertical height low byte
		{0x380b, 1080 & 0xFF},

		//HTS line exposure time in # of pixels Tline=HTS/sclk
		{0x380c, (2500 >> 8) & 0x1F},
		{0x380d, 2500 & 0xFF},
		//VTS frame exposure time in # lines
		{0x380e, (1120 >> 8) & 0xFF},
		{0x380f, 1120 & 0xFF},

		//[7:4]=0x1 horizontal odd subsample increment, [3:0]=0x1 horizontal even subsample increment
		{0x3814, 0x11},
		//[7:4]=0x1 vertical odd subsample increment, [3:0]=0x1 vertical even subsample increment
		{0x3815, 0x11},

		//[2]=0 ISP mirror, [1]=0 sensor mirror, [0]=0 no horizontal binning
		{0x3821, 0x00},

		//little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
		{0x4837, 28}, // 1/84M*2

		//Undocumented anti-green settings
		{0x3618, 0x00}, // Removes vertical lines appearing under bright light
		{0x3612, 0x59},
		{0x3708, 0x64},
		{0x3709, 0x52},
		{0x370c, 0x03},

		//[7:4]=0x0 Formatter RAW, [3:0]=0x0 BGBG/GRGR
		{0x4300, 0x00},
		//[2:0]=0x3 Format select ISP RAW (DPC)
		{0x501f, 0x03}
	};
	config_word_t const cfg_init_[] =
	{
		//[7]=0 Software reset; [6]=1 Software power down; Default=0x02
		{0x3008, 0x42},
		//[1]=1 System input clock from PLL; Default read = 0x11
		{0x3103, 0x03},
		//[3:0]=0000 MD2P,MD2N,MCP,MCN input; Default=0x00
		{0x3017, 0x00},
		//[7:2]=000000 MD1P,MD1N, D3:0 input; Default=0x00
		{0x3018, 0x00},
		//[6:4]=001 PLL charge pump, [3:0]=1000 MIPI 8-bit mode
		{0x3034, 0x18},

		//              +----------------+        +------------------+         +---------------------+        +---------------------+
		//XVCLK         | PRE_DIV0       |        | Mult (4+252)     |         | Sys divider (0=16)  |        | MIPI divider (0=16) |
		//+-------+-----> 3037[3:0]=0001 +--------> 3036[7:0]=0x38   +---------> 3035[7:4]=0001      +--------> 3035[3:0]=0001      |
		//12MHz   |     | / 1            | 12MHz  | * 56             | 672MHz  | / 1                 | 672MHz | / 1                 |
		//        |     +----------------+        +------------------+         +----------+----------+        +----------+----------+
		//        |                                                                       |                              |
		//        |                                                                       |                      MIPISCLK|672MHz
		//        |                                                                       |                              |
		//        |     +----------------+        +------------------+         +----------v----------+        +----------v----------+
		//        |     | PRE_DIVSP      |        | R_DIV_SP         |         | PLL R divider       |        | MIPI PHY            | MIPI_CLK
		//        +-----> 303d[5:4]=01   +--------> 303d[2]=0 (+1)   |         | 3037[4]=1 (+1)      |        |                     +------->
		//              | / 1.5          |  8MHz  | / 1              |         | / 2                 |        | / 2                 | 336MHz
		//              +----------------+        +---------+--------+         +----------+----------+        +---------------------+
		//                                                  |                             |
		//                                                  |                             |
		//                                                  |                             |
		//              +----------------+        +---------v--------+         +----------v----------+        +---------------------+
		//              | SP divider     |        | Mult             |         | BIT div (MIPI 8/10) |        | SCLK divider        | SCLK
		//              | 303c[3:0]=0x1  +<-------+ 303b[4:0]=0x19   |         | 3034[3:0]=0x8)      +----+---> 3108[1:0]=01 (2^)   +------->
		//              | / 1            | 200MHz | * 25             |         | / 2                 |    |   | / 2                 | 84MHz
		//              +--------+-------+        +------------------+         +----------+----------+    |   +---------------------+
		//                       |                                                        |               |
		//                       |                                                        |               |
		//                       |                                                        |               |
		//              +--------v-------+                                     +----------v----------+    |   +---------------------+
		//              | R_SELD5 div    | ADCCLK                              | PCLK div            |    |   | SCLK2x divider      |
		//              | 303d[1:0]=001  +------->                             | 3108[5:4]=00 (2^)   |    +---> 3108[3:2]=00 (2^)   +------->
		//              | / 1            | 200MHz                              | / 1                 |        | / 1                 | 168MHz
		//              +----------------+                                     +----------+----------+        +---------------------+
		//                                                                                |
		//                                                                                |
		//                                                                                |
		//                                                                     +----------v----------+        +---------------------+
		//                                                                     | P divider (* #lanes)| PCLK   | Scale divider       |
		//                                                                     | 3035[3:0]=0001      +--------> 3824[4:0]           |
		//                                                                     | / 1                 | 168MHz | / 2                 |
		//                                                                     +---------------------+        +---------------------+

		//PLL1 configuration
		//[7:4]=0001 System clock divider /1, [3:0]=0001 Scale divider for MIPI /1
		{0x3035, 0x11},
		//[7:0]=56 PLL multiplier
		{0x3036, 0x38},
		//[4]=1 PLL root divider /2, [3:0]=1 PLL pre-divider /1
		{0x3037, 0x11},
		//[5:4]=00 PCLK root divider /1, [3:2]=00 SCLK2x root divider /1, [1:0]=01 SCLK root divider /2
		{0x3108, 0x01},
		//PLL2 configuration
		//[5:4]=01 PRE_DIV_SP /1.5, [2]=1 R_DIV_SP /1, [1:0]=00 DIV12_SP /1
		{0x303D, 0x10},
		//[4:0]=11001 PLL2 multiplier DIV_CNT5B = 25
		{0x303B, 0x19},

		{0x3630, 0x2e},
		{0x3631, 0x0e},
		{0x3632, 0xe2},
		{0x3633, 0x23},
		{0x3621, 0xe0},
		{0x3704, 0xa0},
		{0x3703, 0x5a},
		{0x3715, 0x78},
		{0x3717, 0x01},
		{0x370b, 0x60},
		{0x3705, 0x1a},
		{0x3905, 0x02},
		{0x3906, 0x10},
		{0x3901, 0x0a},
		{0x3731, 0x02},
		//VCM debug mode
		{0x3600, 0x37},
		{0x3601, 0x33},
		//System control register changing not recommended
		{0x302d, 0x60},
		//??
		{0x3620, 0x52},
		{0x371b, 0x20},
		//?? DVP
		{0x471c, 0x50},

		{0x3a13, 0x43},
		{0x3a18, 0x00},
		{0x3a19, 0xf8},
		{0x3635, 0x13},
		{0x3636, 0x06},
		{0x3634, 0x44},
		{0x3622, 0x01},
		{0x3c01, 0x34},
		{0x3c04, 0x28},
		{0x3c05, 0x98},
		{0x3c06, 0x00},
		{0x3c07, 0x08},
		{0x3c08, 0x00},
		{0x3c09, 0x1c},
		{0x3c0a, 0x9c},
		{0x3c0b, 0x40},

		//[7]=1 color bar enable, [3:2]=00 eight color bar
		{0x503d, 0x00},
		//[2]=1 ISP vflip, [1]=1 sensor vflip
		{0x3820, 0x46},

		//[7:5]=010 Two lane mode, [4]=0 MIPI HS TX no power down, [3]=0 MIPI LP RX no power down, [2]=1 MIPI enable, [1:0]=10 Debug mode; Default=0x58
		{0x300e, 0x45},
		//[5]=0 Clock free running, [4]=1 Send line short packet, [3]=0 Use lane1 as default, [2]=1 MIPI bus LP11 when no packet; Default=0x04
		{0x4800, 0x14},
		{0x302e, 0x08},
		//[7:4]=0x3 YUV422, [3:0]=0x0 YUYV
		//{0x4300, 0x30},
		//[7:4]=0x6 RGB565, [3:0]=0x0 {b[4:0],g[5:3],g[2:0],r[4:0]}
		{0x4300, 0x6f},
		{0x501f, 0x01},

		{0x4713, 0x03},
		{0x4407, 0x04},
		{0x440e, 0x00},
		{0x460b, 0x35},
		//[1]=0 DVP PCLK divider manual control by 0x3824[4:0]
		{0x460c, 0x20},
		//[4:0]=1 SCALE_DIV=INT(3824[4:0]/2)
		{0x3824, 0x01},

		//MIPI timing
		//		{0x4805, 0x10}, //LPX global timing select=auto
		//		{0x4818, 0x00}, //hs_prepare + hs_zero_min ns
		//		{0x4819, 0x96},
		//		{0x482A, 0x00}, //hs_prepare + hs_zero_min UI
		//
		//		{0x4824, 0x00}, //lpx_p_min ns
		//		{0x4825, 0x32},
		//		{0x4830, 0x00}, //lpx_p_min UI
		//
		//		{0x4826, 0x00}, //hs_prepare_min ns
		//		{0x4827, 0x32},
		//		{0x4831, 0x00}, //hs_prepare_min UI

		//[7]=1 LENC correction enabled, [5]=1 RAW gamma enabled, [2]=1 Black pixel cancellation enabled, [1]=1 White pixel cancellation enabled, [0]=1 Color interpolation enabled
		{0x5000, 0x07},
		//[7]=0 Special digital effects, [5]=0 scaling, [2]=0 UV average disabled, [1]=1 Color matrix enabled, [0]=1 Auto white balance enabled
		{0x5001, 0x03}
	};
	config_modes_t const modes[] =
	{
			{ MAP_ENUM_TO_CFG(MODE_480P_640_480_15FPS, cfg_480p_15fps_) },
			{ MAP_ENUM_TO_CFG(MODE_480P_640_480_30FPS, cfg_480p_30fps_) },
			{ MAP_ENUM_TO_CFG(MODE_480P_640_480_60FPS, cfg_480p_60fps_) },
			{ MAP_ENUM_TO_CFG(MODE_720P_1280_720_60fps, cfg_720p_60fps_) },
			{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_15fps, cfg_1080p_15fps_) },
			{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps, cfg_1080p_30fps_), },
			{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps_336M_MIPI, cfg_1080p_30fps_336M_mipi_) },
			{ MAP_ENUM_TO_CFG(MODE_1080P_1920_1080_30fps_336M_1LANE_MIPI, cfg_1080p_30fps_336M_1lane_mipi_) },
	};
	config_awb_t const awbs[] =
	{
			{ MAP_ENUM_TO_CFG(AWB_DISABLED, cfg_disable_awb_) },
			{ MAP_ENUM_TO_CFG(AWB_SIMPLE, cfg_simple_awb_) },
			{ MAP_ENUM_TO_CFG(AWB_ADVANCED, cfg_advanced_awb_) }
	};
}

class OV5640 {
public:
	class HardwareError;

	OV5640(I2C_Client& iic, GPIO_Client& gpio) :
		iic_(iic), gpio_(gpio)
	{
		reset();
		init();
	}

	void init()
	{
		uint8_t id_h, id_l;
		readReg(reg_ID_h, id_h);
		readReg(reg_ID_l, id_l);
		if (id_h != dev_ID_h_ || id_l != dev_ID_l_)
		{
			/* Does not work. https://www.xilinx.com/support/answers/64193.html
	      std::stringstream ss;
	      ss << "Got " << std::hex << id_h << id_l << ". Expected " << dev_ID_h_ << dev_ID_l_;
			 */
			char msg[100];
			snprintf(msg, sizeof(msg), "Got %02x %02x. Expected %02x %02x\r\n", id_h, id_l, dev_ID_h_, dev_ID_l_);
			throw HardwareError(HardwareError::WRONG_ID, msg);
		}
		//[1]=0 System input clock from pad; Default read = 0x11
		writeReg(0x3103, 0x11);
		//[7]=1 Software reset; [6]=0 Software power down; Default=0x02
		writeReg(0x3008, 0x82);

		usleep(1000000);

		size_t i;
		for (i=0;i<sizeof(OV5640_cfg::cfg_init_)/sizeof(OV5640_cfg::cfg_init_[0]); ++i)
		{
			writeReg(OV5640_cfg::cfg_init_[i].addr, OV5640_cfg::cfg_init_[i].data);
		}

		//Stay in power down
	}

	Errc reset()
	{
		//Power cycle
		gpio_.clearBit(gpio_.Bits::CAM_GPIO0);
		usleep(1000000);
		gpio_.setBit(gpio_.Bits::CAM_GPIO0);
		usleep(1000000);

		return OK;
	}

	Errc set_mode(OV5640_cfg::mode_t mode)
	{
		if (mode >= OV5640_cfg::mode_t::MODE_END)
			return ERR_LOGICAL;

		//[7]=0 Software reset; [6]=1 Software power down; Default=0x02
		writeReg(0x3008, 0x42);

		auto cfg_mode = &OV5640_cfg::modes[mode];
		writeConfig(cfg_mode->cfg, cfg_mode->cfg_size);

		//[7]=0 Software reset; [6]=0 Software power down; Default=0x02
		writeReg(0x3008, 0x02);
		return OK;
	}

	Errc set_awb(OV5640_cfg::awb_t awb)
	{
		if (awb >= OV5640_cfg::awb_t::AWB_END)
			return ERR_LOGICAL;
		//[7]=0 Software reset; [6]=1 Software power down; Default=0x02
		writeReg(0x3008, 0x42);

		auto cfg_mode = &OV5640_cfg::awbs[awb];
		writeConfig(cfg_mode->cfg, cfg_mode->cfg_size);

		//[7]=0 Software reset; [6]=0 Software power down; Default=0x02
		writeReg(0x3008, 0x02);
		return OK;
	}

	Errc set_isp_format(OV5640_cfg::isp_format_t isp)
	{
		if (isp >= OV5640_cfg::isp_format_t::ISP_END)
			return ERR_LOGICAL;
		//[7]=0 Software reset; [6]=1 Software power down; Default=0x02
		writeReg(0x3008, 0x42);

		switch (isp)
		{
			case OV5640_cfg::isp_format_t::ISP_RGB:
				writeReg(OV5640_cfg::OV5640_FORMAT_MUX_CONTROL, 0x01);
				break;
			case OV5640_cfg::isp_format_t::ISP_RAW:
				writeReg(OV5640_cfg::OV5640_FORMAT_MUX_CONTROL, 0x03);
				break;
			default:
				break;
		}

		//[7]=0 Software reset; [6]=0 Software power down; Default=0x02
		writeReg(0x3008, 0x02);
		return OK;
	}

	~OV5640() { }
	void set_test(OV5640_cfg::test_t test)
	{
		switch(test)
		{
			case OV5640_cfg::test_t::TEST_DISABLED:
				writeReg(OV5640_cfg::OV5640_REG_PRE_ISP_TEST_SET1, 0x00);
				break;
			case OV5640_cfg::test_t::TEST_EIGHT_COLOR_BAR:
				writeReg(OV5640_cfg::OV5640_REG_PRE_ISP_TEST_SET1, 0x80);
				break;
			default:
				break;
		}
	}
	void readReg(uint16_t reg_addr, uint8_t& buf)
	{
		for(auto retry_count = retry_count_; retry_count > 0; --retry_count)
		{
			try
			{
				auto buf_addr = std::vector<uint8_t>{(uint8_t)(reg_addr>>8), (uint8_t)reg_addr};
				iic_.write(dev_address_, buf_addr.data(), buf_addr.size());
				iic_.read(dev_address_, &buf, 1);
				break; //If no exceptions, no mo retries
			}
			catch (I2C_Client::TransmitError const& e)
			{
				if (retry_count > 0)
				{
					continue;
				}
				else
				{
					throw HardwareError(HardwareError::IIC_NACK, e.what());
				}
			}
		}
	}
	void writeReg(uint16_t reg_addr, uint8_t const reg_data)
	{
		for(auto retry_count = retry_count_; retry_count > 0; --retry_count)
		{
			try
			{
				auto buf = std::vector<uint8_t>{(uint8_t)(reg_addr>>8), (uint8_t)reg_addr, reg_data};
				iic_.write(dev_address_, buf.data(), buf.size());
				break; //If no exceptions, no mo retries
			}
			catch (I2C_Client::TransmitError const& e)
			{
				if (retry_count > 0) continue;
				else throw HardwareError(HardwareError::IIC_NACK, e.what());
			}
		}
	}

	void writeRegLiquid(uint8_t const reg_data)
		{
			for(auto retry_count = retry_count_; retry_count > 0; --retry_count)
			{
				try
				{
					auto buf = std::vector<uint8_t>{reg_data};
					iic_.write(dev_address2_, buf.data(), buf.size());
					break; //If no exceptions, no mo retries
				}
				catch (I2C_Client::TransmitError const& e)
				{
					if (retry_count > 0) continue;
					else throw HardwareError(HardwareError::IIC_NACK, e.what());
				}
			}
		}
	class HardwareError : public std::runtime_error
	{
	public:
		using Errc = enum {WRONG_ID = 1, IIC_NACK};
		HardwareError(Errc errc, char const* msg) : std::runtime_error(msg), errc_(errc) {}
		Errc errc() const { return errc_; }
	private:
		Errc errc_;
	};
private:
	void usleep(uint32_t time)
	{//TODO couldn't think of anything better
		for (uint32_t i=0; i<time; i++) ;
	}
	void writeConfig(OV5640_cfg::config_word_t const* cfg, size_t cfg_size)
	{
		for (size_t i=0; i<cfg_size; ++i)
		{
			writeReg(cfg[i].addr, cfg[i].data);
		}
	}
private:
	I2C_Client& iic_;
	GPIO_Client& gpio_;
	uint8_t dev_address_ = (0x78 >> 1);
	uint8_t dev_address2_ = (0x46 >> 1);
	uint8_t const dev_ID_h_ = 0x56;
	uint8_t const dev_ID_l_ = 0x40;
	uint16_t const reg_ID_h = 0x300A;
	uint16_t const reg_ID_l = 0x300B;
	unsigned int const retry_count_ = 10;
};

} /* namespace digilent */

#endif /* OV5640_H_ */
