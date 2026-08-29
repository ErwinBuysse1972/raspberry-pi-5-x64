#pragma once
#include <memory>
#include <string>
#include <vector>
#include <linux/gpio.h>
#include <gpiod.h>
#include "RP1Base.h"
#include "../Tracer/ctracer.h"


namespace SB::RPI5
{
    static constexpr int RP1I2C_MAX_CHANNELS    = 6;
    // DW_apb_i2c bits (from your databook)
    static constexpr uint32_t IC_TAR_SPECIAL          = (1u << 11);
    static constexpr uint32_t IC_TAR_SMBUS_QUICK_CMD  = (1u << 16);

    static constexpr uint32_t IC_DATA_CMD_CMD_READ    = (1u << 8);   // 1=read, 0=write
    static constexpr uint32_t IC_DATA_CMD_STOP        = (1u << 9);
    static constexpr uint32_t IC_DATA_CMD_RESTART     = (1u << 10);

    // RAW_INTR_STAT bits (DW_apb_i2c standard)
    static constexpr uint32_t RAW_INTR_STAT_TX_ABRT   = (1u << 6);
    static constexpr uint32_t RAW_INTR_STAT_STOP_DET  = (1u << 9);


    // Registers described starting page 154
    // https://picture.iczhiku.com/resource/eetop/SYIdEouasSjJWbmv.pdf
    struct I2CRegs
    {
        volatile int32_t con;                // R/W
        volatile int32_t tar;                // R/W
        volatile int32_t sar;                // R/W
        volatile uint32_t pad0;              // R/W     IC_HS_MADDR
        volatile int32_t data_cmd;           // R/W
        volatile int32_t ss_scl_hcnt;        // R/W standard speed I2C clock SCL High
        volatile int32_t ss_scl_lcnt;        // R/W standard speed I2C clock SCL low
        volatile int32_t fs_scl_hcnt;        // R/W Fast mode and fastmode plus speed I2C clock SCL high
        volatile int32_t fs_scl_lcnt;        // R/W Fast mode and fastmode plus speed I2C clock SCL low
        volatile uint32_t _pad1[2];          //  IC_HS_SCL_HCNT/ IC_HS_SCL_LCNT High speed I2C clock SCL
        volatile uint32_t intr_stat;         // R Interrupt status
        volatile int32_t intr_mask;          // R/W interrupt mask
        volatile int32_t raw_intr_stat;      // R Raw interrupt status
        volatile int32_t rx_tl;              // R/W I2C receive FIFO Threshold
        volatile int32_t tx_tl;              // R/W I2C transmit FIFO Threshold
        volatile int32_t clr_intr;           // R Clear combined and individual interrupts (1-bit)
        volatile int32_t clr_rx_under;       // R clear RX_UNDERr interrupt (1-bit)
        volatile int32_t clr_rx_over;        // R clear RX_OVER interrupt (1-bit)
        volatile int32_t clr_tx_over;        // R clear TX_OVER interrupt (1-bit)
        volatile int32_t clr_rd_req;         // R clear RD_REQ interrupt (1-bit)
        volatile int32_t clr_tx_abrt;        // R clear TX_ABORT interrupt (1-bit)
        volatile int32_t clr_rx_done;        // R clear the RX_DONE interrupt (1-bit)
        volatile int32_t clr_activity;       // R clear ACTIVITY interrupt (1-bit)
        volatile int32_t clr_stop_det;       // R clear STOP detect interrupt (1-bit)
        volatile int32_t clr_start_det;      // R clear START detect interrupt (1-bit)
        volatile int32_t clr_gen_call;       // R clear GEN_CALL interrupt
        volatile int32_t enable;             // R/W enable
        volatile int32_t status;             // R Status register
        volatile int32_t txflr;              // R Transmit FIFO Level register
        volatile int32_t rxflr;              // R Receive FIFO Level register
        volatile int32_t sda_hold;           // R/W SDA hold time length register
        volatile int32_t tx_abrt_source;     // R Transmit abort status register
        volatile int32_t slv_data_nack_only; // R/W Generate SLV_DATA_NACK Register (1-bit)
        volatile int32_t dma_cr;             // R/W DMA Control register for transmit and receive handshake interface
        volatile int32_t dma_tdlr;           // R/W DMA Trannsmit data level
        volatile int32_t dma_rdlr;           // R/W DMA Receive data level
        volatile int32_t sda_setup;          // R/W SDA Setup Register
        volatile int32_t ack_general_call;   // R/W Ack general call register
        volatile int32_t enable_status;      // R Enable status register  
        volatile int32_t fs_spklen;          // ISS and FS spike suppression limit
        volatile uint32_t _pad2;             // HS spike suppression limit
        volatile int32_t clr_restart_det;    // CLR_RESTART_DET clear RESTART_DET interrupt
        volatile int32_t SCL_STUCK_AT_LOW_TIMEOUT;       // SCL Stuck at low timeout register
        volatile int32_t IC_SDA_STUCK_AT_LOW_TIMEOUT;    // SDA Stuck at Low Timeout
        volatile uint32_t _pad3[16];  
        volatile uint32_t comp_param_1;      // Component parameter register
        volatile uint32_t comp_version;      // Component version ID
        volatile uint32_t comp_type;         // Component type register value 0x44570140
    } ;

