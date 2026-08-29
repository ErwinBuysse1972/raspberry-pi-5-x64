#include <memory>
#include <cstddef>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <format>
#include "../Tracer/cfunctracer.h"
#include "SBRP1I2C.h"

namespace SB::RPI5
{
    RP1I2C::RP1I2C(std::shared_ptr<CTracer> tracer, uint32_t baudrate)
        : RP1Base(tracer)
        , m_resetOnNext(false)
        , m_channel(-1)
        , m_baudrate(0)
    {
        CFuncTracer trace("RP1I2C::RP1I2C", m_trace);
        try
        {
            m_regs = (I2CRegs*)I2CBase(0);
            if (baudrate != 0)
            {
                int32_t bd = init(baudrate, m_regs);
                if (bd < 0)
                    trace.Error("Could not set the baudrate to %ld", baudrate);
            }
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
    }
    RP1I2C::~RP1I2C()
    {

    }
    bool RP1I2C::setChannel(int channel, uint32_t baudrate)
    {
        CFuncTracer trace("RP1I2C::setChannel", m_trace);

        try
        {
            if (channel < 0 || channel >= SB::RPI5::RP1I2C_MAX_CHANNELS)
            {
                trace.Error("Wrong channel selected : %d", channel);
                return false;
            }

            trace.Info("channel: %d, baudrate: %u", channel, baudrate);

            if (channel != m_channel)
            {
                m_regs = reinterpret_cast<I2CRegs*>(I2CBase(channel));
                m_channel = channel;
            }

            // Always reinitialize when a baudrate is supplied.
            if (baudrate > 0)
            {
                const int32_t bd = init(baudrate, m_regs);

                if (bd < 0)
                {
                    trace.Error(
                        "Could not set baudrate to %u",
                        baudrate);

                    return false;
                }
            }

             // TEMPORARY DEBUG
            trace.Info("I2C register base          : %p", m_regs);
            trace.Info("sizeof(I2CRegs)            : 0x%zX", sizeof(I2CRegs));
            trace.Info("offsetof(tar)              : 0x%zX", offsetof(I2CRegs, tar));
            trace.Info("offsetof(data_cmd)         : 0x%zX", offsetof(I2CRegs, data_cmd));
            trace.Info("offsetof(raw_intr_stat)    : 0x%zX", offsetof(I2CRegs, raw_intr_stat));
            trace.Info("offsetof(clr_tx_abrt)      : 0x%zX", offsetof(I2CRegs, clr_tx_abrt));
            trace.Info("offsetof(clr_stop_det)     : 0x%zX", offsetof(I2CRegs, clr_stop_det));
            trace.Info("offsetof(tx_abrt_source)   : 0x%zX", offsetof(I2CRegs, tx_abrt_source));

            trace.Info("COMP_TYPE                  : 0x%08X", m_regs->comp_type);
            trace.Info("COMP_VERSION               : 0x%08X", m_regs->comp_version);
            trace.Info("COMP_PARAM_1               : 0x%08X", m_regs->comp_param_1);

            return true;
        }
        catch (const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }

        return false;
    }
    int RP1I2C::getChannel()
    {
        return m_channel;
    }
    uint32_t RP1I2C::init(uint32_t baudrate, I2CRegs* i2c)
    {
        CFuncTracer trace("RP1I2C::init", m_trace);
        try
        {
            trace.Info("baudrate: %u", baudrate);

            if (i2c)
                m_regs = i2c;

            if (!enable(false))
            {
                trace.Error("Failed to disable I2C controller during initialization");
                return static_cast<uint32_t>(-1);
            }

            //
            // Master, fast mode, restart enabled,
            // 7-bit addressing.
            //
            m_regs->con = (0x2ul << 1) | 0x01 | 0x40 | 0x20 | 0x100;

            m_regs->tx_tl = 0;
            m_regs->rx_tl = 0;

            return setBaudrate(baudrate);
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return (uint32_t)-1;
    }
    int32_t RP1I2C::setBaudrate(int32_t baudrate)
    {
        CFuncTracer trace("RP1I2C::setBaudrate", m_trace);
        try
        {
             trace.Info("baudrate: %d", baudrate);

            if (!enable(false))
            {
                trace.Error( "Failed to disable I2C controller while setting baudrate");
                return -1;
            }

            m_regs->con = (m_regs->con & ~0x06ul) | ((0x02ul << 1) & 0x06ul);
            const uint32_t period = (constClk + baudrate / 2) / baudrate;
            m_regs->fs_scl_lcnt = period * 3 / 5;
            m_regs->fs_scl_hcnt = period - m_regs->fs_scl_lcnt;

            //
            // Spike suppression.
            //
            m_regs->fs_spklen = m_regs->fs_scl_lcnt < 16 ? 1 : m_regs->fs_scl_lcnt / 16;

            //
            // SDA hold time.
            //
            const uint32_t sdaTxHoldCount = (baudrate < 1000000) ? ((constClk * 3) / 1000000) + 1 : ((constClk * 3) + 25000000) + 1;
            m_regs->sda_hold = (m_regs->sda_hold & ~0x0000FFFFu) | (sdaTxHoldCount & 0x0000FFFFu);

            if (!enable(true))
            {
                trace.Error("Failed to enable I2C controller after setting baudrate");
                return -1;
            }

            m_baudrate = baudrate;

            return static_cast<int32_t>( constClk / period);
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return 0;        
    }
    void RP1I2C::reset()
    {

    }
    int RP1I2C::readBlocking(uint8_t addr, uint8_t *dst, size_t len, bool nonstop)
    {
        CFuncTracer trace("RP1I2C::readBlocking", m_trace, false);
        if (m_baudrate == 0)
        {
            trace.Error("need to initialize the i2c before you can read");
            return -1;
        }
        int result = readBlockingInternal(addr, dst, len, nonstop, 0xFFFFFFFF);
        if (result < 0)
        {
            trace.Error(
                "I2C read failed: "
                "addr=0x%02X requested=%zu result=%d nonstop=%d",
                addr,
                len,
                result,
                nonstop ? 1 : 0);
        }
        else
        {
            trace.Info(
                "I2C read: "
                "addr=0x%02X requested=%zu read=%d nonstop=%d",
                addr,
                len,
                result,
                nonstop ? 1 : 0);
        }
        if (result > 0 && dst != nullptr)
        {
            std::string buffer;

            for (int i = 0; i < result; ++i)
            {
                if (!buffer.empty())
                    buffer += " ";

                buffer += std::format(
                    "{:02X}",
                    static_cast<unsigned int>(dst[i]));
            }

            trace.Info(
                "I2C RX buffer [%d bytes]: %s",
                result,
                buffer.c_str());
        }
        return result;
    }
    int RP1I2C::writeBlocking(uint8_t addr, const uint8_t *src, size_t len, bool nonstop)
    {
        CFuncTracer trace("RP1I2C::writeBlocking", m_trace, false);
        if (m_baudrate == 0)
        {
            trace.Error("need to initialize the i2c before you can read");
            return -1;
        }
        if (src != nullptr && len > 0)
        {
            std::string buffer;

            for (size_t i = 0; i < len; ++i)
            {
                if (!buffer.empty())
                    buffer += " ";

                buffer += std::format(
                    "{:02X}",
                    static_cast<unsigned int>(src[i]));
            }

            trace.Info(
                "I2C write: addr=0x%02X len=%zu nonstop=%d",
                addr,
                len,
                nonstop ? 1 : 0);

            trace.Info(
                "I2C TX buffer [%zu bytes]: %s",
                len,
                buffer.c_str());
        }

        int result = writeBlockingInternal(addr, src, len, nonstop, 0xFFFFFF);
        if (result < 0)
        {
            trace.Error(
                "I2C write failed: "
                "addr=0x%02X requested=%zu result=%d nonstop=%d",
                addr,
                len,
                result,
                nonstop ? 1 : 0);
        }
        else
        {
            trace.Info(
                "I2C write result: "
                "addr=0x%02X requested=%zu written=%d nonstop=%d",
                addr,
                len,
                result,
                nonstop ? 1 : 0);
        }
        return result;
    }
    int RP1I2C::writeReadBlocking(uint8_t addr, const uint8_t *writeData, size_t writeLen, uint8_t *readData, size_t readLen, uint32_t timeoutUs)
    {
        CFuncTracer trace("RP1I2C::writeReadBlocking", m_trace, false);

        try
        {
            if (m_baudrate == 0)
            {
                trace.Error("need to initialize the i2c before you can read");
                return -1;
            }

            if (writeData == nullptr || writeLen == 0)
            {
                trace.Error("Invalid write buffer");
                return -1;
            }

            if (readData == nullptr || readLen == 0)
            {
                trace.Error("Invalid read buffer");
                return -1;
            }

            //
            // Configure the target once for the complete
            // write + repeated START + read transaction.
            //
            if (!enable(false))
            {
                trace.Error("Failed to disable I2C controller");
                return -1;
            }

            m_regs->tar = addr;

            //
            // Clear stale conditions from a previous transaction.
            //
            (void)m_regs->clr_tx_abrt;
            (void)m_regs->clr_stop_det;

            if (!enable(true))
            {
                trace.Error("Failed to enable I2C controller");
                return -1;
            }

            //
            // Log TX data.
            //
            {
                std::string buffer;

                for (size_t i = 0; i < writeLen; ++i)
                {
                    if (!buffer.empty())
                        buffer += " ";

                    buffer += std::format( "{:02X}", static_cast<unsigned int>(writeData[i]));
                }

                trace.Info( "I2C write/read: addr=0x%02X writeLen=%zu readLen=%zu",
                                addr,
                                writeLen,
                                readLen);

                trace.Info("I2C TX buffer [%zu bytes]: %s", writeLen, buffer.c_str());
            }

            size_t writeQueued   = 0;
            size_t readQueued    = 0;
            size_t bytesReceived = 0;

            const uint64_t timeoutEnd = micros() + static_cast<uint64_t>(timeoutUs);

            //
            // Queue the write bytes followed immediately by
            // the read commands.
            //
            while (bytesReceived < readLen)
            {
                //
                // Fill available TX FIFO entries.
                //
                while (getWriteAvailable() > 0)
                {
                    //
                    // First send all write bytes.
                    //
                    if (writeQueued < writeLen)
                    {
                        const uint32_t command = static_cast<uint32_t>( writeData[writeQueued]);

                        //
                        // No STOP here.
                        //
                        m_regs->data_cmd = command;

                        ++writeQueued;
                        continue;
                    }

                    //
                    // Then queue the READ requests.
                    //
                    if (readQueued < readLen)
                    {
                        uint32_t command = IC_DATA_CMD_CMD_READ;

                        //
                        // The first read following the write
                        // generates the repeated START.
                        //
                        if (readQueued == 0)
                            command |= IC_DATA_CMD_RESTART;

                        //
                        // The final read generates STOP.
                        //
                        if (readQueued == readLen - 1)
                            command |= IC_DATA_CMD_STOP;

                        m_regs->data_cmd = command;

                        ++readQueued;
                        continue;
                    }

                    break;
                }

                //
                // Check for I2C abort.
                //
                if (m_regs->raw_intr_stat & RAW_INTR_STAT_TX_ABRT)
                {
                    const uint32_t abortReason = static_cast<uint32_t>(m_regs->tx_abrt_source);

                    trace.Error("I2C write/read aborted: addr=0x%02X reason=0x%08X",
                                addr,
                                abortReason);

                    (void)m_regs->clr_tx_abrt;

                    m_resetOnNext = false;

                    return handleAbort(false, abortReason);
                }

                //
                // Drain received bytes from RX FIFO.
                //
                while (bytesReceived < readLen && getReadAvailable() > 0)
                {
                    readData[bytesReceived] = static_cast<uint8_t>(m_regs->data_cmd & 0xFF);

                    ++bytesReceived;
                }

                //
                // Overall transaction timeout.
                //
                if (micros() > timeoutEnd)
                {
                    trace.Error(
                        "I2C write/read timeout: "
                        "addr=0x%02X "
                        "writeQueued=%zu/%zu "
                        "readQueued=%zu/%zu "
                        "received=%zu/%zu",
                        addr,
                        writeQueued,
                        writeLen,
                        readQueued,
                        readLen,
                        bytesReceived,
                        readLen);

                    const uint32_t abortReason = static_cast<uint32_t>( m_regs->tx_abrt_source);

                    m_resetOnNext = false;

                    return handleAbort( true, abortReason);
                }
            }

            //
            // The last RX byte can become available just before
            // STOP has actually completed on the I2C bus.
            //
            while (!(m_regs->raw_intr_stat & RAW_INTR_STAT_STOP_DET))
            {
                if (m_regs->raw_intr_stat & RAW_INTR_STAT_TX_ABRT)
                {
                    const uint32_t abortReason = static_cast<uint32_t>(m_regs->tx_abrt_source);

                    trace.Error("I2C write/read aborted while waiting for STOP: addr=0x%02X reason=0x%08X",
                        addr,
                        abortReason);

                    (void)m_regs->clr_tx_abrt;

                    m_resetOnNext = false;

                    return handleAbort(false, abortReason);
                }

                if (micros() > timeoutEnd)
                {
                    trace.Error("I2C write/read timeout waiting for STOP: addr=0x%02X",
                        addr);

                    const uint32_t abortReason =static_cast<uint32_t>(m_regs->tx_abrt_source);

                    m_resetOnNext = false;

                    return handleAbort(true, abortReason);
                }
            }

            //
            // Clear STOP condition now that the complete transaction
            // has finished.
            //
            (void)m_regs->clr_stop_det;

            m_resetOnNext = false;

            //
            // Log received data.
            //
            {
                std::string buffer;

                for (size_t i = 0; i < bytesReceived; ++i)
                {
                    if (!buffer.empty())
                        buffer += " ";

                    buffer += std::format( "{:02X}", static_cast<unsigned int>(readData[i]));
                }

                trace.Info( "I2C RX buffer [%zu bytes]: %s", bytesReceived, buffer.c_str());
            }

            return static_cast<int>(bytesReceived);
        }
        catch (const std::exception& e)
        {
            trace.Error( "Exception occurred : %s", e.what());
        }

        m_resetOnNext = false;
        return -1;
    }
    int RP1I2C::readTimeoutPerCharUs(uint8_t addr, uint8_t *dst, size_t len, bool nonstop, uint32_t timeoutPerCharUs)
    {
        CFuncTracer trace("RP1I2C::readTimeoutPerCharUs", m_trace);
        if (m_baudrate == 0)
        {
            trace.Error("need to initialize the i2c before you can read");
            return -1;
        }
        return readBlockingInternal(addr, dst, len, nonstop, timeoutPerCharUs);
    }
    int RP1I2C::writeTimeoutPerCharUs(uint8_t addr, const uint8_t *src, size_t len, bool nonstop, uint32_t timeoutPerCharUs)
    {
        CFuncTracer trace("RP1I2C::writeTimeoutPerCharUs", m_trace);
        if (m_baudrate == 0)
        {
            trace.Error("need to initialize the i2c before you can read");
            return -1;
        }
        return writeBlockingInternal(addr, src, len, nonstop, timeoutPerCharUs);
    }
    bool RP1I2C::writeRegister8(uint8_t deviceAddress, uint8_t registerAddress, uint8_t value)
    {
        CFuncTracer trace("RP1I2C::writeRegister8", m_trace);
        try
        {
            const uint8_t data[] = { registerAddress, value};
            const int result = writeBlocking(deviceAddress, data, sizeof(data), false);
            if (result != sizeof(data))
            {
                trace.Error("Failed to write register 0x%02X to device 0x%02X, result: %d",
                                registerAddress,
                                deviceAddress,
                                result);
                return false;
            }
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    bool RP1I2C::readRegsiter8(uint8_t deviceAddress, uint8_t registerAddress, uint8_t& value)
    {
        CFuncTracer trace("RP1I2C::readRegsiter8", m_trace, false);
        try
        {
            const int result = writeReadBlocking(
                deviceAddress,
                &registerAddress,
                1,
                &value,
                1);

            if (result != 1)
            {
                trace.Error(
                    "Failed to read register 0x%02X "
                    "from device 0x%02X, result=%d",
                    registerAddress,
                    deviceAddress,
                    result);

                return false;
            }

            trace.Info( "I2C register: device=0x%02X register=0x%02X value=0x%02X",
                deviceAddress,
                registerAddress,
                value);

        return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
        return false;
    }
    bool RP1I2C::writeRegisters(uint8_t deviceAddress, uint8_t startAddress, const uint8_t* values, size_t count)
    {
        CFuncTracer trace("RP1I2C::writeRegisters", m_trace);
        try
        {
            if (values == nullptr || count == 0)
            {
                trace.Error("Invalid values buffer or count");
                return false;
            }

            std::vector<uint8_t> data;
            data.reserve(count + 1);

            // first byte is the device's internal register address
            data.push_back(startAddress);

            // Remaining bytes are register data.  For devices supporting
            //   auto-increment, these are written to consecutive registers
            data.insert(data.end(), values, values + count);

            const int result = writeBlocking(deviceAddress, data.data(), data.size(), false);
            if (result != static_cast<int>(data.size()))
            {
                trace.Error(
                    "Failed to write %zu registers starting at 0x%02X "
                    "to device 0x%02X, result %d",
                    count, 
                    startAddress,
                    deviceAddress,
                    result);
                return false;
            }
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
        return false;
    }
    bool RP1I2C::readRegisters(uint8_t deviceAddress, uint8_t startAddress, uint8_t* values, size_t count)
    {
        CFuncTracer trace("RP1I2C::readRegisters", m_trace);
        try
        {
            if (values == nullptr || count == 0)
            {
                trace.Error("Invalid values buffer or count");
                return false;
            }

            const int result = writeReadBlocking(
                                    deviceAddress,
                                    &startAddress,
                                    1,
                                    values,
                                    count);

            if (result != static_cast<int>(count))
            {
                trace.Error(
                    "Failed to read %zu registers starting at 0x%02X "
                    "from device 0x%02X, result %d",
                    count,
                    startAddress,
                    deviceAddress,
                    result);
                return false;
            }
            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occured : %s", e.what());
        }
        return false;
    }


    std::string RP1I2C::LOGREG(const char* title, uint32_t reg, bool bitHeader)
    {
        std::stringstream ss;
        // Header row: bit indices
        if (bitHeader)
        {
            ss << std::setw(25) << "Bit : ";
            for (int i = 31; i >= 0; --i)
            {
                ss << std::setw(2) << i << ' ';
            }
            ss << std::endl;
        }
        // Value row: bit values
        ss << std::setw(25) << title;
        for (int i = 31; i >= 0; --i)
        {
            uint32_t bit = (reg >> i) & 0x1;
            ss << ' ' << bit << ' ';
        }

        // Hex column
        ss << " | 0x"
                << std::hex << std::uppercase
                << std::setw(8) << std::setfill('0') << reg
                << std::dec << std::setfill(' ')
                << std::endl;
        return ss.str();
    }
    std::string RP1I2C::dumpAllRegs(const char *msg)
    {
        CFuncTracer trace("RP1I2C::dumpAllRegs", m_trace);
        std::stringstream ss;
        try
        {
            ss << msg << ":" << std::endl;
            ss << LOGREG("IC_CON", m_regs->con, true);
            ss << LOGREG("IC_TAR", m_regs->tar);
            ss << LOGREG("IC_SAR", m_regs->sar);
            ss << LOGREG("IC_HS_MADDR", m_regs->pad0);
            ss << LOGREG("IC_DATA_CMD", m_regs->data_cmd);
            ss << LOGREG("IC_SS_SCL_HCNT", m_regs->ss_scl_hcnt);
            ss << LOGREG("IC_SS_SCL_LCNT", m_regs->ss_scl_lcnt);
            ss << LOGREG("IC_FS_SCL_HCNT", m_regs->fs_scl_hcnt);
            ss << LOGREG("IC_FS_SCL_LCNT", m_regs->fs_scl_lcnt);
            for (int i = 0; i < 2; ++i)
                ss << LOGREG("PAD1", m_regs->_pad1[i]);
            ss << LOGREG("IC_INTR_STAT", m_regs->intr_stat);
            ss << LOGREG("IC_INTR_MASK", m_regs->intr_mask);
            ss << LOGREG("IC_RAW_INTR_STAT", m_regs->raw_intr_stat);
            ss << LOGREG("IC_RX_TL", m_regs->rx_tl);
            ss << LOGREG("IC_TX_TL", m_regs->tx_tl);
            ss << LOGREG("IC_CLR_INTR", m_regs->clr_intr);
            ss << LOGREG("IC_CLR_RX_UNDER", m_regs->clr_rx_under);
            ss << LOGREG("IC_CLR_RX_OVER", m_regs->clr_rx_over);
            ss << LOGREG("IC_CLR_TX_OVER", m_regs->clr_tx_over);
            ss << LOGREG("IC_CLR_RD_REQ", m_regs->clr_rd_req);
            ss << LOGREG("IC_CLR_TX_ABORT", m_regs->clr_tx_abrt);
            ss << LOGREG("IC_CLR_RX_DONE", m_regs->clr_rx_done);
            ss << LOGREG("IC_CLR_ACTIVITY", m_regs->clr_activity);
            ss << LOGREG("IC_CLR_STOP_DET", m_regs->clr_stop_det);
            ss << LOGREG("IC_CLR_START_DET", m_regs->clr_start_det);
            ss << LOGREG("IC_CLR_GEN_CALL", m_regs->clr_gen_call);
            ss << LOGREG("IC_ENABLE", m_regs->enable);
            ss << LOGREG("IC_STATUS", m_regs->status);
            ss << LOGREG("IC_TXFLR", m_regs->txflr);
            ss << LOGREG("IC_RXFLR", m_regs->rxflr);
            ss << LOGREG("IC_SDA_HOLD", m_regs->sda_hold);
            ss << LOGREG("IC_TX_ABRT_SOURCE", m_regs->tx_abrt_source);
            ss << LOGREG("IC_SLV_DATA_NACK_ONLY", m_regs->slv_data_nack_only);
            ss << LOGREG("IC_DMA_CR", m_regs->dma_cr);
            ss << LOGREG("IC_DMA_TDLR", m_regs->dma_tdlr);
            ss << LOGREG("IC_DMA_RDLR", m_regs->dma_rdlr);
            ss << LOGREG("IC_SDA_SETUP", m_regs->sda_setup);
            ss << LOGREG("IC_ACK_GENERAL_CALL", m_regs->ack_general_call);
            ss << LOGREG("IC_ENABLE_STATUS", m_regs->enable_status);
            ss << LOGREG("IC_FS_SPLLEN", m_regs->fs_spklen);
            ss << LOGREG("IC_HS_SPKLEN", m_regs->_pad2);
            ss << LOGREG("IC_CLR_RESTART_DET", m_regs->clr_restart_det);
            ss << LOGREG("IC_SCL_STUCK_AT_LOW_TO", m_regs->SCL_STUCK_AT_LOW_TIMEOUT);
            ss << LOGREG("IC_SDA_STTUCK_AT_LOW_TO", m_regs->IC_SDA_STUCK_AT_LOW_TIMEOUT);
            for (int i = 0; i < 16; ++i)
                ss << LOGREG("PAD3", m_regs->_pad3[i]);
            ss << LOGREG("IC_COMP_PARAM1", m_regs->comp_param_1);
            ss << LOGREG("IC_COMP_VERSION", m_regs->comp_version);
            ss << LOGREG("IC_COMP_TYPE", m_regs->comp_type);
            return ss.str();
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return "";
        
    }
    bool RP1I2C::enable(bool enable)
    {
        CFuncTracer trace("RP1I2C::enable", m_trace, false);
        try
        {
            const uint32_t requestedState = enable ? 1u : 0u;

            m_regs->enable = requestedState;

            const uint64_t timeout = micros() + 10000;

            while ((static_cast<uint32_t>(m_regs->enable_status) & 0x01u)
                != requestedState)
            {
                if (micros() > timeout)
                {
                    trace.Error(
                        "I2C enable timeout: "
                        "requested=%u "
                        "ENABLE=0x%08X "
                        "ENABLE_STATUS=0x%08X "
                        "STATUS=0x%08X "
                        "RAW_INTR_STAT=0x%08X "
                        "TX_ABRT_SOURCE=0x%08X "
                        "TXFLR=%u "
                        "RXFLR=%u",
                        requestedState,
                        static_cast<uint32_t>(m_regs->enable),
                        static_cast<uint32_t>(m_regs->enable_status),
                        static_cast<uint32_t>(m_regs->status),
                        static_cast<uint32_t>(m_regs->raw_intr_stat),
                        static_cast<uint32_t>(m_regs->tx_abrt_source),
                        static_cast<uint32_t>(m_regs->txflr),
                        static_cast<uint32_t>(m_regs->rxflr));

                    return false;
                }
            }

            return true;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }

    uint64_t RP1I2C::micros()
    {
        CFuncTracer trace("RP1I2C::micros", m_trace, false);
        try
        {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
            uint64_t us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
            return us;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurredc : %s", e.what());
        }
        return 0;
    }
    int32_t RP1I2C::handleAbort(bool timeout, int32_t abortreason)
    {
        CFuncTracer trace("RP1I2C::handleAbort", m_trace, false);
        try
        {
            trace.Error("I2C abort: timeout=%d reason=0x%08X", timeout ? 1 : 0, abortreason);
            if (timeout)
                return 1 << 31 | 1 << 30;  // bit 30 set for timeout
            return abortreason | 1 << 31;
        }
        catch(const std::exception& e)
        {
            trace.Error("exception occurred : %s", e.what());
        }
        return -1;
    }
    size_t RP1I2C::getWriteAvailable()
    {
        CFuncTracer trace("RP1I2C::getWriteAvailable", m_trace, false);
        try
        {
            return constTxBufferDepth - m_regs->txflr;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return 0;
    }
    size_t RP1I2C::getReadAvailable()
    {
        CFuncTracer trace("RP1I2C::getReadAvailable", m_trace, false);
        try
        {
            return m_regs->rxflr;
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return 0;
    }
    int RP1I2C::writeBlockingInternal(uint8_t addr, const uint8_t *src, size_t len, bool nonstop, uint32_t timeoutPerCharUs)
    {
        CFuncTracer trace("RP1I2C::writeBlockingInternal", m_trace, false);
        try
        {
            if (src == nullptr || len == 0)
                return 0;

            //
            // Configure target.
            //
            if (!enable(false))
            {
                trace.Error("Failed to disable I2C controller");
                return -1;
            }

            m_regs->tar = addr;

            //
            // Clear stale status from previous transaction.
            //
            (void)m_regs->clr_tx_abrt;
            (void)m_regs->clr_stop_det;

            if (!enable(true))
            {
                trace.Error("Failed to enable I2C controller");
                return -1;
            }

            size_t bytesQueued = 0;

            const uint64_t timeoutEnd = micros() + static_cast<uint64_t>(timeoutPerCharUs) * static_cast<uint64_t>(len);

            //
            // Queue all write bytes.
            //
            while (bytesQueued < len)
            {
                //
                // Check for abort.
                //
                if (m_regs->raw_intr_stat & RAW_INTR_STAT_TX_ABRT)
                {
                    const uint32_t abortReason = static_cast<uint32_t>( m_regs->tx_abrt_source);
                    trace.Error("I2C write aborted: addr=0x%02X reason=0x%08X queued=%zu/%zu",
                        addr,
                        abortReason,
                        bytesQueued,
                        len);

                    (void)m_regs->clr_tx_abrt;

                    m_resetOnNext = false;

                    return handleAbort( false, abortReason);
                }

                //
                // Queue another byte when TX FIFO has room.
                //
                if (getWriteAvailable() > 0)
                {
                    const bool first = (bytesQueued == 0);
                    const bool last = (bytesQueued == len - 1);

                    uint32_t command = static_cast<uint32_t>( src[bytesQueued]);

                    //
                    // Support old nonstop/restart mechanism.
                    //
                    if (first && m_resetOnNext)
                        command |= IC_DATA_CMD_RESTART;

                    //
                    // Normal write ends with STOP.
                    //
                    if (last && !nonstop)
                        command |= IC_DATA_CMD_STOP;

                    m_regs->data_cmd = command;

                    ++bytesQueued;
                }

                if (micros() > timeoutEnd)
                {
                    trace.Error("I2C write timeout: addr=0x%02X queued=%zu/%zu",
                        addr,
                        bytesQueued,
                        len);

                    m_resetOnNext = false;

                    return handleAbort(true, static_cast<uint32_t>(m_regs->tx_abrt_source));
                }
            }

            //
            // If the transaction ends with STOP, do not return
            // until STOP has actually completed on the bus.
            //
            if (!nonstop)
            {
                while (!(m_regs->raw_intr_stat & RAW_INTR_STAT_STOP_DET))
                {
                    if (m_regs->raw_intr_stat & RAW_INTR_STAT_TX_ABRT)
                    {
                        const uint32_t abortReason =static_cast<uint32_t>(m_regs->tx_abrt_source);

                        trace.Error("I2C write aborted while waiting for STOP: addr=0x%02X reason=0x%08X",
                            addr,
                            abortReason);

                        (void)m_regs->clr_tx_abrt;

                        m_resetOnNext = false;

                        return handleAbort(false,abortReason);
                    }

                    if (micros() > timeoutEnd)
                    {
                        trace.Error("I2C write timeout waiting for STOP: addr=0x%02X",
                            addr);

                        m_resetOnNext = false;

                        return handleAbort(true, static_cast<uint32_t>(m_regs->tx_abrt_source));
                    }
                }

                //
                // Acknowledge/clear STOP_DET.
                //
                (void)m_regs->clr_stop_det;

                m_resetOnNext = false;
            }
            else
            {
                //
                // Only needed for legacy separate
                // write + read operation.
                //
                m_resetOnNext = true;
            }

            return static_cast<int>(bytesQueued);
        }
        catch (const std::exception& e)
        {
            trace.Error("Exception occurred : %s",e.what());
        }

        m_resetOnNext = false;
        return -1;
    }
    int RP1I2C::readBlockingInternal(uint8_t addr, uint8_t* dst, size_t len, bool nonStop, uint32_t timeoutPerCharUs)
    {
        CFuncTracer trace("RP1I2C::readBlockingInternal", m_trace, false);

        try
        {
            if (dst == nullptr || len == 0)
                return 0;

            enable(false);
            m_regs->tar = addr;
            enable(true);

            size_t readRequests = 0;
            size_t bytesReceived = 0;

            bool abort = false;
            bool timeout = false;
            uint32_t abortReason = 0;

            const uint64_t timeoutEnd =
                micros() + static_cast<uint64_t>(timeoutPerCharUs) * len;

            while (bytesReceived < len)
            {
                //
                // Queue READ requests into the TX FIFO.
                //
                while (readRequests < len && getWriteAvailable() > 0)
                {
                    const bool first = (readRequests == 0);
                    const bool last  = (readRequests == len - 1);

                    uint32_t command = IC_DATA_CMD_CMD_READ;

                    if (first && m_resetOnNext)
                        command |= IC_DATA_CMD_RESTART;

                    if (last && !nonStop)
                        command |= IC_DATA_CMD_STOP;

                    m_regs->data_cmd = command;

                    ++readRequests;
                }

                //
                // Check for transaction abort.
                //
                if (m_regs->raw_intr_stat & RAW_INTR_STAT_TX_ABRT)
                {
                    abortReason =
                        static_cast<uint32_t>(m_regs->tx_abrt_source);

                    (void)m_regs->clr_tx_abrt;

                    abort = true;
                    break;
                }

                //
                // Drain all bytes currently available from RX FIFO.
                //
                while (bytesReceived < len && getReadAvailable() > 0)
                {
                    dst[bytesReceived] =
                        static_cast<uint8_t>(m_regs->data_cmd & 0xFF);

                    ++bytesReceived;
                }

                if (micros() > timeoutEnd)
                {
                    timeout = true;
                    abort = true;
                    break;
                }
            }

            m_resetOnNext = nonStop;

            if (abort)
                return handleAbort(timeout, abortReason);

            return static_cast<int>(bytesReceived);
        }
        catch (const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }

        return 0;
    }
    bool RP1I2C::probeQuickWriteAddressOnly(uint8_t addr7, uint32_t timeoutUs)
    {
        CFuncTracer trace("RP1I2C::probeQuickWriteAddressOnly", m_trace, false);
        try
        {
            // Preconditions:
            // - controller in master mode
            // - addr7 is 7-bit (0x00..0x7F), typically scan 0x03..0x77
            // - IC_SMBUS parameter must be 1 for this to actually work
            if (m_baudrate == 0)
            {
                trace.Error("need to initialize the i2c before you can read");
                return false;
            }

            // Program TAR requires disabled in many configs; your code already does this pattern.
            if (!enable(false))
                return false;

            // Set target address + "special" + "SMBus quick"
            // NOTE: SMBUS_QUICK_CMD is only meaningful when SPECIAL=1 (per your doc).
            m_regs->tar = (uint32_t(addr7) & 0x7Fu)
                            | IC_TAR_SPECIAL
                            | IC_TAR_SMBUS_QUICK_CMD;

            // Clear any stale abort/stop conditions
            (void)m_regs->clr_tx_abrt;
            (void)m_regs->clr_stop_det;

           /* trace.Info(
                    "Probe 0x%02X: TAR=0x%08X",
                    addr7,
                    static_cast<uint32_t>(m_regs->tar));
*/
            if (!enable(true))
                return false;

            // Wait for TX FIFO space
            uint64_t tEnd = micros() + timeoutUs;
            while (!getWriteAvailable())
            {
                if (micros() > tEnd)
                    return false;
            }

            // Issue one QUICK "write" command.
            // For SMBus Quick, DAT[7:0] is not used as a data byte in the normal sense.
            // CMD must be 0 for "quick write". STOP ends the transfer.
            m_regs->data_cmd = 0u /* CMD=0 write */
                         | IC_DATA_CMD_STOP;

            // Wait for either STOP (success) or TX_ABRT (NACK/error)
            for (;;)
            {
                uint32_t ris = m_regs->raw_intr_stat;
                const uint32_t abort =
                static_cast<uint32_t>(m_regs->tx_abrt_source);

  /*              trace.Info(
                    "Probe 0x%02X: RAW=0x%08X STATUS=0x%08X "
                    "ABRT=0x%08X TXFLR=%d",
                    addr7,
                    ris,
                    static_cast<uint32_t>(m_regs->status),
                    abort,
                    m_regs->txflr);
*/
                if (ris & RAW_INTR_STAT_TX_ABRT)
                {
                    uint32_t reason = m_regs->tx_abrt_source;
                    (void)m_regs->clr_tx_abrt;
                    // Optional: log 'reason' to distinguish addr-nack vs other errors
                    (void)reason;
/*
                    trace.Info(
                        "Probe 0x%02X -> NACK, ABRT=0x%08X",
                        addr7,
                        abort);
  */
                   return false;
                }

                if (ris & RAW_INTR_STAT_STOP_DET)
                {
                    (void)m_regs->clr_stop_det;

  /*                   trace.Info(
                        "Probe 0x%02X -> ACK",
                        addr7);
*/
                    return true;
                }

                if (micros() > tEnd)
                {
                     trace.Info(
                        "Probe 0x%02X -> TIMEOUT",
                        addr7);
                    return false;
                }
            }
        }
        catch(const std::exception& e)
        {
            trace.Error("Exception occurred : %s", e.what());
        }
        return false;
    }
    
    uint32_t RP1I2C::getComponentVersion()
    {
        return m_regs->comp_version;
    }
    uint32_t RP1I2C::getComponentType()
    {
        return m_regs->comp_type;
    }
    uint32_t RP1I2C::getInfo()
    {
        return m_regs->comp_param_1;
    }
    std::string RP1I2C::getInfoString(uint32_t info)
    {
        const uint32_t txBufferDepth    = ((info >> 16) & 0xFF) + 1;
        const uint32_t rxBufferDepth    = ((info >> 8)  & 0xFF) + 1;
        const bool addEncodedParams     = (info & (1u << 7)) != 0;
        const bool hasDma               = (info & (1u << 6)) != 0;
        const bool intrIo               = (info & (1u << 5)) != 0;
        const bool hcCountValues        = (info & (1u << 4)) != 0;
        const uint32_t maxSpeedMode     = (info >> 2) & 0x03;
        const uint32_t apbDataWidth     = info & 0x03;

        auto speedModeToString = [](uint32_t value)
        {
            switch (value)
            {
            case 1: return "Standard";
            case 2: return "Fast";
            case 3: return "High";
            default: return "Reserved/Unknown";
            }
        };

        auto apbDataWidthToString = [](uint32_t value)
        {
            switch (value)
            {
            case 0: return "8 bit";
            case 1: return "16 bit";
            case 2: return "32 bit";
            default: return "Reserved/Unknown";
            }
        };

        return std::format(
            "IC_COMP_PARAM_1 : 0x{:08X}\n"
            "  TX FIFO depth       : {}\n"
            "  RX FIFO depth       : {}\n"
            "  Encoded parameters  : {}\n"
            "  DMA                 : {}\n"
            "  Interrupt I/O       : {}\n"
            "  HC count values     : {}\n"
            "  Maximum speed mode  : {} ({})\n"
            "  APB data width      : {} ({})",
            info,
            txBufferDepth,
            rxBufferDepth,
            addEncodedParams ? "Yes" : "No",
            hasDma ? "Yes" : "No",
            intrIo ? "Yes" : "No",
            hcCountValues ? "Yes" : "No",
            maxSpeedMode,
            speedModeToString(maxSpeedMode),
            apbDataWidth,
            apbDataWidthToString(apbDataWidth));
    }
}