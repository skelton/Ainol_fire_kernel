/*
 * VDIN driver
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <mach/am_regs.h>
#include <linux/amports/canvas.h>
#include <linux/tvin/tvin.h>

#include "tvin_global.h"
#include "vdin_ctl.h"
#include "tvin_format_table.h"
#include "vdin_regs.h"
#include "vdin.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"

/* black bar det enable/disable test */
static int black_bar_enable = 0;
module_param(black_bar_enable, bool, 0664);
MODULE_PARM_DESC(black_bar_enable, "black bar enable/disable");

/***************************Local defines**********************************/
#define BBAR_BLOCK_THR_FACTOR           3
#define BBAR_LINE_THR_FACTOR            7

#define VDIN_MUX_NULL                   0
#define VDIN_MUX_MPEG                   1
#define VDIN_MUX_656                    2
#define VDIN_MUX_TVFE                   3
#define VDIN_MUX_CVD2                   4
#define VDIN_MUX_HDMI                   5
#define VDIN_MUX_DVIN                   6

#define VDIN_MAP_Y_G                    0
#define VDIN_MAP_BPB                    1
#define VDIN_MAP_RCR                    2

#define MEAS_MUX_NULL                   0
#define MEAS_MUX_656                    1
#define MEAS_MUX_TVFE                   2
#define MEAS_MUX_CVD2                   3
#define MEAS_MUX_HDMI                   4
#define MEAS_MUX_DVIN                   5

#define VDIN_WAIT_VALID_VS      30  // check hcnt/vcnt after N*vs.
#define VDIN_IGNORE_VS_CNT      20  // ignore n*vs which have wrong data.
#define VDIN_MEAS_HSCNT_DIFF    0x50  // the diff value between normal/bad data
#define VDIN_MEAS_VSCNT_DIFF    0x50  // the diff value between normal/bad data

