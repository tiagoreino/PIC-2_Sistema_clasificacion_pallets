// Prueba de PWM en D11 con dos velocidades - Arduino UNO (ATmega328P) en C
// Ciclo: arranque (VEL_ARRANQUE durante ARRANQUE_MS) -> velocidad fija (VEL_FIJA)
//        durante MARCHA_MS -> parado PARADO_MS, y se repite. Muestra cada paso por UART (9600).
// D13 (IN1) queda en alto y D6 (IN2) en bajo, asi el motor gira adelante si esta conectado.

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define VEL_ARRANQUE    115    // PWM 0-255 del arranque
#define ARRANQUE_MS     100     // duracion del arranque
#define VEL_FIJA        80     // PWM 0-255 despues del arranque
#define MARCHA_MS       20000    // tiempo a velocidad fija
#define PARADO_MS       500    // tiempo parado antes de repetir

#define PIN_PWM     PB3     // D11 (OC2A)
#define PIN_IN1     PB5     // D13
#define PIN_IN2     PD6     // D6

static void uart_init(void)
{
    UBRR0 = 103;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_texto(const char *s)
{
    while (*s) {
        while (!(UCSR0A & (1 << UDRE0))) {}
        UDR0 = *s++;
    }
}

static void uart_numero(uint8_t n)
{
    char buf[4];
    buf[0] = '0' + n / 100;
    buf[1] = '0' + (n / 10) % 10;
    buf[2] = '0' + n % 10;
    buf[3] = '\0';
    uart_texto(buf);
}

static void pwm(uint8_t valor)
{
    if (valor == 0) {
        TCCR2A &= (uint8_t)~(1 << COM2A1);
        PORTB &= (uint8_t)~(1 << PIN_PWM);
    } else {
        OCR2A = valor;
        TCCR2A |= (1 << COM2A1);
    }

    uart_texto("PWM D11 = ");
    uart_numero(valor);
    uart_texto("\r\n");
}

int main(void)
{
    PORTB &= (uint8_t)~(1 << PIN_PWM);
    DDRB |= (1 << PIN_PWM) | (1 << PIN_IN1);
    PORTB |= (1 << PIN_IN1);
    DDRD |= (1 << PIN_IN2);
    PORTD &= (uint8_t)~(1 << PIN_IN2);

    // Timer2: Fast PWM en OC2A (D11), ~976 Hz
    TCCR2A = (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS22);

    uart_init();
    uart_texto("Prueba PWM en D11 (arranque + velocidad fija)\r\n");

    while (1) {
        uart_texto("Arranque: ");
        pwm(VEL_ARRANQUE);
        _delay_ms(ARRANQUE_MS);

        uart_texto("Fija: ");
        pwm(VEL_FIJA);
        _delay_ms(MARCHA_MS);

        uart_texto("Parado: ");
        pwm(0);
        _delay_ms(PARADO_MS);
    }
}