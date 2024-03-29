#include <config.h>
#include <asm/arch/cpu.h>
#include <asm/arch/io.h>
#include <asm/arch/reg_addr.h>

#define  I2C_CONTROL_REG      (volatile unsigned long *)0xc8100500
#define  I2C_SLAVE_ADDR       (volatile unsigned long *)0xc8100504
#define  I2C_TOKEN_LIST_REG0  (volatile unsigned long *)0xc8100508
#define  I2C_TOKEN_LIST_REG1  (volatile unsigned long *)0xc810050c
#define  I2C_TOKEN_WDATA_REG0 (volatile unsigned long *)0xc8100510
#define  I2C_TOKEN_WDATA_REG1 (volatile unsigned long *)0xc8100514
#define  I2C_TOKEN_RDATA_REG0 (volatile unsigned long *)0xc8100518
#define  I2C_TOKEN_RDATA_REG1 (volatile unsigned long *)0xc810051c

#define  I2C_END               0x0
#define  I2C_START             0x1
#define  I2C_SLAVE_ADDR_WRITE  0x2
#define  I2C_SLAVE_ADDR_READ   0x3
#define  I2C_DATA              0x4
#define  I2C_DATA_LAST         0x5
#define  I2C_STOP              0x6

void hard_i2c_init(void)
{
    (*I2C_CONTROL_REG) = ((*I2C_CONTROL_REG) & ~(0x3FF << 12)) | (500 << 12);
    
    setbits_le32(P_AO_RTI_PIN_MUX_REG, 3 << 5);
}

//uncomment this function if you need read back

unsigned char hard_i2c_read8(unsigned char SlaveAddr, unsigned char RegAddr)
{    
    // Set the I2C Address
    (*I2C_SLAVE_ADDR) = ((*I2C_SLAVE_ADDR) & ~0xff) | SlaveAddr;
    // Fill the token registers
    (*I2C_TOKEN_LIST_REG0) = (I2C_END  << 24)             |
                             (I2C_DATA_LAST << 20)        |  // Read Data
                             (I2C_SLAVE_ADDR_READ << 16)  |
                             (I2C_START << 12)            |
                             (I2C_DATA << 8)              |  // Read RegAddr
                             (I2C_SLAVE_ADDR_WRITE << 4)  |
                             (I2C_START << 0);

    // Fill the write data registers
    (*I2C_TOKEN_WDATA_REG0) = (RegAddr << 0);
    // Start and Wait
    (*I2C_CONTROL_REG) &= ~(1 << 0);   // Clear the start bit
    (*I2C_CONTROL_REG) |= (1 << 0);   // Set the start bit
    while( (*I2C_CONTROL_REG) & (1 << 2) ) {}

    return( (unsigned char)((*I2C_TOKEN_RDATA_REG0) & 0xFF) );
}

void hard_i2c_write8(unsigned char SlaveAddr, unsigned char RegAddr, unsigned char Data)
{
    // Set the I2C Address
    (*I2C_SLAVE_ADDR) = ((*I2C_SLAVE_ADDR) & ~0xff) | SlaveAddr;
    // Fill the token registers
    (*I2C_TOKEN_LIST_REG0) = (I2C_END  << 16)             |
                             (I2C_DATA << 12)             |    // Write Data
                             (I2C_DATA << 8)              |    // Write RegAddr
                             (I2C_SLAVE_ADDR_WRITE << 4)  |
                             (I2C_START << 0);

    // Fill the write data registers
    (*I2C_TOKEN_WDATA_REG0) = (Data << 8) | (RegAddr << 0);
    // Start and Wait
    (*I2C_CONTROL_REG) &= ~(1 << 0);   // Clear the start bit
    (*I2C_CONTROL_REG) |= (1 << 0);   // Set the start bit
    while( (*I2C_CONTROL_REG) & (1 << 2) ) {}
}

unsigned char hard_i2c_read168(unsigned char SlaveAddr, unsigned short RegAddr)
{
    unsigned char *pr = (unsigned char*)&RegAddr;
    unsigned char data;

    // Set the I2C Address
    (*I2C_SLAVE_ADDR) = ((*I2C_SLAVE_ADDR) & ~0xff) | SlaveAddr;
    // Fill the token registers
    (*I2C_TOKEN_LIST_REG0) = (I2C_END << 28)        |
                             (I2C_DATA_LAST << 24)             |  // Read Data
                             (I2C_SLAVE_ADDR_READ << 20)  |
                             (I2C_START << 16)            |
                             (I2C_DATA << 12)             |
                             (I2C_DATA << 8)              |  // Read RegAddr
                             (I2C_SLAVE_ADDR_WRITE << 4)  |
                             (I2C_START << 0);

    // Fill the write data registers
    (*I2C_TOKEN_WDATA_REG0) = (*(pr + 1) << 8) | *pr;
    // Start and Wait
    (*I2C_CONTROL_REG) &= ~(1 << 0);   // Clear the start bit
    (*I2C_CONTROL_REG) |= (1 << 0);   // Set the start bit
    while( (*I2C_CONTROL_REG) & (1 << 2) ) {}

    data = *I2C_TOKEN_RDATA_REG0 & 0xff;
    return data;
}

void hard_i2c_write168(unsigned char SlaveAddr, unsigned short RegAddr, unsigned char Data)
{
    unsigned char *pr = (unsigned char*)&RegAddr;

    // Set the I2C Address
    (*I2C_SLAVE_ADDR) = ((*I2C_SLAVE_ADDR) & ~0xff) | SlaveAddr;
    // Fill the token registers
    (*I2C_TOKEN_LIST_REG0) = (I2C_END << 20)              |
                             (I2C_DATA << 16)             |    // Write Data
                             (I2C_DATA << 12)             |
                             (I2C_DATA << 8)              |    // Write RegAddr
                             (I2C_SLAVE_ADDR_WRITE << 4)  |
                             (I2C_START << 0);

    // Fill the write data registers
    (*I2C_TOKEN_WDATA_REG0) = (Data<<16) | (*(pr + 1) << 8) | *pr;
    // Start and Wait
    (*I2C_CONTROL_REG) &= ~(1 << 0);   // Clear the start bit
    (*I2C_CONTROL_REG) |= (1 << 0);   // Set the start bit
    while( (*I2C_CONTROL_REG) & (1 << 2) ) {}
}