/***************************Local Structures**********************************/
static struct vdin_matrix_lup_s vdin_matrix_lup[31] =
{
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                         0x00000000, 0x0400200, 0x00000200,},
    // VDIN_MATRIX_RGB_YUV601
    //    0     0.257  0.504  0.098     16
    //    0    -0.148 -0.291  0.439    128
    //    0     0.439 -0.368 -0.071    128
    {0x00000000, 0x00000000, 0x01070204, 0x00641f68, 0x1ed601c2, 0x01c21e87,
                                        0x00001fb7, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV601_RGB
    //  -16     1.164  0      1.596      0
    // -128     1.164 -0.391 -0.813      0
    // -128     1.164  2.018  0          0
    {0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
                                        0x00000000, 0x00000000, 0x00000000,},
    // VDIN_MATRIX_RGB_YUV601F
    //    0     0.299  0.587  0.114      0
    //    0    -0.169 -0.331  0.5      128
    //    0     0.5   -0.419 -0.081    128
    {0x00000000, 0x00000000, 0x01320259, 0x00751f53, 0x1ead0200, 0x02001e53,
                                        0x00001fad, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV601F_RGB
    //    0     1      0      1.402      0
    // -128     1     -0.344 -0.714      0
    // -128     1      1.772  0          0
    {0x00000600, 0x00000600, 0x04000000, 0x059c0400, 0x1ea01d25, 0x04000717,
                                        0x00000000, 0x00000000, 0x00000000,},
    // VDIN_MATRIX_RGBS_YUV601
    //  -16     0.299  0.587  0.114     16
    //  -16    -0.173 -0.339  0.511    128
    //  -16     0.511 -0.429 -0.083    128
    {0x07c007c0, 0x000007c0, 0x01320259, 0x00751f4f, 0x1ea5020b, 0x020b1e49,
                                        0x00001fab, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV601_RGBS
    //  -16     1      0      1.371     16
    // -128     1     -0.336 -0.698     16
    // -128     1      1.733  0         16
    {0x07c00600, 0x00000600, 0x04000000, 0x057c0400, 0x1ea81d35, 0x040006ef,
                                        0x00000000, 0x00400040, 0x00000040,},
    // VDIN_MATRIX_RGBS_YUV601F
    //  -16     0.348  0.683  0.133      0
    //  -16    -0.197 -0.385  0.582    128
    //  -16     0.582 -0.488 -0.094    128
    {0x07c007c0, 0x000007c0, 0x016402bb, 0x00881f36, 0x1e760254, 0x02541e0c,
                                        0x00001fa0, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV601F_RGBS
    //    0     0.859  0      1.204     16
    // -128     0.859 -0.295 -0.613     16
    // -128     0.859  1.522  0         16
    {0x00000600, 0x00000600, 0x03700000, 0x04d10370, 0x1ed21d8c, 0x03700617,
                                        0x00000000, 0x00400040, 0x00000040,},
    // VDIN_MATRIX_YUV601F_YUV601
    //    0     0.859  0      0         16
    // -128     0      0.878  0        128
    // -128     0      0      0.878    128
    {0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
                                        0x00000383, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV601_YUV601F
    //  -16     1.164  0      0          0
    // -128     0      1.138  0        128
    // -128     0      0      1.138    128
    {0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
                                        0x0000048d, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_RGB_YUV709
    //    0     0.183  0.614  0.062     16
    //    0    -0.101 -0.338  0.439    128
    //    0     0.439 -0.399 -0.04     128
    {0x00000000, 0x00000000, 0x00bb0275, 0x003f1f99, 0x1ea601c2, 0x01c21e67,
                                        0x00001fd7, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709_RGB
    //  -16     1.164  0      1.793      0
    // -128     1.164 -0.213 -0.534      0
    // -128     1.164  2.115  0          0
    {0x07c00600, 0x00000600, 0x04a80000, 0x072c04a8, 0x1f261ddd, 0x04a80876,
                                        0x00000000, 0x00000000, 0x00000000,},
    // VDIN_MATRIX_RGB_YUV709F
    //    0     0.213  0.715  0.072      0
    //    0    -0.115 -0.385  0.5      128
    //    0     0.5   -0.454 -0.046    128
    {0x00000000, 0x00000000, 0x00da02dc, 0x004a1f8a, 0x1e760200, 0x02001e2f,
                                        0x00001fd1, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV709F_RGB
    //    0     1      0      1.575      0
    // -128     1     -0.187 -0.468      0
    // -128     1      1.856  0          0
    {0x00000600, 0x00000600, 0x04000000, 0x064d0400, 0x1f411e21, 0x0400076d,
                                        0x00000000, 0x00000000, 0x00000000,},
    // VDIN_MATRIX_RGBS_YUV709
    //  -16     0.213  0.715  0.072     16
    //  -16    -0.118 -0.394  0.511    128
    //  -16     0.511 -0.464 -0.047    128
    {0x07c007c0, 0x000007c0, 0x00da02dc, 0x004a1f87, 0x1e6d020b, 0x020b1e25,
                                        0x00001fd0, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709_RGBS
    //  -16     1      0      1.54      16
    // -128     1     -0.183 -0.459     16
    // -128     1      1.816  0         16
    {0x07c00600, 0x00000600, 0x04000000, 0x06290400, 0x1f451e2a, 0x04000744,
                                        0x00000000, 0x00400040, 0x00000040,},
    // VDIN_MATRIX_RGBS_YUV709F
    //  -16     0.248  0.833  0.084      0
    //  -16    -0.134 -0.448  0.582    128
    //  -16     0.582 -0.529 -0.054    128
    {0x07c007c0, 0x000007c0, 0x00fe0355, 0x00561f77, 0x1e350254, 0x02541de2,
                                        0x00001fc9, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV709F_RGBS
    //    0     0.859  0      1.353     16
    // -128     0.859 -0.161 -0.402     16
    // -128     0.859  1.594  0         16
    {0x00000600, 0x00000600, 0x03700000, 0x05690370, 0x1f5b1e64, 0x03700660,
                                        0x00000000, 0x00400040, 0x00000040,},
    // VDIN_MATRIX_YUV709F_YUV709
    //    0     0.859  0      0         16
    // -128     0      0.878  0        128
    // -128     0      0      0.878    128
    {0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
                                        0x00000383, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709_YUV709F
    //  -16     1.164  0      0          0
    // -128     0      1.138  0        128
    // -128     0      0      1.138    128
    {0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
                                        0x0000048d, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV601_YUV709
    //  -16     1     -0.115 -0.207     16
    // -128     0      1.018  0.114    128
    // -128     0      0.075  1.025    128
    {0x07c00600, 0x00000600, 0x04001f8a, 0x1f2c0000, 0x04120075, 0x0000004d,
                                        0x0000041a, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709_YUV601
    //  -16     1      0.100  0.192     16
    // -128     0      0.990 -0.110    128
    // -128     0     -0.072  0.984    128
    {0x07c00600, 0x00000600, 0x04000066, 0x00c50000, 0x03f61f8f, 0x00001fb6,
                                        0x000003f0, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV601_YUV709F
    //  -16     1.164 -0.134 -0.241      0
    // -128     0      1.160  0.129    128
    // -128     0      0.085  1.167    128
    {0x07c00600, 0x00000600, 0x04a81f77, 0x1f090000, 0x04a40084, 0x00000057,
                                        0x000004ab, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV709F_YUV601
    //    0     0.859  0.088  0.169     16
    // -128     0      0.869 -0.097    128
    // -128     0     -0.063  0.864    128
    {0x00000600, 0x00000600, 0x0370005a, 0x00ad0000, 0x037a1f9d, 0x00001fbf,
                                        0x00000375, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV601F_YUV709
    //    0     0.859 -0.101 -0.182     16
    // -128     0      0.894  0.100    128
    // -128     0      0.066  0.900    128
    {0x00000600, 0x00000600, 0x03701f99, 0x1f460000, 0x03930066, 0x00000044,
                                        0x0000039a, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709_YUV601F
    //  -16     1.164  0.116  0.223      0
    // -128     0      1.128 -0.126    128
    // -128     0     -0.082  1.120    128
    {0x07c00600, 0x00000600, 0x04a80077, 0x00e40000, 0x04831f7f, 0x00001fac,
                                        0x0000047b, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_YUV601F_YUV709F
    //    0     1     -0.118 -0.212     16
    // -128     0      1.018  0.114    128
    // -128     0      0.075  1.025    128
    {0x00000600, 0x00000600, 0x04001f87, 0x1f270000, 0x04120075, 0x0000004d,
                                        0x0000041a, 0x00400200, 0x00000200,},
    // VDIN_MATRIX_YUV709F_YUV601F
    //    0     1      0.102  0.196      0
    // -128     0      0.990 -0.111    128
    // -128     0     -0.072  0.984    128
    {0x00000600, 0x00000600, 0x04000068, 0x00c90000, 0x03f61f8e, 0x00001fb6,
                                        0x000003f0, 0x00000200, 0x00000200,},
    // VDIN_MATRIX_RGBS_RGB
    //  -16     1.164  0      0          0
    //  -16     0      1.164  0          0
    //  -16     0      0      1.164      0
    {0x07c007c0, 0x000007c0, 0x04a80000, 0x00000000, 0x04a80000, 0x00000000,
                                        0x000004a8, 0x00000000, 0x00000000,},
    // VDIN_MATRIX_RGB_RGBS
    //    0     0.859  0      0         16
    //    0     0      0.859  0         16
    //    0     0      0      0.859     16
    {0x00000000, 0x00000000, 0x03700000, 0x00000000, 0x03700000, 0x00000000,
                                        0x00000370, 0x00400040, 0x00000040,},
};

/***************************Local function**********************************/

inline void vdin_get_format_convert(struct vdin_dev_s *devp)
{
    if (devp->prop.color_format == TVIN_RGB444)
    {
        if ((devp->parm.port >= TVIN_PORT_VGA0) &&
            (devp->parm.port <= TVIN_PORT_VGA7))
            devp->format_convert = VDIN_FORMAT_CONVERT_RGB_YUV444;
        else
            devp->format_convert = VDIN_FORMAT_CONVERT_RGB_YUV422;
    }
    else
        devp->format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;
}

#if defined(CONFIG_ARCH_MESON2)
void vdin_set_meas_mux(unsigned int offset, enum tvin_port_e port_)
{
//    unsigned int offset = devp->addr_offset;
    unsigned int meas_mux = MEAS_MUX_NULL;

    switch ((port_)>>8)
    {
        case 0x01: // mpeg
            meas_mux = MEAS_MUX_NULL;
            break;
        case 0x02: // 656
            meas_mux = MEAS_MUX_656;
            break;
        case 0x04: // VGA
            meas_mux = MEAS_MUX_TVFE;
            break;
        case 0x08: // COMPONENT
            meas_mux = MEAS_MUX_TVFE;
            break;
        case 0x10: // CVBS
            meas_mux = MEAS_MUX_CVD2;
            break;
        case 0x20: // SVIDEO
            meas_mux = MEAS_MUX_CVD2;
            break;
        case 0x40: // hdmi
            meas_mux = MEAS_MUX_HDMI;
            break;
        case 0x80: // dvin
            meas_mux = MEAS_MUX_DVIN;
            break;
        default:
            meas_mux = MEAS_MUX_NULL;
            break;
    }
    // mux
    WRITE_CBUS_REG_BITS((VDIN_MEAS_CTRL0 + offset),
        meas_mux, MEAS_HS_VS_SEL_BIT, MEAS_HS_VS_SEL_WID);
    // manual reset, rst = 1 & 0
    WRITE_CBUS_REG_BITS((VDIN_MEAS_CTRL0 + offset),
        1, MEAS_RST_BIT, MEAS_RST_WID);
    WRITE_CBUS_REG_BITS((VDIN_MEAS_CTRL0 + offset),
        0, MEAS_RST_BIT, MEAS_RST_WID);
}
#endif

static inline void vdin_set_top(unsigned int offset, enum tvin_port_e port, unsigned int h)
{
//    unsigned int offset = devp->addr_offset;
    unsigned int vdin_mux = VDIN_MUX_NULL;
    unsigned int vdin_data_bus_0 = VDIN_MAP_Y_G;
    unsigned int vdin_data_bus_1 = VDIN_MAP_BPB;
    unsigned int vdin_data_bus_2 = VDIN_MAP_RCR;

    // [28:16]         top.input_width_m1   = h-1
    // [12: 0]         top.output_width_m1  = h-1
    WRITE_CBUS_REG((VDIN_WIDTHM1I_WIDTHM1O    + offset), ((h-1)<<16)|(h-1));
    switch ((port)>>8)
    {
        case 0x01: // mpeg
            vdin_mux = VDIN_MUX_MPEG;
            break;
        case 0x02: // 656
            vdin_mux = VDIN_MUX_656;
            break;
        case 0x04: // VGA
            vdin_mux = VDIN_MUX_TVFE;
            // In the order of RGB for further RGB->YUV601 or RGB->YUV709 convertion
            vdin_data_bus_0 = VDIN_MAP_RCR;
            vdin_data_bus_1 = VDIN_MAP_Y_G;
            vdin_data_bus_2 = VDIN_MAP_BPB;
            break;
        case 0x08: // COMPONENT
            vdin_mux = VDIN_MUX_TVFE;
            break;
        case 0x10: // CVBS
            vdin_mux = VDIN_MUX_CVD2;
            break;
        case 0x20: // SVIDEO
            vdin_mux = VDIN_MUX_CVD2;
            break;
        case 0x40: // hdmi
            vdin_mux = VDIN_MUX_HDMI;
            // In the order of RGB for further RGB->YUV601 or RGB->YUV709 convertion
            vdin_data_bus_0 = VDIN_MAP_RCR;
            vdin_data_bus_1 = VDIN_MAP_Y_G;
            vdin_data_bus_2 = VDIN_MAP_BPB;
            break;
        case 0x80: // dvin
            vdin_mux = VDIN_MUX_DVIN;
            break;
        default:
            vdin_mux = VDIN_MUX_NULL;
            break;
    }
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset),
        vdin_mux, VDIN_SEL_BIT, VDIN_SEL_WID);
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset),
        vdin_data_bus_0, COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset),
        vdin_data_bus_1, COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset),
        vdin_data_bus_2, COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
}

#if defined(CONFIG_ARCH_MESON2)

#define TVIN_MAX_PIXCLK 20000


/*
    this fucntion will set the bellow parameters of devp:
        1.h_active
        2.v_active
 */
void vdin_set_decimation(struct vdin_dev_s *devp)
{
    unsigned int offset = devp->addr_offset;
    unsigned int decimation_ratio = 0;
    unsigned short pix_clk = tvin_fmt_tbl[devp->parm.info.fmt].pixel_clk;
    unsigned short new_clk = 0;

    if ((devp->prop.pixel_repeat >=  2) &&
        (devp->prop.pixel_repeat <= 16))
        decimation_ratio = devp->prop.pixel_repeat - 1;
    new_clk = pix_clk / (decimation_ratio + 1);

    while (new_clk > TVIN_MAX_PIXCLK)
    {
        decimation_ratio++;
        new_clk = pix_clk / (decimation_ratio + 1);
    }
    pr_info("%s decimation_ratio = %d\n",__func__, decimation_ratio);

    if (decimation_ratio)
    {
        // ratio
        WRITE_CBUS_REG_BITS((VDIN_ASFIFO_CTRL2 + offset),
            decimation_ratio, ASFIFO_DECIMATION_NUM_BIT, ASFIFO_DECIMATION_NUM_WID);
    	// en
        WRITE_CBUS_REG_BITS((VDIN_ASFIFO_CTRL2 + offset),
            1, ASFIFO_DECIMATION_DE_EN_BIT, ASFIFO_DECIMATION_DE_EN_WID);
        // manual reset, rst = 1 & 0
        WRITE_CBUS_REG_BITS((VDIN_ASFIFO_CTRL2 + offset),
            1, ASFIFO_DECIMATION_SYNC_WITH_DE_BIT, ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
        WRITE_CBUS_REG_BITS((VDIN_ASFIFO_CTRL2 + offset),
            0, ASFIFO_DECIMATION_SYNC_WITH_DE_BIT, ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
    }
    devp->h_active = tvin_fmt_tbl[devp->parm.info.fmt].h_active / (decimation_ratio + 1);
    devp->v_active = tvin_fmt_tbl[devp->parm.info.fmt].v_active;
    // output_width_m1
    WRITE_CBUS_REG_BITS((VDIN_INTF_WIDTHM1 + offset),
        (devp->h_active - 1), VDIN_INTF_WIDTHM1_BIT, VDIN_INTF_WIDTHM1_WID);
    return ;
}

/*
    this fucntion will set the bellow parameters of devp:
        1.h_active
        2.v_active
 */
void vdin_set_cutwin(struct vdin_dev_s *devp)
{
    unsigned int offset = devp->addr_offset;

    if ((devp->parm.cutwin.hs <= devp->parm.cutwin.he) &&
        (devp->parm.cutwin.he <= devp->h_active - 1)   &&
        (devp->parm.cutwin.he)                         &&
        (devp->parm.cutwin.vs <= devp->parm.cutwin.ve) &&
        (devp->parm.cutwin.ve <= devp->v_active - 1)   &&
        (devp->parm.cutwin.ve))
    {
        devp->h_active = devp->parm.cutwin.he - devp->parm.cutwin.hs + 1;
        devp->v_active = devp->parm.cutwin.ve - devp->parm.cutwin.vs + 1;

    #ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
        /* set new video size, change size only if manual fmt is wrong */
        if ((devp->parm.port >= TVIN_PORT_CVBS0) &&
            (devp->parm.port <= TVIN_PORT_SVIDEO7))
        {
            if ((devp->cvbs_pos_chg == TVIN_CVBS_POS_P_TO_N) ||
                (devp->cvbs_pos_chg == TVIN_CVBS_POS_N_TO_P))
            {
                if (devp->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)  //288-240
                {
                    devp->v_active -= 48;
                    devp->parm.cutwin.ve -= 48;
                }
                else
                {
                    devp->v_active += 48;
                    devp->parm.cutwin.ve += 48;
                }
            }
        }
    #endif
        WRITE_CBUS_REG((VDIN_WIN_H_START_END + offset), (devp->parm.cutwin.hs << INPUT_WIN_H_START_BIT) | (devp->parm.cutwin.he << INPUT_WIN_H_END_BIT));
        WRITE_CBUS_REG((VDIN_WIN_V_START_END + offset), (devp->parm.cutwin.vs << INPUT_WIN_V_START_BIT) | (devp->parm.cutwin.ve << INPUT_WIN_V_END_BIT));
        WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0  + offset), 1, INPUT_WIN_SEL_EN_BIT, INPUT_WIN_SEL_EN_WID);
        pr_info("%s cutwin.hs = %d, cutwin.vs = %d\n", __func__,
                devp->parm.cutwin.hs, devp->parm.cutwin.vs);
    }
}

#endif

static inline void vdin_set_color_matrix(unsigned int offset, enum tvin_sig_fmt_e fmt, enum vdin_format_convert_e format_convert)
{
//    unsigned int offset = devp->addr_offset;
    unsigned int v = tvin_fmt_tbl[fmt].v_active;
    enum tvin_scan_mode_e mode = tvin_fmt_tbl[fmt].scan_mode;
    enum vdin_matrix_csc_e    matrix_csc = VDIN_MATRIX_NULL;
    struct vdin_matrix_lup_s *matrix_tbl;

    switch (format_convert)
    {
        case VDIN_MATRIX_XXX_YUV_BLACK:
            matrix_csc = VDIN_MATRIX_XXX_YUV601_BLACK;
            break;
        case VDIN_FORMAT_CONVERT_RGB_YUV422:
            matrix_csc = VDIN_MATRIX_RGBS_YUV601;
            break;
        case VDIN_FORMAT_CONVERT_RGB_YUV444:
            matrix_csc = VDIN_MATRIX_RGB_YUV601;
            break;
        case VDIN_FORMAT_CONVERT_YUV_RGB:
            if (
                ((mode == TVIN_SCAN_MODE_PROGRESSIVE) && (v >= 720)) || //  720p & above
                ((mode == TVIN_SCAN_MODE_INTERLACED)  && (v >= 540))    // 1080i & above
               )
                matrix_csc = VDIN_MATRIX_YUV709_RGB;
            else
                matrix_csc = VDIN_MATRIX_YUV601_RGB;
            break;
        case VDIN_FORMAT_CONVERT_YUV_YUV422:
        case VDIN_FORMAT_CONVERT_YUV_YUV444:
            if (
                ((mode == TVIN_SCAN_MODE_PROGRESSIVE) && (v >= 720)) || //  720p & above
                ((mode == TVIN_SCAN_MODE_INTERLACED)  && (v >= 540))    // 1080i & above
               )
                matrix_csc = VDIN_MATRIX_YUV709_YUV601;
            break;
        default:
            matrix_csc = VDIN_MATRIX_NULL;
            break;
    }

    if (matrix_csc == VDIN_MATRIX_NULL)
    {
        WRITE_CBUS_REG_BITS((VDIN_MATRIX_CTRL + offset),
            0, VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
    }
    else
    {
        matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];
        WRITE_CBUS_REG((VDIN_MATRIX_PRE_OFFSET0_1 + offset),matrix_tbl->pre_offset0_1);
        WRITE_CBUS_REG((VDIN_MATRIX_PRE_OFFSET2 + offset), matrix_tbl->pre_offset2);
        WRITE_CBUS_REG((VDIN_MATRIX_COEF00_01 + offset), matrix_tbl->coef00_01);
        WRITE_CBUS_REG((VDIN_MATRIX_COEF02_10 + offset), matrix_tbl->coef02_10);
        WRITE_CBUS_REG((VDIN_MATRIX_COEF11_12 + offset), matrix_tbl->coef11_12);
        WRITE_CBUS_REG((VDIN_MATRIX_COEF20_21 + offset), matrix_tbl->coef20_21);
        WRITE_CBUS_REG((VDIN_MATRIX_COEF22 + offset), matrix_tbl->coef22);
        WRITE_CBUS_REG((VDIN_MATRIX_OFFSET0_1 + offset), matrix_tbl->post_offset0_1);
        WRITE_CBUS_REG((VDIN_MATRIX_OFFSET2 + offset), matrix_tbl->post_offset2);
        WRITE_CBUS_REG_BITS((VDIN_MATRIX_CTRL + offset),
            1, VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
    }
}
void vdin_set_matrix(struct vdin_dev_s *devp)
{
    vdin_set_color_matrix(devp->addr_offset, devp->parm.info.fmt, devp->format_convert);
}
void vdin_set_matrix_blank(struct vdin_dev_s *devp)
{
    vdin_set_color_matrix(devp->addr_offset, devp->parm.info.fmt, VDIN_MATRIX_XXX_YUV_BLACK);
}
static inline void vdin_set_bbar(unsigned int offset, unsigned int v, unsigned int h)
{
    unsigned int region_width = 1, block_thr = 0, line_thr = 0;
    while ((region_width<<1) < h)
    {
        region_width <<= 1;
    }

    block_thr = (region_width>>1) * v;
    block_thr = block_thr - (block_thr >> BBAR_BLOCK_THR_FACTOR); // bblk=(bpix>thr)
    line_thr  = h >> BBAR_LINE_THR_FACTOR;                        // bln=!(wpix>=thr)

    // region_width
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_CTRL0 + offset),
        region_width, BLKBAR_H_WIDTH_BIT, BLKBAR_H_WIDTH_WID);
    // win_he
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_H_START_END + offset),
        (h - 1), BLKBAR_HEND_BIT, BLKBAR_HEND_WID);
    // win_ve
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_V_START_END + offset),
        (v - 1), BLKBAR_VEND_BIT, BLKBAR_VEND_WID);
    // bblk_thr_on_bpix
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_CNT_THRESHOLD + offset),
        block_thr, BLKBAR_CNT_TH_BIT, BLKBAR_CNT_TH_WID);
    // blnt_thr_on_wpix
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_ROW_TH1_TH2 + offset),
        line_thr, BLKBAR_ROW_TH1_BIT, BLKBAR_ROW_TH1_WID);
    // blnb_thr_on_wpix
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_ROW_TH1_TH2 + offset),
        line_thr, BLKBAR_ROW_TH2_BIT, BLKBAR_ROW_TH2_WID);
    // en
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_CTRL0 + offset),
        1, BLKBAR_DET_TOP_EN_BIT, BLKBAR_DET_TOP_EN_WID);
    // manual reset, rst = 0 & 1, raising edge mode
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_CTRL0 + offset),
        0, BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
    WRITE_CBUS_REG_BITS((VDIN_BLKBAR_CTRL0 + offset),
        1, BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
}

