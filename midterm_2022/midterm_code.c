#define PHATCTL 0x00420 /* offset for telescope register */
#define SOLAR_DEPLOY_ON 0x1
#define CAM_CTRL_ON 0x4
#define ET_SIG_PROCESS_HOME 0x100
#define RUN_CMD_ON 0x80000000

#define CAM_ON_TIME_MS 2000 /* in ms */
#define DELAY_BETWEEN_CMDS_MS 400
#define ONE_SEC_TRANS_DELAY 1000

#define HWREG_U32(bar, offset) (u32*)((bar) + (offset))

static int kelper_use_camera(void)
{
	u32 __iomem *workreg = HWREG_U32(hw_addr, PHATCTL);
	u32 regval = ioread32(workreg);

	/* retract solar array */
	regval &= ~(SOLAR_DEPLOY_ON); 
	regval |= RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* delay between commands */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	/* turn camera on */
	regval |= CAM_CTRL_ON | RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* let camera be on for 2 seconds */
	mdelay(CAM_ON_TIME_MS);

	/* turn off camera */
	regval &= ~(CAM_CTRL_ON);
	regval |= RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* delay between commands */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	/* turn solar array back on */
	regval |= SOLAR_DEPLOY_ON | RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* delay between commands */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	return 0;
}

static int kelper_send_data()
{
	u32 __iomem *workreg = HWREG_U32(hw_addr, PHATCTL);
	u32 regval = ioread32(workreg);
	
	/* retract solar arrays for transmission */
	regval &= ~(SOLAR_DEPLOY_ON); 
	regval |= RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* delay between commands */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	/* send 1 second of data */
	regval |= ET_SIG_PROCESS_HOME | RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* 1 second transmission delay */
	mdelay(ONE_SEC_TRANS_DELAY);

	/* stop tranmission */
	regval &= ~(ET_SIG_PROCESS_HOME);
	regval |= RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* wait for cmd cycle */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	/* send second second of data */
	regval |= ET_SIG_PROCESS_HOME | RUN_CMD_ON;
	iowrite32(regval, workreg);
	
	/* 1 second tranmission delay */
	mdelay(ONE_SEC_TRANS_DELAY);

	/* end transmission of second chunk of data */
	regval &= ~(ET_SIG_PROCESS_HOME);
	regval |= RUN_CMD_ON;
	iowrite32(regval, workreg);

	/* wait for cmd cycle */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	/* reengage solar arrays */
	regval |= SOLAR_DEPLOY_ON | RUN_CMD_ON;
	iowrite32(regval, workreg);
	
	/* wait for cmd cycle */
	mdelay(DELAY_BETWEEN_CMDS_MS);

	return 0;
}

static int run_midterm_problem()
{
	kelper_use_camera();
	kelper_send_data()
}
