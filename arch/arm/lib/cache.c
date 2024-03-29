/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

/* for now: just dummy functions to satisfy the linker */

#include <common.h>
#include <asm/cache.h>
#ifndef CONFIG_L2_OFF
#include <asm/cache-l2x0.h>
#endif

void  flush_cache (unsigned long dummy1, unsigned long dummy2)
{
#if defined(CONFIG_OMAP2420) || defined(CONFIG_ARM1136)
	void arm1136_cache_flush(void);

	arm1136_cache_flush();
#endif
#ifdef CONFIG_ARM926EJS
	/* test and clean, page 2-23 of arm926ejs manual */
	asm("0: mrc p15, 0, r15, c7, c10, 3\n\t" "bne 0b\n" : : : "memory");
	/* disable write buffer as well (page 2-22) */
	asm("mcr p15, 0, %0, c7, c10, 4" : : "r" (0));
#endif
#ifdef CONFIG_OMAP34XX
	void v7_flush_cache_all(void);

	v7_flush_cache_all();
#endif
	dcache_flush();

	return;
}

void cache_flush(void)
{
	asm ("mcr p15, 0, %0, c7, c5, 0": :"r" (0));
}

void dcache_flush(void)
{
#ifndef CONFIG_DCACHE_OFF
    if(dcache_status())
    {
        _clean_invd_dcache();
    }
#endif

#ifndef CONFIG_L2_OFF
    if(l2x0_status())
    {
        l2x0_clean_inv_all();
    }
#endif
}

#ifndef CONFIG_L2_OFF
void l2_cache_enable(void)
{
	l2x0_enable();
}
void l2_cache_disable(void)
{
	l2x0_disable();
}
int  l2_cache_status(void)
{
	return l2x0_status();
}
#endif

void dcache_flush_line(unsigned addr)
{
#ifndef CONFIG_DCACHE_OFF
        _clean_invd_dcache_addr(addr);    
#endif
#ifndef CONFIG_L2_OFF
      l2x0_flush_line(addr);  
#endif

}
void dcache_clean_line(unsigned addr)
{
#ifndef CONFIG_DCACHE_OFF
        _clean_dcache_addr(addr);    
#endif
#ifndef CONFIG_L2_OFF
      l2x0_clean_line(addr);  
#endif

}
void dcache_inv_line(unsigned addr)
{
#ifndef CONFIG_DCACHE_OFF
        _invalidate_dcache_addr(addr);    
#endif
#ifndef CONFIG_L2_OFF
      l2x0_inv_line(addr);  
#endif

}
#ifndef CONFIG_SYS_CACHE_LINE_SIZE
#error please define 'CONFIG_SYS_CACHE_LINE_SIZE'
#else
#define CACHE_LINE_SIZE CONFIG_SYS_CACHE_LINE_SIZE
#endif
void dcache_flush_range(unsigned start, unsigned size)
{
    unsigned st,end,i;
    st=start&(~(CACHE_LINE_SIZE-1));
    end=start+size;
    for(i=st;i<end;i+=CACHE_LINE_SIZE)
    {
        dcache_flush_line(i);
    }
#ifndef CONFIG_L2_OFF
    l2x0_wait_flush();
#endif    
}
void dcache_clean_range(unsigned start,unsigned size)
{
    unsigned st,end,i;
    st=start&(~(CACHE_LINE_SIZE-1));
    end=start+size;
    for(i=st;i<end;i+=CACHE_LINE_SIZE)
    {
        dcache_clean_line(i);
    }
#ifndef CONFIG_L2_OFF
    l2x0_wait_clean();
#endif    
    
}
void dcache_invalid_range(unsigned start, unsigned size)
{
    unsigned st,end,i;
    st=start&(~(CACHE_LINE_SIZE-1));
    end=(start+size)&(~(CACHE_LINE_SIZE-1));
    if(st!=start)
    {
        dcache_flush_line(st);
    }
    if(end!=(start+size))
    {
        dcache_flush_line(end);
        end+=CACHE_LINE_SIZE;
    }
#ifndef CONFIG_L2_OFF
    l2x0_wait_flush();
#endif    
    for(i=st;i<end;i+=CACHE_LINE_SIZE)
    {
        dcache_inv_line(i);
    }
#ifndef CONFIG_L2_OFF
    l2x0_wait_inv();
#endif    
    
}

void icache_invalid(void)
{
#ifndef CONFIG_ICACHE_OFF        
    _invalidate_icache();
#endif    	
}

void dcache_invalid(void)
{
#ifndef CONFIG_DCACHE_OFF
    _invalidate_dcache();
#endif	
#ifndef CONFIG_L2_OFF
    l2x0_inv_all();
#endif
}

void dcache_clean(void)
{
#ifndef CONFIG_DCACHE_OFF
    if(dcache_status())
    {
        _clean_dcache();
    }
#endif

#ifndef CONFIG_L2_OFF
   if(l2x0_status())
    {
        l2x0_clean_all();
    }
#endif		
}