static inline void vdin_set_histogram(unsigned int offset, unsigned int hs, unsigned int he, unsigned int vs, unsigned int ve)
{
    unsigned int pixel_sum = 0, record_len = 0, hist_pow = 0;
    if ((hs < he) && (vs < ve))
    {
        pixel_sum = (he - hs + 1) * (ve - vs + 1);
        record_len = 0xffff<<3;
        while ((pixel_sum > record_len) && (hist_pow < 3))
        {
            hist_pow++;
            record_len <<= 1;
        }
        // pow
        WRITE_CBUS_REG_BITS((VDIN_HIST_CTRL + offset),
            hist_pow, HIST_POW_BIT, HIST_POW_WID);
        // win_hs
        WRITE_CBUS_REG_BITS((VDIN_HIST_H_START_END + offset),
            hs, HIST_HSTART_BIT, HIST_HSTART_WID);
        // win_he
        WRITE_CBUS_REG_BITS((VDIN_HIST_H_START_END + offset),
            he, HIST_HEND_BIT, HIST_HEND_WID);
        // win_vs
        WRITE_CBUS_REG_BITS((VDIN_HIST_V_START_END + offset),
            vs, HIST_VSTART_BIT, HIST_VSTART_WID);
        // win_ve
        WRITE_CBUS_REG_BITS((VDIN_HIST_V_START_END + offset),
            ve, HIST_VEND_BIT, HIST_VEND_WID);
    }
}

