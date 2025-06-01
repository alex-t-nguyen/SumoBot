
#include <stdint.h>

#define ADC_CHANNEL_COUNT (4U) // There are 8 channels, but only using 4 (A1-4)

typedef uint16_t adc_channel_values_t[ADC_CHANNEL_COUNT];

void adc_init(void);
void adc_get_channel_values(adc_channel_values_t values);
