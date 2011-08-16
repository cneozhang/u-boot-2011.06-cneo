/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
#ifdef CONFIG_S3C24X0

#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

DECLARE_GLOBAL_DATA_PTR;


/* macro to read the 16 bit timer */
static inline ulong READ_TIMER(void)
{
	struct s3c24x0_timers *timers = s3c24x0_get_base_timers();

	return readl(&timers->tcnto4) & 0xffff;
}

int timer_init(void)
{
	struct s3c24x0_timers *timers = s3c24x0_get_base_timers();
	ulong tmr;

	/* use PWM Timer 4 because it has no output */
	/* prescaler for Timer 4 is 16 */
	writel(0x0f00, &timers->tcfg0);
	gd->timer_rate_hz = get_PCLK() /(2*16*100);
	gd->tbl = get_PCLK() / (2 * 16);
	/* load value for 10 ms timeout */
	gd->lastinc = gd->timer_rate_hz;
	writel(gd->timer_rate_hz, &timers->tcntb4);
	/* auto load, manual update of timer 4 */
	tmr = (readl(&timers->tcon) & ~0x0700000) | 0x0600000;
	writel(tmr, &timers->tcon);
	/* auto load, start timer 4 */
	tmr = (tmr & ~0x0700000) | 0x0500000;
	writel(tmr, &timers->tcon);
	gd->timer_reset_value = 0;

	return (0);
}

/*
 * timer without interrupts
 */

void reset_timer(void)
{
	reset_timer_masked();
}

ulong get_timer(ulong base)
{
	return get_timer_masked() - base;
}

void set_timer(ulong t)
{
	gd->timer_reset_value = t;
}

void __udelay (unsigned long usec)
{
	ulong tmo;
	ulong start = get_ticks();

	tmo = usec / 1000;
	tmo *= (gd->timer_rate_hz * 100);
	tmo /= 1000;

	while ((ulong) (get_ticks() - start) < tmo)
		/*NOP*/;
}

void reset_timer_masked(void)
{
	/* reset time */
	gd->lastinc = READ_TIMER();
	gd->timer_reset_value = 0;
}

ulong get_timer_masked(void)
{
	ulong tmr = get_ticks();

	return tmr / (gd->tbl / CONFIG_SYS_HZ);
}

void udelay_masked(unsigned long usec)
{
	ulong tmo;
	ulong endtime;
	signed long diff;

	if (usec >= 1000) {
		tmo = usec / 1000;
		tmo *= (gd->timer_rate_hz * 100);
		tmo /= 1000;
	} else {
		tmo = usec * (gd->timer_rate_hz * 100);
		tmo /= (1000 * 1000);
	}

	endtime = get_ticks() + tmo;

	do {
		ulong now = get_ticks();
		diff = endtime - now;
	} while (diff >= 0);
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	ulong now = READ_TIMER();

	if (gd->lastinc >= now) {
		/* normal mode */
		gd->timer_reset_value += gd->lastinc - now;
	} else {
		/* we have an overflow ... */
		gd->timer_reset_value += gd->lastinc + gd->timer_rate_hz - now;
	}
	gd->lastinc = now;

	return gd->timer_reset_value;
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	ulong tbclk;

#if defined(CONFIG_SMDK2400)
	tbclk = gd->timer_rate_hz * 100;
#elif defined(CONFIG_SBC2410X) || \
      defined(CONFIG_SMDK2410) || \
	defined(CONFIG_S3C2440) || \
      defined(CONFIG_VCMA9)
	tbclk = CONFIG_SYS_HZ;
#else
#	error "tbclk not configured"
#endif

	return tbclk;
}

/*
 * reset the cpu by setting up the watchdog timer and let him time out
 */
void reset_cpu(ulong ignored)
{
	struct s3c24x0_watchdog *watchdog;

	watchdog = s3c24x0_get_base_watchdog();

	/* Disable watchdog */
	writel(0x0000, &watchdog->wtcon);

	/* Initialize watchdog timer count register */
	writel(0x0001, &watchdog->wtcnt);

	/* Enable watchdog timer; assert reset at timer timeout */
	writel(0x0021, &watchdog->wtcon);

	while (1)
		/* loop forever and wait for reset to happen */;

	/*NOTREACHED*/
}

#endif /* CONFIG_S3C24X0 */