static inline void vdin_set_wr_ctrl(unsigned int offset, unsigned int v, unsigned int h, enum vdin_format_convert_e format_convert)
{
    unsigned int write_format444 = 0;

    switch (format_convert)
    {
        case VDIN_FORMAT_CONVERT_YUV_YUV422:
        case VDIN_FORMAT_CONVERT_RGB_YUV422:
            write_format444 = 0;
            break;
        default:
            write_format444 = 1;
            break;
    }

    // win_he
    WRITE_CBUS_REG_BITS((VDIN_WR_H_START_END + offset),
        (h -1), WR_HEND_BIT, WR_HEND_WID);
    // win_ve
#if defined(CONFIG_ARCH_MESON)
    WRITE_CBUS_REG_BITS((VDIN_WR_V_START_END + offset),
        (v), WR_VEND_BIT, WR_VEND_WID);
#elif defined(CONFIG_ARCH_MESON2)
    WRITE_CBUS_REG_BITS((VDIN_WR_V_START_END + offset),
        (v -1), WR_VEND_BIT, WR_VEND_WID);
#endif

    // format444
    WRITE_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        write_format444, WR_FMT_BIT, WR_FMT_WID);
    /*
    // canvas_id
    WRITE_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        VDIN_START_CANVAS, WR_CANVAS_BIT, WR_CANVAS_WID);
    */
    // req_urgent
    WRITE_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        1, WR_REQ_URGENT_BIT, WR_REQ_URGENT_WID);
    // req_en
    WRITE_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
}

void set_wr_ctrl(int h_pos,int v_pos,struct vdin_dev_s *devp)
{
    enum tvin_sig_fmt_e fmt = devp->parm.info.fmt;
    int def_h_pos = tvin_fmt_tbl[fmt].hs_bp;
    int def_v_pos = tvin_fmt_tbl[fmt].vs_bp;
    if(h_pos + def_h_pos <0){
        unsigned int w_s = abs(h_pos + def_h_pos);
        w_s = (1 + (w_s>>3))<<3;
        WRITE_CBUS_REG_BITS((VDIN_WR_H_START_END + devp->addr_offset),
        w_s, WR_HSTART_BIT, WR_HSTART_WID);
    }else{
        WRITE_CBUS_REG_BITS((VDIN_WR_H_START_END + devp->addr_offset),
        0, WR_HSTART_BIT, WR_HSTART_WID);
    }
    if(v_pos + def_v_pos<0){
        WRITE_CBUS_REG_BITS((VDIN_WR_V_START_END + devp->addr_offset),
        abs(v_pos + def_v_pos), WR_VSTART_BIT, WR_VSTART_WID);
    }else{
        WRITE_CBUS_REG_BITS((VDIN_WR_V_START_END + devp->addr_offset),
        0, WR_VSTART_BIT, WR_VSTART_WID);
    }
    vdin_set_wr_ctrl(devp->addr_offset,devp->v_active, devp->h_active, devp->format_convert);
}

/***************************global function**********************************/

#if defined(CONFIG_ARCH_MESON2)
inline void vdin_get_meas_timing(struct vdin_dev_s *devp)
{
    unsigned int offset = devp->addr_offset;
    unsigned int hs_cnt = 0;
    unsigned long long hi = 0, lo = 0;

    hs_cnt = READ_CBUS_REG_BITS((VDIN_MEAS_HS_COUNT + offset),
                            MEAS_HS_CNT_BIT, MEAS_HS_CNT_WID);
    devp->meas_th = (hs_cnt+32)>>6;
    hi = (unsigned long long)(READ_CBUS_REG_BITS((VDIN_MEAS_VS_COUNT_HI + offset),
                        MEAS_VS_TOTAL_CNT_HI_BIT, MEAS_VS_TOTAL_CNT_HI_WID));
    lo = (unsigned long long)(READ_CBUS_REG((VDIN_MEAS_VS_COUNT_LO + offset)));
    devp->meas_tv = (unsigned int)((hi<<(unsigned long long)32)|lo);
}

inline unsigned int vdin_get_meas_hcnt64(struct vdin_dev_s *devp)
{
    unsigned int offset = devp->addr_offset;

    return (READ_CBUS_REG_BITS((VDIN_MEAS_HS_COUNT + offset),
                            MEAS_HS_CNT_BIT, MEAS_HS_CNT_WID));
}
#else
inline unsigned int vdin_get_meas_hcnt64(struct vdin_dev_s *devp)
{
    return 0;
}
#endif

inline unsigned int vdin_get_active_h(unsigned int offset)
{
    return (READ_CBUS_REG_BITS(VDIN_ACTIVE_MAX_PIX_CNT_STATUS + offset, ACTIVE_MAX_PIX_CNT_SDW_BIT, ACTIVE_MAX_PIX_CNT_SDW_WID) + 1);
}

inline unsigned int vdin_get_active_v(unsigned int offset)
{
    return (READ_CBUS_REG_BITS(VDIN_LCNT_SHADOW_STATUS + offset, ACTIVE_LN_CNT_SDW_BIT, ACTIVE_LN_CNT_SDW_WID) + 1);
}

inline unsigned int vdin_get_total_v(unsigned int offset)
{
    return (READ_CBUS_REG_BITS(VDIN_LCNT_SHADOW_STATUS + offset, GO_LN_CNT_SDW_BIT, GO_LN_CNT_SDW_WID));
}

inline void vdin_set_canvas_id(unsigned int offset, unsigned int canvas_id)
{
    WRITE_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        canvas_id, WR_CANVAS_BIT, WR_CANVAS_WID);
}

inline unsigned int vdin_get_canvas_id(unsigned int offset)
{
    return READ_CBUS_REG_BITS((VDIN_WR_CTRL + offset),
        WR_CANVAS_BIT, WR_CANVAS_WID);
}

/* reset default writing cavnas register */
inline void vdin_set_def_wr_canvas(struct vdin_dev_s *devp)
{
    unsigned int offset = devp->addr_offset;
    unsigned int def_canvas;
    def_canvas = vdin_canvas_ids[devp->index][0];

    // [31:24]       write.out_ctrl         = 0x0b
    // [   23]       write.frame_rst_on_vs  = 1
    // [   22]       write.lfifo_rst_on_vs  = 1
    // [   21]       write.clr_direct_done  = 0
    // [   20]       write.clr_nr_done      = 0
    // [   12]       write.format444        = 1/(422, 444)
    // [   11]       write.canvas_latch_en  = 0
    // [    9]       write.req_urgent       = 0 ***sub_module.enable***
    // [    8]       write.req_en           = 0 ***sub_module.enable***
    // [ 7: 0]       write.canvas           = 0
    WRITE_CBUS_REG((VDIN_WR_CTRL + offset), (0x0bc01000 | def_canvas));
}