    class RP1I2C : public RP1Base
    {
        public:
            RP1I2C(std::shared_ptr<CTracer> tracer, uint baudrate = 0);
            virtual ~RP1I2C();

            bool setChannel(int channel, uint32_t baudrate = 1000000);
            int getChannel();
            uint32_t init(uint32_t baudrate, I2CRegs* i2c = nullptr);
            int32_t setBaudrate(int32_t baudrate);
            void reset();
            int readBlocking(uint8_t addr, uint8_t *dst, size_t len, bool nonstop);
            int writeBlocking(uint8_t addr, const uint8_t *src, size_t len, bool nonstop);
            int writeReadBlocking(uint8_t addr, const uint8_t *writeData, size_t writeLen, uint8_t *readData, size_t readLen, uint32_t timeoutUS = 100000);
            int readTimeoutPerCharUs(uint8_t addr, uint8_t *dst, size_t len, bool nonstop, uint32_t timeoutPerCharUs);
            int writeTimeoutPerCharUs(uint8_t addr, const uint8_t *src, size_t len, bool nonstop, uint32_t timeoutPerCharUs);
            bool writeRegister8(uint8_t deviceAddress, uint8_t registerAddress, uint8_t value);
            bool readRegsiter8(uint8_t deviceAddress, uint8_t registerAddress, uint8_t& value);
            bool writeRegisters(uint8_t deviceAddress, uint8_t startAddress, const uint8_t* values, size_t count);
            bool readRegisters(uint8_t deviceAddress, uint8_t startAddress, uint8_t* values, size_t count);
            bool probeQuickWriteAddressOnly(uint8_t addr7, uint32_t timeoutUs);
            uint32_t getComponentVersion();
            uint32_t getComponentType();
            uint32_t getInfo();
            std::string getInfoString(uint32_t info);

            std::string dumpAllRegs(const char *msg);
            bool enable(bool enable);
        private:
            I2CRegs* m_regs;
            bool m_resetOnNext;
            uint32_t m_baudrate;

            int m_channel;
            const uint32_t constClk = 200000000;
            const uint8_t constTxBufferDepth= 32;

            uint64_t micros();
            int32_t handleAbort(bool timeout, int32_t abortreason);
            size_t getWriteAvailable();
            size_t getReadAvailable();
            int writeBlockingInternal(uint8_t addr, const uint8_t *src, size_t len, bool nonstop, uint32_t timeoutPerCharUs);
            int readBlockingInternal(uint8_t addr, uint8_t* dst, size_t len, bool nonStop, uint32_t timeoutPerCharUs);
            std::string LOGREG(const char* title, uint32_t reg, bool bitHeader = false);
    };
}