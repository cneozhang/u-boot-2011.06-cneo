/*
 * (C) Copyright 2006 OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

#include <nand.h>
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>

#define S3C2440_NFCONT_SECCL       (1<<6)
#define S3C2440_NFCONT_MECCL       (1<<5)
#define S3C2440_NFCONT_INITECC     (1<<4)
#define S3C2440_NFCONT_nCE         (1<<1)
#define S3C2440_NFCONT_MODE        (1<<0) 
#define S3C2440_NFCONF_TACLS(x)    ((x)<<12)
#define S3C2440_NFCONF_TWRPH0(x)   ((x)<<8)
#define S3C2440_NFCONF_TWRPH1(x)   ((x)<<4)
 
#define S3C2440_ADDR_NALE       0x08
#define S3C2440_ADDR_NCLE		0x0C


#ifdef CONFIG_NAND_SPL

/* in the early stage of NAND flash booting, printf() is not available */
#define printf(fmt, args...)

static void nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(this->IO_ADDR_R);
}
#endif

static void s3c2440_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
 
{
	struct nand_chip *chip =mtd->priv;
    struct s3c2440_nand *nand = s3c2440_get_base_nand();
 
    debugX(1,"hwcontrol(): 0x%02x 0x%02x\n", cmd, ctrl);
 
    if (ctrl & NAND_CTRL_CHANGE) {
    	ulong IO_ADDR_W = (ulong)nand;
        if (!(ctrl & NAND_CLE))
            IO_ADDR_W |= S3C2440_ADDR_NCLE;
 
        if (!(ctrl & NAND_ALE))
            IO_ADDR_W |= S3C2440_ADDR_NALE;
 
        if(cmd == NAND_CMD_NONE)
            IO_ADDR_W = &nand->nfdata;
 
        chip->IO_ADDR_W = (void *)IO_ADDR_W;
 
        if (ctrl & NAND_NCE)
            writel(readl(&nand->nfcont)& ~S3C2440_NFCONT_nCE,
                &nand->nfcont);
        else
            writel(readl(&nand->nfcont)| S3C2440_NFCONT_nCE,
                &nand->nfcont);
    }
 
    if (cmd !=NAND_CMD_NONE)
        writeb(cmd, chip->IO_ADDR_W);
}


static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	debugX(1, "dev_ready\n");
	return readl(&nand->nfstat) & 0x01;
}

#ifdef CONFIG_S3C2440_NAND_HWECC
void s3c2440_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	debugX(1, "s3c2440_nand_init_hwecc(%p, %d)\n", mtd, mode);
	/* init ecc */
	writel(readl(&nand->nfcont) | S3C2440_NFCONT_INITECC, &nand->nfcont);
	/* unlock main ecc */
	writel(readl(&nand->nfcont) & (~S3C2440_NFCONT_MECCL), &nand->nfcont); 
}

static int s3c2440_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				      u_char *ecc_code)
{
	u_int32_t ecc;
	struct s3c2440_nand *nand = s3c2440_get_base_nand();

	/* lock main ecc */
	writel(readl(&nand->nfcont) | S3C2440_NFCONT_MECCL, &nand->nfcont);

	ecc = readl(&nand->nfmecc0);
	ecc_code[0] = (u_int8_t)(ecc&0x000000ff);
	ecc_code[1] = (u_int8_t)((ecc&0x0000ff00)>>8);
	ecc_code[2] = (u_int8_t)((ecc&0x00ff0000)>>16);
	ecc_code[3] = (u_int8_t)((ecc&0xff000000)>>24);
	debugX(1, "s3c2440_nand_calculate_hwecc(%p,): 0x%02x 0x%02x 0x%02x  0x%02x\n",
	       mtd , ecc_code[0], ecc_code[1], ecc_code[2], ecc_code[3]);

	return 0;
}