inline void vdin_set_vframe_prop_info(vframe_t *vf, unsigned int offset)
{
//    unsigned int offset = devp->addr_offset;
    unsigned int vs_cnt_hi = 0, vs_cnt_lo = 0;
    struct vframe_bbar_s bbar = {0};

    // fetch hist info
    //vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,     HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   );
    vf->prop.hist.luma_sum   = READ_CBUS_REG(VDIN_HIST_SPL_VAL + offset);
    //vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,  HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID );
    vf->prop.hist.chroma_sum = READ_CBUS_REG(VDIN_HIST_CHROMA_SUM + offset);
    vf->prop.hist.pixel_sum  = READ_CBUS_REG_BITS((VDIN_HIST_SPL_PIX_CNT + offset),
                                HIST_PIX_CNT_BIT,     HIST_PIX_CNT_WID    );
    vf->prop.hist.luma_max   = READ_CBUS_REG_BITS((VDIN_HIST_MAX_MIN + offset),
                                HIST_MAX_BIT,         HIST_MAX_WID        );
    vf->prop.hist.luma_min   = READ_CBUS_REG_BITS((VDIN_HIST_MAX_MIN + offset),
                                HIST_MIN_BIT,         HIST_MIN_WID        );
    vf->prop.hist.gamma[0]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST00 + offset),
                                HIST_ON_BIN_00_BIT,   HIST_ON_BIN_00_WID  );
    vf->prop.hist.gamma[1]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST00 + offset),
                                HIST_ON_BIN_01_BIT,   HIST_ON_BIN_01_WID  );
    vf->prop.hist.gamma[2]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST01 + offset),
                                HIST_ON_BIN_02_BIT,   HIST_ON_BIN_02_WID  );
    vf->prop.hist.gamma[3]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST01 + offset),
                                HIST_ON_BIN_03_BIT,   HIST_ON_BIN_03_WID  );
    vf->prop.hist.gamma[4]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST02 + offset),
                                HIST_ON_BIN_04_BIT,   HIST_ON_BIN_04_WID  );
    vf->prop.hist.gamma[5]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST02 + offset),
                                HIST_ON_BIN_05_BIT,   HIST_ON_BIN_05_WID  );
    vf->prop.hist.gamma[6]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST03 + offset),
                                HIST_ON_BIN_06_BIT,   HIST_ON_BIN_06_WID  );
    vf->prop.hist.gamma[7]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST03 + offset),
                                HIST_ON_BIN_07_BIT,   HIST_ON_BIN_07_WID  );
    vf->prop.hist.gamma[8]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST04 + offset),
                                HIST_ON_BIN_08_BIT,   HIST_ON_BIN_08_WID  );
    vf->prop.hist.gamma[9]   = READ_CBUS_REG_BITS((VDIN_DNLP_HIST04 + offset),
                                HIST_ON_BIN_09_BIT,   HIST_ON_BIN_09_WID  );
    vf->prop.hist.gamma[10]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST05 + offset),
                                HIST_ON_BIN_10_BIT,   HIST_ON_BIN_10_WID  );
    vf->prop.hist.gamma[11]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST05 + offset),
                                HIST_ON_BIN_11_BIT,   HIST_ON_BIN_11_WID  );
    vf->prop.hist.gamma[12]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST06 + offset),
                                HIST_ON_BIN_12_BIT,   HIST_ON_BIN_12_WID  );
    vf->prop.hist.gamma[13]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST06 + offset),
                                HIST_ON_BIN_13_BIT,   HIST_ON_BIN_13_WID  );
    vf->prop.hist.gamma[14]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST07 + offset),
                                HIST_ON_BIN_14_BIT,   HIST_ON_BIN_14_WID  );
    vf->prop.hist.gamma[15]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST07 + offset),
                                HIST_ON_BIN_15_BIT,   HIST_ON_BIN_15_WID  );
    vf->prop.hist.gamma[16]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST08 + offset),
                                HIST_ON_BIN_16_BIT,   HIST_ON_BIN_16_WID  );
    vf->prop.hist.gamma[17]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST08 + offset),
                                HIST_ON_BIN_17_BIT,   HIST_ON_BIN_17_WID  );
    vf->prop.hist.gamma[18]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST09 + offset),
                                HIST_ON_BIN_18_BIT,   HIST_ON_BIN_18_WID  );
    vf->prop.hist.gamma[19]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST09 + offset),
                                HIST_ON_BIN_19_BIT,   HIST_ON_BIN_19_WID  );
    vf->prop.hist.gamma[20]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST10 + offset),
                                HIST_ON_BIN_20_BIT,   HIST_ON_BIN_20_WID  );
    vf->prop.hist.gamma[21]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST10 + offset),
                                HIST_ON_BIN_21_BIT,   HIST_ON_BIN_21_WID  );
    vf->prop.hist.gamma[22]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST11 + offset),
                                HIST_ON_BIN_22_BIT,   HIST_ON_BIN_22_WID  );
    vf->prop.hist.gamma[23]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST11 + offset),
                                HIST_ON_BIN_23_BIT,   HIST_ON_BIN_23_WID  );
    vf->prop.hist.gamma[24]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST12 + offset),
                                HIST_ON_BIN_24_BIT,   HIST_ON_BIN_24_WID  );
    vf->prop.hist.gamma[25]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST12 + offset),
                                HIST_ON_BIN_25_BIT,   HIST_ON_BIN_25_WID  );
    vf->prop.hist.gamma[26]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST13 + offset),
                                HIST_ON_BIN_26_BIT,   HIST_ON_BIN_26_WID  );
    vf->prop.hist.gamma[27]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST13 + offset),
                                HIST_ON_BIN_27_BIT,   HIST_ON_BIN_27_WID  );
    vf->prop.hist.gamma[28]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST14 + offset),
                                HIST_ON_BIN_28_BIT,   HIST_ON_BIN_28_WID  );
    vf->prop.hist.gamma[29]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST14 + offset),
                                HIST_ON_BIN_29_BIT,   HIST_ON_BIN_29_WID  );
    vf->prop.hist.gamma[30]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST15 + offset),
                                HIST_ON_BIN_30_BIT,   HIST_ON_BIN_30_WID  );
    vf->prop.hist.gamma[31]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST15 + offset),
                                HIST_ON_BIN_31_BIT,   HIST_ON_BIN_31_WID  );
    vf->prop.hist.gamma[32]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST16 + offset),
                                HIST_ON_BIN_32_BIT,   HIST_ON_BIN_32_WID  );
    vf->prop.hist.gamma[33]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST16 + offset),
                                HIST_ON_BIN_33_BIT,   HIST_ON_BIN_33_WID  );
    vf->prop.hist.gamma[34]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST17 + offset),
                                HIST_ON_BIN_34_BIT,   HIST_ON_BIN_34_WID  );
    vf->prop.hist.gamma[35]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST17 + offset),
                                HIST_ON_BIN_35_BIT,   HIST_ON_BIN_35_WID  );
    vf->prop.hist.gamma[36]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST18 + offset),
                                HIST_ON_BIN_36_BIT,   HIST_ON_BIN_36_WID  );
    vf->prop.hist.gamma[37]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST18 + offset),
                                HIST_ON_BIN_37_BIT,   HIST_ON_BIN_37_WID  );
    vf->prop.hist.gamma[38]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST19 + offset),
                                HIST_ON_BIN_38_BIT,   HIST_ON_BIN_38_WID  );
    vf->prop.hist.gamma[39]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST19 + offset),
                                HIST_ON_BIN_39_BIT,   HIST_ON_BIN_39_WID  );
    vf->prop.hist.gamma[40]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST20 + offset),
                                HIST_ON_BIN_40_BIT,   HIST_ON_BIN_40_WID  );
    vf->prop.hist.gamma[41]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST20 + offset),
                                HIST_ON_BIN_41_BIT,   HIST_ON_BIN_41_WID  );
    vf->prop.hist.gamma[42]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST21 + offset),
                                HIST_ON_BIN_42_BIT,   HIST_ON_BIN_42_WID  );
    vf->prop.hist.gamma[43]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST21 + offset),
                                HIST_ON_BIN_43_BIT,   HIST_ON_BIN_43_WID  );
    vf->prop.hist.gamma[44]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST22 + offset),
                                HIST_ON_BIN_44_BIT,   HIST_ON_BIN_44_WID  );
    vf->prop.hist.gamma[45]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST22 + offset),
                                HIST_ON_BIN_45_BIT,   HIST_ON_BIN_45_WID  );
    vf->prop.hist.gamma[46]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST23 + offset),
                                HIST_ON_BIN_46_BIT,   HIST_ON_BIN_46_WID  );
    vf->prop.hist.gamma[47]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST23 + offset),
                                HIST_ON_BIN_47_BIT,   HIST_ON_BIN_47_WID  );
    vf->prop.hist.gamma[48]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST24 + offset),
                                HIST_ON_BIN_48_BIT,   HIST_ON_BIN_48_WID  );
    vf->prop.hist.gamma[49]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST24 + offset),
                                HIST_ON_BIN_49_BIT,   HIST_ON_BIN_49_WID  );
    vf->prop.hist.gamma[50]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST25 + offset),
                                HIST_ON_BIN_50_BIT,   HIST_ON_BIN_50_WID  );
    vf->prop.hist.gamma[51]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST25 + offset),
                                HIST_ON_BIN_51_BIT,   HIST_ON_BIN_51_WID  );
    vf->prop.hist.gamma[52]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST26 + offset),
                                HIST_ON_BIN_52_BIT,   HIST_ON_BIN_52_WID  );
    vf->prop.hist.gamma[53]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST26 + offset),
                                HIST_ON_BIN_53_BIT,   HIST_ON_BIN_53_WID  );
    vf->prop.hist.gamma[54]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST27 + offset),
                                HIST_ON_BIN_54_BIT,   HIST_ON_BIN_54_WID  );
    vf->prop.hist.gamma[55]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST27 + offset),
                                HIST_ON_BIN_55_BIT,   HIST_ON_BIN_55_WID  );
    vf->prop.hist.gamma[56]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST28 + offset),
                                HIST_ON_BIN_56_BIT,   HIST_ON_BIN_56_WID  );
    vf->prop.hist.gamma[57]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST28 + offset),
                                HIST_ON_BIN_57_BIT,   HIST_ON_BIN_57_WID  );
    vf->prop.hist.gamma[58]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST29 + offset),
                                HIST_ON_BIN_58_BIT,   HIST_ON_BIN_58_WID  );
    vf->prop.hist.gamma[59]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST29 + offset),
                                HIST_ON_BIN_59_BIT,   HIST_ON_BIN_59_WID  );
    vf->prop.hist.gamma[60]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST30 + offset),
                                HIST_ON_BIN_60_BIT,   HIST_ON_BIN_60_WID  );
    vf->prop.hist.gamma[61]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST30 + offset),
                                HIST_ON_BIN_61_BIT,   HIST_ON_BIN_61_WID  );
    vf->prop.hist.gamma[62]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST31 + offset),
                                HIST_ON_BIN_62_BIT,   HIST_ON_BIN_62_WID  );
    vf->prop.hist.gamma[63]  = READ_CBUS_REG_BITS((VDIN_DNLP_HIST31 + offset),
                                HIST_ON_BIN_63_BIT,   HIST_ON_BIN_63_WID  );

    // fetch bbar info
    bbar.top        = READ_CBUS_REG_BITS((VDIN_BLKBAR_STATUS0 + offset),
                                BLKBAR_TOP_POS_BIT,   BLKBAR_TOP_POS_WID  );
    bbar.bottom     = READ_CBUS_REG_BITS((VDIN_BLKBAR_STATUS0 + offset),
                                BLKBAR_BTM_POS_BIT,   BLKBAR_BTM_POS_WID  );
    bbar.left       = READ_CBUS_REG_BITS((VDIN_BLKBAR_STATUS1 + offset),
                                BLKBAR_LEFT_POS_BIT,  BLKBAR_LEFT_POS_WID );
    bbar.right      = READ_CBUS_REG_BITS((VDIN_BLKBAR_STATUS1 + offset),
                                BLKBAR_RIGHT_POS_BIT, BLKBAR_RIGHT_POS_WID);
    if(bbar.top > bbar.bottom){
        bbar.top = 0;
        bbar.bottom = vf->height - 1;
    }
    if(bbar.left > bbar.right){
        bbar.left = 0;
        bbar.right = vf->width - 1;
    }

    // Update Histgram windown with detected BlackBar window
    vdin_set_histogram(offset, bbar.left, bbar.right, bbar.top, bbar.bottom);

    if (black_bar_enable)
    {
        vf->prop.bbar.top        = bbar.top;
        vf->prop.bbar.bottom     = bbar.bottom;
        vf->prop.bbar.left       = bbar.left;
        vf->prop.bbar.right      = bbar.right;
    }
    else
        memset(&vf->prop.bbar, 0, sizeof(struct vframe_bbar_s));

