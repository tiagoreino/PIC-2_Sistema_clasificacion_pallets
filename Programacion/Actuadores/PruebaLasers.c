// Prueba de los 3 láseres con LDR - Arduino UNO (ATmega328P) en C
// Muestra por UART (9600) el valor del ADC de A0, A1 y A2 (0-1023) cada 250 ms.
// El LED de la placa (D13) se prende si alguno está por debajo del umbral (láser cortado).

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define UMBRAL_LDR      600     // por debajo = láser cortado (ajustar con esta prueba)
#define LED             PB5     // D13

static void uart_init(void)
{
    UBRR0 = 103;                            // 9600 baudios a 16 MHz
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8N1
}

static void uart_char(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) {}
    UDR0 = c;
}

static void uart_texto(const char *s)
{
    while (*s) uart_char(*s++);
}

static void uart_numero(uint16_t n)
{
    char buf[5];
    for (int8_t i = 3; i >= 0; i--) {
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    buf[4] = '\0';
    uart_texto(buf);
}

static void adc_init(void)
{
    ADMUX = (1 << REFS0);                                   // referencia AVcc (5 V)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // prescaler 128
    DIDR0 = (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D);     // sin buffer digital en A0-A2
}

static uint16_t adc_leer(uint8_t canal)
{
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {}
    return ADC;
}

int main(void)
{
    // A0-A2 como entradas SIN pull-up (la LDR tiene su propia resistencia)
    DDRC &= (uint8_t)~((1 << PC0) | (1 << PC1) | (1 << PC2));
    PORTC &= (uint8_t)~((1 << PC0) | (1 << PC1) | (1 << PC2));

    DDRB |= (1 << LED);

    uart_init();
    adc_init();
    _delay_ms(100);
    uart_texto("Prueba de laseres con LDR (umbral ");
    uart_numero(UMBRAL_LDR);
    uart_texto(")\r\n");

    while (1) {
        uint16_t camara  = adc_leer(0);
        uint16_t pre     = adc_leer(1);
        uint16_t stacker = adc_leer(2);

        uart_texto("Camara(A0)=");
        uart_numero(camara);
        uart_texto(camara < UMBRAL_LDR ? " CORTADO" : " ok     ");
        uart_texto("  Pre(A1)=");
        uart_numero(pre);
        uart_texto(pre < UMBRAL_LDR ? " CORTADO" : " ok     ");
        uart_texto("  Stacker(A2)=");
        uart_numero(stacker);
        uart_texto(stacker < UMBRAL_LDR ? " CORTADO" : " ok");
        uart_texto("\r\n");

        if (camara < UMBRAL_LDR || pre < UMBRAL_LDR || stacker < UMBRAL_LDR)
            PORTB |= (1 << LED);
        else
            PORTB &= (uint8_t)~(1 << LED);

        _delay_ms(250);
    }
}