static int s3c2440_nand_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
#if 0
	u_int32_t ecc_stat;
	u_int32_t err_byte, err_bit;
	u_int8_t temp;
	struct s3c2440_nand *nand = s3c2440_get_base_nand();

	writel((u_int32_t)(read_ecc[1]<<16) | read_ecc[0], &nand->nfmeccd0);
	writel((u_int32_t)(read_ecc[4]<<16) | read_ecc[3], &nand->nfmeccd1);

	ecc_stat = readl(&nand->nfestat0);
	
	if((ecc_stat & 0x3)== 0)			/* no error */
		return 0; 	
	
	else if((ecc_stat & 0x3) == 1){		/* 1-bit error */
		err_byte =~(ecc_stat >> 7); 
		err_byte &=0x7ff;
		err_bit =~(ecc_stat>>4);
		err_bit &=0x7;
		
		temp= *(dat+err_byte);
	
		temp = (temp&(1<<err_bit))?(temp&(~(1<<err_bit))):(temp|(1<<err_bit));
	
		*(dat+err_byte) = temp; 
		printf("s3c2440_nand_correct_data,1-bit error and corrected: ");
		printf("Byte.%d - Bit.%d\n",err_byte,err_bit);
		return 0;
	}

	else{								/* error */
		printf("s3c2440_nand_correct_data: not implemented\n");
		printf("calc_ecc[0],calc_ecc[1],calc_ecc[2],calc_ecc[3]:"
			"0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			calc_ecc[0],calc_ecc[1],calc_ecc[2],calc_ecc[3]);
		printf("read_ecc[0],read_ecc[1],read_ecc[2],read_ecc[3]:"
			"0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			read_ecc[0],read_ecc[1],read_ecc[2],read_ecc[3]);
		return -1;
	}
#else
	if (read_ecc[0] == calc_ecc[0] &&
	    read_ecc[1] == calc_ecc[1] &&
	    read_ecc[2] == calc_ecc[2] &&
	    read_ecc[3] == calc_ecc[3])
		return 0;

	printf("s3c2440_nand_correct_data: not implemented\n");
	return -1;
#endif
}

static int s3c2440_nand_calculate_spare_ecc(struct mtd_info *mtd, const u_char *dat,
				      u_char *ecc_code)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();

	/* lock spare ecc */
	writel(readl(&nand->nfcont) | S3C2440_NFCONT_SECCL, &nand->nfcont);
	
	ecc_code[0] = readb(&nand->nfsecc);
	ecc_code[1] = readb(&nand->nfsecc + 2);

	debugX(1, "s3c2440_nand_calculate_spare_ecc(%p,): 0x%02x 0x%02x\n",
	       mtd , ecc_code[0], ecc_code[2]);

	return 0;
}

static int s3c2440_nand_correct_spare_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	if (read_ecc[0] == calc_ecc[0] &&
	    read_ecc[1] == calc_ecc[1] &&
	    read_ecc[2] == calc_ecc[2] &&
	    read_ecc[3] == calc_ecc[3])
		return 0;

	printf("s3c2440_nand_correct_data: not implemented\n");
	return -1;
}

#endif

int board_nand_init(struct nand_chip *nand)
 
{
	u_int32_t cfg;
    u_int8_t tacls, twrph0, twrph1;
    struct s3c24x0_clock_power *clk_power = s3c24x0_get_base_clock_power();
    struct s3c2440_nand *nand_reg = s3c2440_get_base_nand();
 
    debugX(1,"board_nand_init()\n");
	/* Enable NAND flash clk */
    writel(readl(&clk_power->clkcon) |(1 << 4), &clk_power->clkcon);
 
    /* initialize hardware */
#if defined(CONFIG_S3C24XX_CUSTOM_NAND_TIMING)
    tacls = CONFIG_S3C24XX_TACLS;
    twrph0 = CONFIG_S3C24XX_TWRPH0;
    twrph1 = CONFIG_S3C24XX_TWRPH1;
#else
    tacls = 1;
    twrph0 = 3;
    twrph1 = 1;
#endif
    cfg = S3C2440_NFCONF_TACLS(tacls);
    cfg |= S3C2440_NFCONF_TWRPH0(twrph0 - 1);
    cfg |= S3C2440_NFCONF_TWRPH1(twrph1 - 1);
    writel(cfg,&nand_reg->nfconf);
 
    cfg = S3C2440_NFCONT_SECCL;
    cfg |= S3C2440_NFCONT_MECCL;
    cfg |= S3C2440_NFCONT_MODE;
    writel(cfg,&nand_reg->nfcont);
 
    /* initialize nand_chip data structure */
    nand->IO_ADDR_R = (void*)&nand_reg->nfdata;
    nand->IO_ADDR_W = (void*)&nand_reg->nfdata;
    nand->select_chip = NULL;
 
    /* read_buf and write_buf are default */
    /* read_byte and write_byte are default*/
 
#ifdef CONFIG_NAND_SPL
    nand->read_buf = nand_read_buf;
#endif

    /* hwcontrol always must be implemented*/
    nand->cmd_ctrl = s3c2440_hwcontrol;
    nand->dev_ready = s3c2440_dev_ready;

#ifdef CONFIG_S3C2440_NAND_HWECC
    nand->ecc.hwctl = s3c2440_nand_enable_hwecc;
    nand->ecc.calculate = s3c2440_nand_calculate_ecc;
    nand->ecc.correct = s3c2440_nand_correct_data;
    nand->ecc.mode = NAND_ECC_HW;
    nand->ecc.size =CONFIG_SYS_NAND_ECCSIZE;
    nand->ecc.bytes =CONFIG_SYS_NAND_ECCBYTES;
#else
    nand->ecc.mode = NAND_ECC_SOFT;
#endif
 
#ifdef CONFIG_S3C2440_NAND_BBT
    nand->options = NAND_USE_FLASH_BBT;
#else
    nand->options = 0;
#endif
    debugX(1, "end ofnand_init\n");
    return 0;
}