#if defined(CONFIG_ARCH_MESON2)
    // fetch meas info - For M2 or further chips only, not for M1 chip
    vf->prop.meas.vs_span_cnt = READ_CBUS_REG_BITS((VDIN_MEAS_VS_COUNT_HI + offset),
                    MEAS_IND_VS_TOTAL_CNT_N_BIT, MEAS_IND_VS_TOTAL_CNT_N_WID);

    vs_cnt_hi = READ_CBUS_REG_BITS((VDIN_MEAS_VS_COUNT_HI + offset),
                    MEAS_VS_TOTAL_CNT_HI_BIT, MEAS_VS_TOTAL_CNT_HI_WID);
    vs_cnt_lo = READ_CBUS_REG((VDIN_MEAS_VS_COUNT_LO + offset));
    vf->prop.meas.vs_cnt =
        ((unsigned long long)vs_cnt_hi<<(unsigned long long)32)
        | (unsigned long long)vs_cnt_lo;
#else
    vf->prop.meas.vs_span_cnt = 0;
    vf->prop.meas.vs_cnt      = 0;
#endif

    vf->prop.meas.hs_cnt0     = 0;
    vf->prop.meas.hs_cnt1     = 0;
    vf->prop.meas.hs_cnt2     = 0;
    vf->prop.meas.hs_cnt3     = 0;
}

static inline ulong vdin_reg_limit(ulong val, ulong wid)
{
    if (val < (1<<wid))
        return(val);
    else
        return((1<<wid)-1);
}


void vdin_set_all_regs(struct vdin_dev_s *devp)
{
    /* matrix sub-module */
    vdin_set_color_matrix(devp->addr_offset, devp->parm.info.fmt, devp->format_convert);

    /* bbar sub-module */
    vdin_set_bbar(devp->addr_offset, devp->v_active, devp->h_active);

    /* hist sub-module */
    vdin_set_histogram(devp->addr_offset, 0, devp->h_active - 1, 0, devp->v_active - 1);

    /* write sub-module */
    vdin_set_wr_ctrl(devp->addr_offset, devp->v_active, devp->h_active, devp->format_convert);

    /* top sub-module */
    vdin_set_top(devp->addr_offset, devp->parm.port, devp->h_active);

    /*  */
#if defined(CONFIG_ARCH_MESON2)
        vdin_set_meas_mux(devp->addr_offset, devp->parm.port);
#endif
}

