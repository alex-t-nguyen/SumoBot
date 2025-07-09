#include "drivers/adc.h"
#include "common/assert_handler.h"
#include "common/defines.h"
#include "common/trace.h"
#include "drivers/io.h"
#include <msp430.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Strategy:
 * Setup ADC to sample a sequence of channels include the channels of interest.
 * Interrupt after the sequence has been sampled to cache the sampled values
 * and start a new round of sampling. Let the caller retrieve the latest
 * values from the cache. Use DMA (DTC) and slow clock (ACLK) to reduce CPU
 * involved. Using a slower clock means less interrupts, so less CPU
 * involvement.
 */
static const io_signal_enum *adc_pins;
static uint8_t adc_pin_cnt;
static bool initialized = false;
static volatile adc_channel_values_t
    adc_dma_buffer; // Array of uint16_6 of size ADC_CHANNEL_COUNT (8)

/**
 * Enable ADC12 and start sampling and conversion
 */
static void adc_enable_and_start_conversion(void) {
    // Set ENC to enable conversions with ADC12
    // Set ADC12SC to trigger rising edge sampling start with ADC12
    ADC12CTL0 |= ADC12ENC | ADC12SC;
}

void adc_init(void) {
    ASSERT(!initialized);
    adc_pins = get_io_adc_pins(&adc_pin_cnt);

    /**
     * ADC12CTL0
     *
     * ADC12MSC: Do conversion of other channels in sequence after the previous
     * conversion is finished
     */
    ADC12CTL0 = ADC12ON + ADC12MSC + ADC12SHT0_4 + ADC12SHT1_4;

    /**
     * ADC12CTL1
     * ARC12CSTARTADDx: 1 (Use ADC12MEM1 as the conversion start address, and
     * the conversion sequence ends at ADC12MCTL4 (connected to ADCMEM4 and
     * sequence ends because set ADC12EOS in ADC12MCTL4) ADC12DIVx: ADC12 clock
     * source divider ADC12SSELx: ADC12 clock source selector (1: ACLK)
     * ADC12SHSx: Sample and hold source select (0: ADC12SC bit). Conversions
     * for the sequence of channels are triggered by a rising edge of this bit.
     * This acts as the SHI signal, which will trigger sampling on rising edge.
     * Will be setting manually through interrupts. ADC12CONSEQ: Conversion
     * sequence mode as a sequence of channels. The ADC12 interrupt flag for the
     * last ADC12MEMx in the sequence (determined by ADC12EOS) can trigger a DMA
     * transfer ADC12ISSH: 0 (Don't invert selected ADC12SC signal (SHI))
     * ADC12SHP: 0 (Use actual ADC12SC signal as SHI signal as the SAMPCON and
     * rising and falling edge of this signal starts and stop sampling time) 1
     * (Use sampling timer with SHI signal as the SAMPCON, where rising edge of
     * SHI signal starts sampling, and timer clock cycles stop sampling instead
     * of falling edge of SHI signal)
     */
    ADC12CTL1 = ADC12CSTARTADD_1 + ADC12DIV_7 + ADC12SSEL_1 + ADC12SHS_0 +
                ADC12CONSEQ_1 + ADC12SHP;

    /**
     * ADC12CTL2
     * ADC12RESx: ADC Resolution (2 = 12 bits)
     * ADC12DF: Data read-back format (0 = Binary unsigned)
     * ADC12SR: Sampling rate (0 = 200ksps)
     *
     */
    ADC12CTL2 = ADC12RES_2;

    /**
     * ADC12MCTLx
     * ADC12EOS: Current register indicate end of sampling sequence
     * ADC12INCHx: Select input channel for the corresponding ADC12MEMx
     * ADC12SREFx: Select the reference voltage (0: AVCC, AVSS)
     */
    ADC12MCTL1 = ADC12INCH_1 + ADC12SREF_0;
    ADC12MCTL2 = ADC12INCH_2 + ADC12SREF_0;
    ADC12MCTL3 = ADC12INCH_3 + ADC12SREF_0;
    ADC12MCTL4 =
        ADC12EOS + ADC12INCH_4 +
        ADC12SREF_0; // Setting ADC12EOS means this input channel will represent
                     // the end of a sequence. Only need to sample up to channel
                     // 4, even though there are 15 channels.

    /**
     * ADC12IEx
     * Enable interrupts for ADC12IFGx
     * Using Analog-In A1,A2,A3,A4
     */
    // ADC12IE = ADC12IE1 + ADC12IE2 + ADC12IE3;

    /**
     * Refernce voltage
     */
    // I think the reference voltage is automatically enabled on request by
    // ADC12 and disabled otherwise, so I don't think we need to set any bits in
    // REFCTL0 register to turn on the reference voltage

    /**
     * DMA
     * Use DMA0 (channel 0 out of 0/1/2) to transfer ADC conversions (channel
     * 3-7 not implemented on MSP430F5529) Select DMA to act as trigger for
     * transferring the block of ADC converted data when the ADC12MEM4 IFG is
     * set
     */
    DMA0CTL &= ~DMAEN; // Disable DMA0 while configuring
    DMACTL0 |= DMA0TSEL__ADC12IFG;

    /**
     * DMA0CTL
     * DMADT: Transfer mode
     *          0: Single transfer (Each ADC input transfer requires an IFG
     * trigger and DMAEN gets reset after each transfer) 1: Block transfer
     * (Transfer block of ADC inputs with 1 IFG trigger and DMAEN gets reset
     * after each transfer)
     *
     * DMADSTINCR: DMA destination address increment
     * DMASRCINCR: DMA source address increment
     * DMADSTBYTE: DMA destination byte (0: Word, 1: Byte)
     * DMASRCBYTE: DMA source byte (0: Word, 1: Byte)
     * DMALEVEL: 0 (Edge sensitive (rising edge))
     * DMAEN: 1 (Enable DMA)
     */
    DMA0CTL = DMADT_1 | DMADSTINCR_3 | DMASRCINCR_3 | DMAIE;
    DMA0SA = (uint16_t)&ADC12MEM1;
    DMA0DA = (uint16_t)adc_dma_buffer; // Destination array in RAM to store
                                       // values transferred by DMA
    DMA0SZ = (uint16_t)4; // Number of bytes/words to transfer (decrements after
                          // each transfer until reaching 0)
    DMA0CTL |= DMAEN;     // Enable DMA0 after done configuring
                          //
    adc_enable_and_start_conversion();

    initialized = true;
}

INTERRUPT_FUNCTION(DMA_VECTOR) isr_dma(void) {
    switch (__even_in_range(DMAIV, DMAIV_DMA2IFG)) {
    case DMAIV_NONE:
        break;
    case DMAIV_DMA0IFG:
        ADC12CTL0 &= ~ADC12SC; // Need to manually reset ADC12SC bit to trigger
                               // another ADC sample and conversion
        DMA0CTL |= DMAEN; // Need to re-enable DMA because this bit is cleared
                          // after every transfer because using DMADT_1
        adc_enable_and_start_conversion(); // Start an ADC sample and conversion
                                           // after transferring previous
                                           // conversion through DMA
        break;
    case DMAIV_DMA1IFG:
        break;
    case DMAIV_DMA2IFG:
        break;
    default:
        break;
    }
}

void adc_get_channel_values(adc_channel_values_t buffer) {

    __disable_interrupt();
    uint8_t i;
    for (i = 0; i < ADC_CHANNEL_COUNT; i++) {
        buffer[i] = adc_dma_buffer[i];
    }
    __enable_interrupt();
}