void vdin_delay_line(unsigned short num,unsigned int offset)
{
    unsigned int val = 0u;
    unsigned int old = 0u;
    val = 0x80000 | (num<<12);
    old = READ_CBUS_REG(VDIN_SCALE_COEF+ offset);
    val = old | val;
    WRITE_CBUS_REG((VDIN_SCALE_COEF + offset), val);
}
inline void vdin_set_default_regmap(unsigned int offset)
{
    unsigned int def_canvas_id;
//    unsigned int offset = devp->addr_offset;

    // [   31]        mpeg.en               = 0 ***sub_module.enable***
    // [   30]        mpeg.even_fld         = 0/(odd, even)

#if defined(CONFIG_ARCH_MESON)
    // [   29]        mpeg.force_vs         = 0 // for test
    // [   28]        mpeg.force_hs         = 0 // for test
#elif defined(CONFIG_ARCH_MESON2)
    // [   27]        mpeg.vs_en            = 0
#endif

    // [26:20]         top.hold_ln          = 0    //8
    // [   19]      vs_dly.en               = 0 ***sub_module.enable***
    // [18:12]      vs_dly.dly_ln           = 0
    // [11:10]         map.comp2            = 2/(comp0, comp1, comp2)
    // [ 9: 8]         map.comp1            = 1/(comp0, comp1, comp2)
    // [ 7: 6]         map.comp0            = 0/(comp0, comp1, comp2)

#if defined(CONFIG_ARCH_MESON2)
    // [    5]   input_win.en               = 0 ***sub_module.enable***
#endif

    // [    4]         top.datapath_en      = 1
    // [ 3: 0]         top.mux              = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin)
    WRITE_CBUS_REG((VDIN_COM_CTRL0            + offset), 0x00000910);
    // [   23] asfifo_tvfe.de_en            = 1
    // [   22] asfifo_tvfe.vs_en            = 1
    // [   21] asfifo_tvfe.hs_en            = 1
    // [   20] asfifo_tvfe.vs_inv           = 0/(positive-active, negative-active)
    // [   19] asfifo_tvfe.hs_inv           = 0/(positive-active, negative-active)
    // [   18] asfifo_tvfe.rst_on_vs        = 1
    // [   17] asfifo_tvfe.clr_ov_flag      = 0
    // [   16] asfifo_tvfe.rst              = 0
    // [    7]  asfifo_656.de_en            = 1
    // [    6]  asfifo_656.vs_en            = 1
    // [    5]  asfifo_656.hs_en            = 1
    // [    4]  asfifo_656.vs_inv           = 0/(positive-active, negative-active)
    // [    3]  asfifo_656.hs_inv           = 0/(positive-active, negative-active)
    // [    2]  asfifo_656.rst_on_vs        = 0
    // [    1]  asfifo_656.clr_ov_flag      = 0
    // [    0]  asfifo_656.rst              = 0
    WRITE_CBUS_REG((VDIN_ASFIFO_CTRL0         + offset), 0x00e400e0);
    // [   23] asfifo_hdmi.de_en            = 1
    // [   22] asfifo_hdmi.vs_en            = 1
    // [   21] asfifo_hdmi.hs_en            = 1
    // [   20] asfifo_hdmi.vs_inv           = 0/(positive-active, negative-active)
    // [   19] asfifo_hdmi.hs_inv           = 0/(positive-active, negative-active)
    // [   18] asfifo_hdmi.rst_on_vs        = 1
    // [   17] asfifo_hdmi.clr_ov_flag      = 0
    // [   16] asfifo_hdmi.rst              = 0
    // [    7] asfifo_cvd2.de_en            = 1
    // [    6] asfifo_cvd2.vs_en            = 1
    // [    5] asfifo_cvd2.hs_en            = 1
    // [    4] asfifo_cvd2.vs_inv           = 0/(positive-active, negative-active)
    // [    3] asfifo_cvd2.hs_inv           = 0/(positive-active, negative-active)
    // [    2] asfifo_cvd2.rst_on_vs        = 1
    // [    1] asfifo_cvd2.clr_ov_flag      = 0
    // [    0] asfifo_cvd2.rst              = 0
    WRITE_CBUS_REG((VDIN_ASFIFO_CTRL1         + offset), 0x00e400e4);
    // [28:16]         top.input_width_m1   = 0
    // [12: 0]         top.output_width_m1  = 0
    WRITE_CBUS_REG((VDIN_WIDTHM1I_WIDTHM1O    + offset), 0x00000000);
    // [14: 8]         hsc.init_pix_in_ptr  = 0
    // [    7]         hsc.phsc_en          = 0
    // [    6]         hsc.en               = 0 ***sub_module.enable***
    // [    5]         hsc.short_ln_en      = 1
    // [    4]         hsc.nearest_en       = 0
    // [    3]         hsc.phase0_always    = 1
    // [ 2: 0]         hsc.filt_dep         = 0/(DEPTH4,DEPTH1, DEPTH2, DEPTH3)
    WRITE_CBUS_REG((VDIN_SC_MISC_CTRL         + offset), 0x00000028);
    // [28:24]         hsc.phase_step_int   = 0 <u5.0>
    // [23: 0]         hsc.phase_step_fra   = 0 <u0.24>
    WRITE_CBUS_REG((VDIN_HSC_PHASE_STEP       + offset), 0x00000000);
    // [30:29]         hsc.repeat_pix0_num  = 1 // ? to confirm pix0 is always used
    // [28:24]         hsc.ini_receive_num  = 4 // ? to confirm pix0 is always used
    // [23: 0]         hsc.ini_phase        = 0
    WRITE_CBUS_REG((VDIN_HSC_INI_CTRL         + offset), 0x24000000);

#if defined(CONFIG_ARCH_MESON2)
    // [   25]  decimation.rst              = 0
    // [   24]  decimation.en               = 0 ***sub_module.enable***
    // [23:20]  decimation.phase            = 0
    // [19:16]  decimation.ratio            = 0/(1, 1/2, ..., 1/16)
    // [    7] asfifo_dvin.de_en            = 1
    // [    6] asfifo_dvin.vs_en            = 1
    // [    5] asfifo_dvin.hs_en            = 1
    // [    4] asfifo_dvin.vs_inv           = 0/(positive-active, negative-active)
    // [    3] asfifo_dvin.hs_inv           = 0/(positive-active, negative-active)
    // [    2] asfifo_dvin.rst_on_vs        = 1
    // [    1] asfifo_dvin.clr_ov_flag      = 0
    // [    0] asfifo_dvin.rst              = 0
    WRITE_CBUS_REG((VDIN_ASFIFO_CTRL2         + offset), 0x000000e4);
#endif

    // [    0]      matrix.en               = 0 ***sub_module.enable***
    WRITE_CBUS_REG((VDIN_MATRIX_CTRL          + offset), 0x00000000);
    // [28:16]      matrix.coef00           = 0 <s2.10>
    // [12: 0]      matrix.coef01           = 0 <s2.10>
    WRITE_CBUS_REG((VDIN_MATRIX_COEF00_01     + offset), 0x00000000);
    // [28:16]      matrix.coef02           = 0 <s2.10>
    // [12: 0]      matrix.coef10           = 0 <s2.10>
    WRITE_CBUS_REG((VDIN_MATRIX_COEF02_10     + offset), 0x00000000);
    // [28:16]      matrix.coef11           = 0 <s2.10>
    // [12: 0]      matrix.coef12           = 0 <s2.10>
    WRITE_CBUS_REG((VDIN_MATRIX_COEF11_12     + offset), 0x00000000);
    // [28:16]      matrix.coef20           = 0 <s2.10>
    // [12: 0]      matrix.coef21           = 0 <s2.10>
    WRITE_CBUS_REG((VDIN_MATRIX_COEF20_21     + offset), 0x00000000);
    // [12: 0]      matrix.coef22           = 0 <s2.10>
    WRITE_CBUS_REG((VDIN_MATRIX_COEF22        + offset), 0x00000000);
    // [26:16]      matrix.offset0          = 0 <s8.2>
    // [10: 0]      matrix.ofsset1          = 0 <s8.2>
    WRITE_CBUS_REG((VDIN_MATRIX_OFFSET0_1     + offset), 0x00000000);
    // [10: 0]      matrix.ofsset2          = 0 <s8.2>
    WRITE_CBUS_REG((VDIN_MATRIX_OFFSET2       + offset), 0x00000000);
    // [26:16]      matrix.pre_offset0      = 0 <s8.2>
    // [10: 0]      matrix.pre_ofsset1      = 0 <s8.2>
    WRITE_CBUS_REG((VDIN_MATRIX_PRE_OFFSET0_1 + offset), 0x00000000);
    // [10: 0]      matrix.pre_ofsset2      = 0 <s8.2>
    WRITE_CBUS_REG((VDIN_MATRIX_PRE_OFFSET2   + offset), 0x00000000);
    // [11: 0]       write.lfifo_buf_size   = 0x100
    WRITE_CBUS_REG((VDIN_LFIFO_CTRL           + offset), 0x00000100);
    // [15:14]     clkgate.bbar             = 0/(auto, off, on, on)
    // [13:12]     clkgate.bbar             = 0/(auto, off, on, on)
    // [11:10]     clkgate.bbar             = 0/(auto, off, on, on)
    // [ 9: 8]     clkgate.bbar             = 0/(auto, off, on, on)
    // [ 7: 6]     clkgate.bbar             = 0/(auto, off, on, on)
    // [ 5: 4]     clkgate.bbar             = 0/(auto, off, on, on)
    // [ 3: 2]     clkgate.bbar             = 0/(auto, off, on, on)
    // [    0]     clkgate.bbar             = 0/(auto, off!!!!!!!!)
    WRITE_CBUS_REG((VDIN_COM_GCLK_CTRL        + offset), 0x00000000);

#if defined(CONFIG_ARCH_MESON2)
    // [12: 0]  decimation.output_width_m1  = 0
    WRITE_CBUS_REG((VDIN_INTF_WIDTHM1         + offset), 0x00000000);
#endif

    def_canvas_id = offset? vdin_canvas_ids[1][0]:vdin_canvas_ids[0][0];

    // [31:24]       write.out_ctrl         = 0x0b
    // [   23]       write.frame_rst_on_vs  = 1
    // [   22]       write.lfifo_rst_on_vs  = 1
    // [   21]       write.clr_direct_done  = 0
    // [   20]       write.clr_nr_done      = 0
    // [   12]       write.format444        = 1/(422, 444)
    // [   11]       write.canvas_latch_en  = 0
    // [    9]       write.req_urgent       = 0 ***sub_module.enable***
    // [    8]       write.req_en           = 0 ***sub_module.enable***
    // [ 7: 0]       write.canvas           = 0
    WRITE_CBUS_REG((VDIN_WR_CTRL              + offset), (0x0bc01000 | def_canvas_id));
    // [27:16]       write.output_hs        = 0
    // [11: 0]       write.output_he        = 0
    WRITE_CBUS_REG((VDIN_WR_H_START_END       + offset), 0x00000000);
    // [27:16]       write.output_vs        = 0
    // [11: 0]       write.output_ve        = 0
    WRITE_CBUS_REG((VDIN_WR_V_START_END       + offset), 0x00000000);
    // [ 6: 5]        hist.pow              = 0
    // [ 3: 2]        hist.mux              = 0/(matrix_out, hsc_out, phsc_in)
    // [    1]        hist.win_en           = 1
    // [    0]        hist.read_en          = 1
    WRITE_CBUS_REG((VDIN_HIST_CTRL            + offset), 0x00000003);
    // [28:16]        hist.win_hs           = 0
    // [12: 0]        hist.win_he           = 0
    WRITE_CBUS_REG((VDIN_HIST_H_START_END     + offset), 0x00000000);
    // [28:16]        hist.win_vs           = 0
    // [12: 0]        hist.win_ve           = 0
    WRITE_CBUS_REG((VDIN_HIST_V_START_END     + offset), 0x00000000);

#if defined(CONFIG_ARCH_MESON2)
    //set VDIN_MEAS_CLK_CNTL, select XTAL clock
    WRITE_CBUS_REG(VDIN_MEAS_CLK_CNTL, 0x00000100);

    // [   18]        meas.rst              = 0
    // [   17]        meas.widen_hs_vs_en   = 1
    // [   16]        meas.vs_cnt_accum_en  = 0
    // [14:12]        meas.mux              = 0/(null, 656, tvfe, cvd2, hdmi, dvin)
    // [11: 4]        meas.vs_span_m1       = 0
    // [ 2: 0]        meas.hs_ind           = 0
    WRITE_CBUS_REG((VDIN_MEAS_CTRL0           + offset), 0x00020000);
    // [28:16]        meas.hs_range_start   = 112  // HS range0: Line #112 ~ Line #175
    // [12: 0]        meas.hs_range_end     = 175
    WRITE_CBUS_REG((VDIN_MEAS_HS_RANGE        + offset), 0x007000af);
#endif

#if defined(CONFIG_ARCH_MESON2)
    // [    8]        bbar.white_en         = 0
    // [ 7: 0]        bbar.white_thr        = 0
    WRITE_CBUS_REG((VDIN_BLKBAR_CTRL1         + offset), 0x00000000);
#endif

#if defined(CONFIG_ARCH_MESON)
    // [31:22]        bbar.black_thr        = 4
#elif defined(CONFIG_ARCH_MESON2)
    // [31:24]        bbar.black_thr        = 0x14
#endif

    // [20: 8]        bbar.region_width     = 0
    // [ 7: 5]        bbar.src_on_v         = 0/(Y, sU, sV, U, V)
    // [    4]        bbar.search_one_step  = 0
    // [    3]        bbar.raising_edge_rst = 0
    // [ 2: 1]        bbar.mux              = 0/(matrix_out, hsc_out, phsc_in)
    // [    0]        bbar.en               = 0 ***sub_module.enable***
    WRITE_CBUS_REG((VDIN_BLKBAR_CTRL0         + offset), 0x14000000);
    // [28:16]        bbar.win_hs           = 0
    // [12: 0]        bbar.win_he           = 0
    WRITE_CBUS_REG((VDIN_BLKBAR_H_START_END   + offset), 0x00000000);
    // [28:16]        bbar.win_vs           = 0
    // [12: 0]        bbar.win_ve           = 0
    WRITE_CBUS_REG((VDIN_BLKBAR_V_START_END   + offset), 0x00000000);
    // [19: 0]        bbar.bblk_thr_on_bpix = 0
    WRITE_CBUS_REG((VDIN_BLKBAR_CNT_THRESHOLD + offset), 0x00000000);
    // [28:16]        bbar.blnt_thr_on_wpix = 0
    // [12: 0]        bbar.blnb_thr_on_wpix = 0
    WRITE_CBUS_REG((VDIN_BLKBAR_ROW_TH1_TH2   + offset), 0x00000000);

#if defined(CONFIG_ARCH_MESON2)
    // [28:16]   input_win.hs               = 0
    // [12: 0]   input_win.he               = 0
    WRITE_CBUS_REG((VDIN_WIN_H_START_END      + offset), 0x00000000);
    // [28:16]   input_win.vs               = 0
    // [12: 0]   input_win.ve               = 0
    WRITE_CBUS_REG((VDIN_WIN_V_START_END      + offset), 0x00000000);
#endif
}

inline void vdin_hw_enable(unsigned int offset)
{
    /* enable video data input */
    // [    4]  top.datapath_en  = 1
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset), 1, 4, 1);

    /* mux input */
    // [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin)
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset), 0, 0, 4);

    /* enable clock of blackbar, histogram, histogram, line fifo1, matrix,
     * hscaler, pre hscaler, clock0
     */
    // [15:14]  Enable blackbar clock       = 00/(auto, off, on, on)
    // [13:12]  Enable histogram clock      = 00/(auto, off, on, on)
    // [11:10]  Enable line fifo1 clock     = 00/(auto, off, on, on)
    // [ 9: 8]  Enable matrix clock         = 00/(auto, off, on, on)
    // [ 7: 6]  Enable hscaler clock        = 00/(auto, off, on, on)
    // [ 5: 4]  Enable pre hscaler clock    = 00/(auto, off, on, on)
    // [ 3: 2]  Enable clock0               = 00/(auto, off, on, on)
    // [    0]  Enable register clock       = 00/(auto, off!!!!!!!!)
    WRITE_CBUS_REG((VDIN_COM_GCLK_CTRL + offset), 0x0);
}


inline void vdin_hw_disable(unsigned int offset)
{
    /* disable video data input */
    // [    4]  top.datapath_en  = 0
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset), 0, 4, 1);

    /* mux null input */
    // [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin)
    WRITE_CBUS_REG_BITS((VDIN_COM_CTRL0 + offset), 0, 0, 4);

    /* disable clock of blackbar, histogram, histogram, line fifo1, matrix,
     * hscaler, pre hscaler, clock0
     */
    // [15:14]  Disable blackbar clock      = 01/(auto, off, on, on)
    // [13:12]  Disable histogram clock     = 01/(auto, off, on, on)
    // [11:10]  Disable line fifo1 clock    = 01/(auto, off, on, on)
    // [ 9: 8]  Disable matrix clock        = 01/(auto, off, on, on)
    // [ 7: 6]  Disable hscaler clock       = 01/(auto, off, on, on)
    // [ 5: 4]  Disable pre hscaler clock   = 01/(auto, off, on, on)
    // [ 3: 2]  Disable clock0              = 01/(auto, off, on, on)
    // [    0]  Enable register clock       = 00/(auto, off!!!!!!!!)
    WRITE_CBUS_REG((VDIN_COM_GCLK_CTRL + offset), 0x5554);
}

/* get current vsync field type 0:top 1 bottom */
inline unsigned int vdin_get_field_type(unsigned int offset)
{
    return READ_CBUS_REG_BITS((VDIN_COM_STATUS0 + offset), 0, 1);
}



void vdin_enable_module(unsigned int offset, bool enable)
{
    if (enable)
    {
        //set VDIN_MEAS_CLK_CNTL, select XTAL clock
        WRITE_CBUS_REG((VDIN_MEAS_CLK_CNTL + offset), 0x00000100);
		//vdin_hw_enable(offset);
        //todo: check them
    }
    else
    {
        //set VDIN_MEAS_CLK_CNTL, select XTAL clock
        WRITE_CBUS_REG((VDIN_MEAS_CLK_CNTL + offset), 0x00000000);
		vdin_hw_disable(offset);
    }
}

/* check invalid vs to avoid screen flicker */
inline bool vdin_check_vs(struct vdin_dev_s *devp)
{
    bool ret = false;
    unsigned int hmeas = 0, vmeas = 0;

    if ((devp->parm.port < TVIN_PORT_COMP0) || (devp->parm.port > TVIN_PORT_COMP7))
        return ret;

    /* check vs after n*vs avoid unstable signal after TVIN_IOC_START_DEC*/
    if (devp->vs_cnt_valid++ >= VDIN_WAIT_VALID_VS)
        devp->vs_cnt_valid = VDIN_WAIT_VALID_VS;

    /* check hcnt/vcnt to find the wrong vs */
    hmeas = READ_CBUS_REG(VDIN_MEAS_HS_COUNT);
    vmeas = READ_CBUS_REG(VDIN_MEAS_VS_COUNT_LO);
    if ((abs((signed int)devp->meas_hcnt - (signed int)hmeas) > VDIN_MEAS_HSCNT_DIFF) ||
        (abs((signed int)devp->meas_vcnt - (signed int)vmeas) > VDIN_MEAS_VSCNT_DIFF)
       )
    {
        devp->meas_hcnt = hmeas;
        devp->meas_vcnt = vmeas;
        if (devp->vs_cnt_valid >= VDIN_WAIT_VALID_VS)
            devp->vs_cnt_ignore = VDIN_IGNORE_VS_CNT;
    }

    /* Do not send wrong data to video buffer */
    if (devp->vs_cnt_ignore)
    {
        devp->vs_cnt_ignore--;
        ret = true;
    }

    return ret;
}


