// Clasificadora de pallets - Arduino UNO (ATmega328P, 16 MHz) en C
// Basado en PruebaPistonPIC: rele del piston activo en bajo y espera de fin de carrera.

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdint.h>
#include <stdbool.h>

// ---------------- Configuracion ----------------
#define VEL_NORMAL        80        // PWM 0-255 de la cinta
#define VEL_ARRANQUE      115       // PWM del empujon al arrancar desde parado
#define ARRANQUE_MS       100UL     // duracion del empujon, despues queda la velocidad fija
#define UMBRAL_LDR        900       // ADC 0-1023: por debajo = laser cortado (ajustar con la prueba)
#define EMERGENCIA_NC     0         // 1 si el boton de emergencia es normalmente cerrado
#define FIN_PISTON_NC     0         // 1 = fin de carrera del piston cableado en NC a GND
#ifndef DEBUG_UART
#define DEBUG_UART        1         // 1 = muestra los cambios de estado por el monitor serie (9600)
#endif
#ifndef BUZZER_PASIVO
#define BUZZER_PASIVO     0         // 1 si el buzzer es pasivo (se le genera un tono de 500 Hz)
#endif

#define ANTIRREBOTE_MS    40UL
#define ANTIRREBOTE_LDR_MS 5UL     // las LDR responden rapido: el pallet pasa en poco tiempo
#define BLANQUEO_RELE_MS  300UL     // tras conmutar el rele se ignoran los botones (ruido)
#define PULSO_FOTO_MS     100UL
#define RESULTADO_MS      8000UL    // espera maxima del 01/10 de la ESP32
#define REINTENTOS_FOTO   2
#define TRAYECTO_MS       15000UL   // maximo de cinta entre laseres
#define PISTON_MIN_MS     3000UL    // igual que ESPERA_MS de PruebaPistonPIC
#define PISTON_MAX_MS     10000UL   // si no llega el fin de carrera, error
#define PISTON_BAJADA_MS  2000UL    // espera despues de apagar el rele
#ifndef USAR_FIN_PISTON
#define USAR_FIN_PISTON   1         // 0 = sin fin de carrera: el piston se apaga solo a los PISTON_MIN_MS
#endif

// ---------------- Pines ----------------
// ESP32 (sin pull-up)
#define ESP_BAJO        PD2     // D2  <- GPIO2
#define ESP_ALTO        PD7     // D7  <- GPIO12

// Piston (igual que PruebaPistonPIC)
#define RELE_PISTON     PB0     // D8, activo en bajo
#define FIN_PISTON      PC2     // A2 (ver FIN_PISTON_NC)

// Puente H (cinta)
#define CINTA_EN        PB3     // D11 (OC2A, PWM) -> ENA
#define CINTA_IN1       PB5     // D13 -> IN1
#define CINTA_IN2       PD6     // D6  -> IN2

// Laseres con LDR (entradas analogicas)
#define LASER_CAMARA    PC0     // A0 / ADC0
#define LASER_STACKER   PC1     // A1 / ADC1, debajo del stacker

// Botones (a GND, pull-up interno)
#define BTN_AVANZAR     PD4     // D4
#define BTN_PARAR       PD5     // D5
#define BTN_RETROCEDER  PB1     // D9
#define BTN_SUBIR       PB2     // D10
#define BTN_BAJAR       PB4     // D12
#define BTN_FOTO        PC3     // A3
#define BTN_EMERGENCIA  PC4     // A4

// Pedido de foto a la ESP32
#define FOTO            PC5     // A5 -> divisor -> GPIO3

// Buzzer en D0 (RX libre: la UART solo usa TX), a traves de un transistor
#define BUZZER          PD0

#define COD_NADA        0
#define COD_ACEPTADO    1
#define COD_RECHAZADO   2
#define COD_MANUAL      3

// ---------------- Tiempo (Timer0, igual que PruebaPistonPIC) ----------------
static volatile uint32_t contador_ms = 0;

ISR(TIMER0_COMPA_vect)
{
    contador_ms++;
}

static uint32_t tiempo_ms(void)
{
    uint32_t tiempo;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        tiempo = contador_ms;
    }

    return tiempo;
}

static void configurar_temporizador(void)
{
    TCCR0A = (1 << WGM01);              // CTC
    TCCR0B = 0;
    TCNT0 = 0;
    OCR0A = 249;                        // 16 MHz / 64 / 250 = 1 ms
    TIFR0 = (1 << OCF0A);
    TIMSK0 = (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);

    // Timer2: Fast PWM en OC2A (D11) para la cinta, ~976 Hz
    TCCR2A = (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS22);
    OCR2A = 0;
}

// ---------------- ADC para las LDR ----------------
static uint16_t adc_valor[2] = { 1023, 1023 };
static uint8_t adc_canal = 0;

static void configurar_adc(void)
{
    ADMUX = (1 << REFS0);                                   // referencia AVcc
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0 = (1 << ADC0D) | (1 << ADC1D);
}

static uint16_t adc_leer(uint8_t canal)
{
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {}
    return ADC;
}

// Un canal por ms para no frenar el lazo
static void adc_tick(void)
{
    adc_valor[adc_canal] = adc_leer(adc_canal);
    if (++adc_canal >= 2) adc_canal = 0;
}

// ---------------- Entradas con antirrebote ----------------
typedef struct {
    volatile uint8_t *pin;
    uint8_t bit;
    uint8_t nivel_activo;
    uint8_t estado;         // 1 = activo
    uint8_t flanco;         // 1 solo en el ms en que paso a activo
    uint8_t ultima;
    uint32_t ultimo_cambio;
} entrada_t;

enum {
    E_ESP_BAJO, E_ESP_ALTO, E_FIN_PISTON,
    E_LASER_CAMARA, E_LASER_STACKER,
    E_AVANZAR, E_PARAR, E_RETROCEDER, E_SUBIR, E_BAJAR, E_FOTO,
    E_EMERGENCIA,
    N_ENTRADAS
};

static entrada_t entradas[N_ENTRADAS] = {
    [E_ESP_BAJO]      = { &PIND, ESP_BAJO,       1 },
    [E_ESP_ALTO]      = { &PIND, ESP_ALTO,       1 },
    [E_FIN_PISTON]    = { &PINC, FIN_PISTON,     FIN_PISTON_NC ? 1 : 0 },
    [E_LASER_CAMARA]  = { &PINC, LASER_CAMARA,   0 },
    [E_LASER_STACKER] = { &PINC, LASER_STACKER,  0 },
    [E_AVANZAR]       = { &PIND, BTN_AVANZAR,    0 },
    [E_PARAR]         = { &PIND, BTN_PARAR,      0 },
    [E_RETROCEDER]    = { &PINB, BTN_RETROCEDER, 0 },
    [E_SUBIR]         = { &PINB, BTN_SUBIR,      0 },
    [E_BAJAR]         = { &PINB, BTN_BAJAR,      0 },
    [E_FOTO]          = { &PINC, BTN_FOTO,       0 },
    [E_EMERGENCIA]    = { &PINC, BTN_EMERGENCIA, EMERGENCIA_NC ? 1 : 0 },
};

static uint8_t leer_crudo(uint8_t i)
{
    entrada_t *e = &entradas[i];

    // Las LDR se leen por ADC: activo = laser cortado
    if (i == E_LASER_CAMARA)  return adc_valor[0] < UMBRAL_LDR;
    if (i == E_LASER_STACKER) return adc_valor[1] < UMBRAL_LDR;

    return ((*e->pin >> e->bit) & 1) == e->nivel_activo;
}

static void entradas_iniciar(uint32_t ahora)
{
    for (uint8_t i = 0; i < N_ENTRADAS; i++) {
        entradas[i].estado = entradas[i].ultima = leer_crudo(i);
        entradas[i].ultimo_cambio = ahora;
    }
}

// Momento del ultimo cambio del rele: el ruido al conmutar dispara botones falsos
static uint32_t rele_cambio = 0xFFFFFFFFUL - BLANQUEO_RELE_MS;

static bool es_boton(uint8_t i)
{
    return i == E_FIN_PISTON || i == E_AVANZAR || i == E_PARAR || i == E_RETROCEDER ||
           i == E_SUBIR || i == E_BAJAR || i == E_FOTO;
}

static void entradas_actualizar(uint32_t ahora)
{
    bool blanqueo = (uint32_t)(ahora - rele_cambio) < BLANQUEO_RELE_MS;

    for (uint8_t i = 0; i < N_ENTRADAS; i++) {
        entrada_t *e = &entradas[i];
        uint8_t lectura = leer_crudo(i);

        e->flanco = 0;

        if (blanqueo && es_boton(i)) {
            e->ultima = e->estado;
            e->ultimo_cambio = ahora;
            continue;
        }

        if (lectura != e->ultima) {
            e->ultima = lectura;
            e->ultimo_cambio = ahora;
        }

        uint32_t espera = (i == E_LASER_CAMARA || i == E_LASER_STACKER)
                          ? ANTIRREBOTE_LDR_MS : ANTIRREBOTE_MS;

        if ((uint32_t)(ahora - e->ultimo_cambio) >= espera &&
            lectura != e->estado) {
            e->estado = lectura;
            e->flanco = lectura;
        }
    }
}

#define ACTIVA(n)  (entradas[n].estado)
#define FLANCO(n)  (entradas[n].flanco)

static uint8_t codigo_esp(void)
{
    return (ACTIVA(E_ESP_ALTO) << 1) | ACTIVA(E_ESP_BAJO);
}

// ---------------- Cinta ----------------
typedef enum { CINTA_PARADA, CINTA_ADELANTE, CINTA_ATRAS } dir_cinta_t;

static dir_cinta_t cinta_dir = CINTA_PARADA;
static uint8_t cinta_vel_fija = 0;
static bool cinta_arrancando = false;
static uint32_t cinta_inicio = 0;

static void cinta(dir_cinta_t dir, uint8_t velocidad)
{
    PORTB &= (uint8_t)~(1 << CINTA_IN1);
    PORTD &= (uint8_t)~(1 << CINTA_IN2);

    if (dir == CINTA_PARADA || velocidad == 0) {
        TCCR2A &= (uint8_t)~(1 << COM2A1);
        PORTB &= (uint8_t)~(1 << CINTA_EN);
        cinta_dir = CINTA_PARADA;
        cinta_arrancando = false;
        return;
    }

    if (dir == CINTA_ADELANTE) PORTB |= (1 << CINTA_IN1);
    else                       PORTD |= (1 << CINTA_IN2);

    cinta_vel_fija = velocidad;

    // Si arranca desde parado o cambia de sentido: empujon y despues la velocidad fija
    if (dir != cinta_dir && VEL_ARRANQUE > velocidad) {
        OCR2A = VEL_ARRANQUE;
        cinta_arrancando = true;
        cinta_inicio = tiempo_ms();
    } else if (!cinta_arrancando) {
        OCR2A = velocidad;
    }

    cinta_dir = dir;
    TCCR2A |= (1 << COM2A1);
}

static void cinta_tick(uint32_t ahora)
{
    if (cinta_arrancando && (uint32_t)(ahora - cinta_inicio) >= ARRANQUE_MS) {
        OCR2A = cinta_vel_fija;
        cinta_arrancando = false;
    }
}

// ---------------- Piston ----------------
// Igual que PruebaPistonPIC: rele en bajo, minimo 3 s y se apaga cuando llega al fin de carrera
static bool piston_activo = false;
static bool piston_fallo = false;
static uint32_t piston_inicio = 0;

static void piston_subir(uint32_t ahora)
{
    PORTB &= (uint8_t)~(1 << RELE_PISTON);
    rele_cambio = ahora;
    piston_activo = true;
    piston_fallo = false;
    piston_inicio = ahora;
}

static void piston_bajar(void)
{
    if (!(PORTB & (1 << RELE_PISTON))) rele_cambio = tiempo_ms();
    PORTB |= (1 << RELE_PISTON);
    piston_activo = false;
}

static void piston_tick(uint32_t ahora)
{
    if (!piston_activo) return;

    uint32_t t = ahora - piston_inicio;

#if USAR_FIN_PISTON
    if (t >= PISTON_MIN_MS && ACTIVA(E_FIN_PISTON)) {
#else
    if (t >= PISTON_MIN_MS) {
#endif
        piston_bajar();
    } else if (t >= PISTON_MAX_MS) {
        piston_bajar();
        piston_fallo = true;
    }
}

// ---------------- Pedido de foto (pulso no bloqueante) ----------------
static bool foto_activa = false;
static uint32_t foto_inicio = 0;

static void foto_pedir(uint32_t ahora)
{
    PORTC |= (1 << FOTO);
    foto_activa = true;
    foto_inicio = ahora;
}

static void foto_tick(uint32_t ahora)
{
    if (foto_activa && (uint32_t)(ahora - foto_inicio) >= PULSO_FOTO_MS) {
        PORTC &= (uint8_t)~(1 << FOTO);
        foto_activa = false;
    }
}

// ---------------- Buzzer (pitidos no bloqueantes) ----------------
static uint8_t buzz_pasos = 0;      // cada pitido = 2 pasos (sonando / silencio)
static uint32_t buzz_inicio = 0;

static void pitidos(uint8_t n, uint32_t ahora)
{
    buzz_pasos = n * 2;
    buzz_inicio = ahora;
}

static void buzzer_tick(uint32_t ahora)
{
    if (buzz_pasos && (uint32_t)(ahora - buzz_inicio) >= 150UL) {
        buzz_pasos--;
        buzz_inicio = ahora;
    }

    // Pasos pares = sonando
    if (buzz_pasos && !(buzz_pasos & 1)) {
#if BUZZER_PASIVO
        PORTD ^= (1 << BUZZER);
#else
        PORTD |= (1 << BUZZER);
#endif
    } else {
        PORTD &= (uint8_t)~(1 << BUZZER);
    }
}

static void todo_apagado(void)
{
    cinta(CINTA_PARADA, 0);
    piston_bajar();
}

// ---------------- Debug por UART (D1 TX, 9600) ----------------
#if DEBUG_UART
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
#else
#define uart_init()
#define uart_texto(s) ((void)0)
#endif

// ---------------- Maquina de estados ----------------
typedef enum {
    ST_INICIO,
    ST_BUSCAR_PALLET,       // cinta normal hasta el laser de la camara
    ST_PEDIR_FOTO,          // espera 00 de la ESP32 y pide la foto
    ST_ESPERA_RESULTADO,
    ST_ACEPTADO,            // sigue de largo: espera que pase debajo del stacker
    ST_ACEPTADO_SALIENDO,
    ST_RECHAZADO_STACKER,   // cinta hasta el laser del stacker
    ST_SUBIENDO,
    ST_BAJANDO,
    ST_MANUAL,
    ST_EMERGENCIA,
    ST_ERROR
} estado_t;

static estado_t estado = ST_INICIO;
static uint32_t inicio_estado = 0;
static uint8_t intentos = 0;

static const char *const nombres[] = {
    "INICIO", "BUSCAR_PALLET", "PEDIR_FOTO", "ESPERA_RESULTADO",
    "ACEPTADO", "ACEPTADO_SALIENDO", "RECHAZADO_STACKER",
    "SUBIENDO", "BAJANDO", "MANUAL", "EMERGENCIA", "ERROR"
};

static void cambiar(estado_t nuevo, uint32_t ahora)
{
    if (nuevo != estado) {
        uart_texto("-> ");
        uart_texto(nombres[nuevo]);
        uart_texto("\r\n");
    }
    estado = nuevo;
    inicio_estado = ahora;
}

static void ir_a_error(uint32_t ahora)
{
    todo_apagado();
    pitidos(5, ahora);
    cambiar(ST_ERROR, ahora);
}

#if DEBUG_UART
// Muestra lo que llega de la ESP32 y cada boton/laser que se activa
static void debug_entradas(void)
{
    static const char *const nombres_e[N_ENTRADAS] = {
        "ESP bajo", "ESP alto", "FIN PISTON",
        "LASER CAMARA", "LASER STACKER",
        "AVANZAR", "PARAR", "RETROCEDER", "SUBIR", "BAJAR", "FOTO",
        "EMERGENCIA"
    };
    static uint8_t cod_anterior = 0xFF;
    uint8_t cod = codigo_esp();

    if (cod != cod_anterior) {
        char txt[] = "ESP32 manda: 00\r\n";
        txt[13] = '0' + ((cod >> 1) & 1);
        txt[14] = '0' + (cod & 1);
        uart_texto(txt);
        cod_anterior = cod;
    }

    for (uint8_t i = E_FIN_PISTON; i < N_ENTRADAS; i++) {
        if (FLANCO(i)) {
            uart_texto("Activo: ");
            uart_texto(nombres_e[i]);
            uart_texto("\r\n");
        }
    }
}
#endif

static void ejecutar_secuencia(uint32_t ahora)
{
    uint8_t cod = codigo_esp();
    uint32_t t = ahora - inicio_estado;

    // La emergencia tiene prioridad sobre todo
    if (ACTIVA(E_EMERGENCIA) && estado != ST_EMERGENCIA) {
        todo_apagado();
        pitidos(3, ahora);
        cambiar(ST_EMERGENCIA, ahora);
        return;
    }

    // Objeto extrano: la PC pasa a 11 mientras se espera el resultado -> emergencia
    if (estado == ST_ESPERA_RESULTADO && cod == COD_MANUAL) {
        todo_apagado();
        pitidos(3, ahora);
        cambiar(ST_EMERGENCIA, ahora);
        return;
    }

    // Modo segun la ESP32 (11 = manual)
    if (estado != ST_EMERGENCIA) {
        if (cod == COD_MANUAL && estado != ST_MANUAL) {
            todo_apagado();
            cambiar(ST_MANUAL, ahora);
            return;
        }
        if (cod != COD_MANUAL && estado == ST_MANUAL) {
            todo_apagado();
            cambiar(ST_INICIO, ahora);
            return;
        }
    }

    switch (estado) {
    case ST_INICIO:
        cinta(CINTA_ADELANTE, VEL_NORMAL);
        cambiar(ST_BUSCAR_PALLET, ahora);
        break;

    case ST_BUSCAR_PALLET:
        if (ACTIVA(E_LASER_CAMARA)) {
            cinta(CINTA_PARADA, 0);
            cambiar(ST_PEDIR_FOTO, ahora);
        }
        break;

    case ST_PEDIR_FOTO:
        // Si todavia dura el 01/10 del pallet anterior, se espera el 00
        if (cod == COD_NADA) {
            intentos = 0;
            foto_pedir(ahora);
            uart_texto("Foto pedida\r\n");
            cambiar(ST_ESPERA_RESULTADO, ahora);
        } else if (t >= RESULTADO_MS) {
            ir_a_error(ahora);
        }
        break;

    case ST_ESPERA_RESULTADO:
        if (cod == COD_ACEPTADO) {
            pitidos(1, ahora);
            cinta(CINTA_ADELANTE, VEL_NORMAL);
            cambiar(ST_ACEPTADO, ahora);
        } else if (cod == COD_RECHAZADO) {
            pitidos(2, ahora);
            cinta(CINTA_ADELANTE, VEL_NORMAL);
            cambiar(ST_RECHAZADO_STACKER, ahora);
        } else if (t >= RESULTADO_MS) {
            if (++intentos < REINTENTOS_FOTO) {
                foto_pedir(ahora);
                cambiar(ST_ESPERA_RESULTADO, ahora);
            } else {
                ir_a_error(ahora);
            }
        }
        break;

    case ST_ACEPTADO:
        if (ACTIVA(E_LASER_STACKER)) {
            cambiar(ST_ACEPTADO_SALIENDO, ahora);
        } else if (t >= TRAYECTO_MS) {
            ir_a_error(ahora);
        }
        break;

    case ST_ACEPTADO_SALIENDO:
        // Cuando el pallet termino de pasar debajo del stacker, se busca el proximo
        if (!ACTIVA(E_LASER_STACKER)) {
            cambiar(ST_BUSCAR_PALLET, ahora);
        } else if (t >= TRAYECTO_MS) {
            ir_a_error(ahora);
        }
        break;

    case ST_RECHAZADO_STACKER:
        if (ACTIVA(E_LASER_STACKER)) {
            cinta(CINTA_PARADA, 0);
            piston_subir(ahora);
            cambiar(ST_SUBIENDO, ahora);
        } else if (t >= TRAYECTO_MS) {
            ir_a_error(ahora);
        }
        break;

    case ST_SUBIENDO:
        if (piston_fallo) {
            ir_a_error(ahora);
        } else if (!piston_activo) {
            cambiar(ST_BAJANDO, ahora);
        }
        break;

    case ST_BAJANDO:
        if (t >= PISTON_BAJADA_MS) {
            cinta(CINTA_ADELANTE, VEL_NORMAL);
            cambiar(ST_BUSCAR_PALLET, ahora);
        }
        break;

    case ST_MANUAL:
        // En manual el laser de la camara no pide foto: se usa el boton
        if (FLANCO(E_AVANZAR))    cinta(CINTA_ADELANTE, VEL_NORMAL);
        if (FLANCO(E_RETROCEDER)) cinta(CINTA_ATRAS, VEL_NORMAL);
        if (FLANCO(E_PARAR))      cinta(CINTA_PARADA, 0);
        if (FLANCO(E_SUBIR))      piston_subir(ahora);
        if (FLANCO(E_BAJAR))      piston_bajar();
        if (FLANCO(E_FOTO)) {
            foto_pedir(ahora);
            uart_texto("Foto pedida\r\n");
        }
        break;

    case ST_EMERGENCIA:
        // Sale con la emergencia liberada y apretando PARAR
        if (!ACTIVA(E_EMERGENCIA) && FLANCO(E_PARAR)) {
            cambiar(cod == COD_MANUAL ? ST_MANUAL : ST_INICIO, ahora);
        }
        break;

    case ST_ERROR:
        if (FLANCO(E_PARAR)) {
            cambiar(ST_INICIO, ahora);
        }
        break;
    }
}

// ---------------- Inicializacion ----------------
static void configurar_pines(void)
{
    // Rele del piston: HIGH (apagado) antes de ponerlo como salida
    PORTB |= (1 << RELE_PISTON);
    DDRB |= (1 << RELE_PISTON);

    // Puente H y foto como salidas en bajo
    PORTB &= (uint8_t)~((1 << CINTA_EN) | (1 << CINTA_IN1));
    DDRB |= (1 << CINTA_EN) | (1 << CINTA_IN1);
    PORTD &= (uint8_t)~(1 << CINTA_IN2);
    DDRD |= (1 << CINTA_IN2);
    PORTC &= (uint8_t)~(1 << FOTO);
    DDRC |= (1 << FOTO);
    PORTD &= (uint8_t)~(1 << BUZZER);
    DDRD |= (1 << BUZZER);

    // ESP32: entradas sin pull-up
    DDRD &= (uint8_t)~((1 << ESP_BAJO) | (1 << ESP_ALTO));
    PORTD &= (uint8_t)~((1 << ESP_BAJO) | (1 << ESP_ALTO));

    // Resto de entradas con pull-up
    DDRD &= (uint8_t)~((1 << BTN_AVANZAR) | (1 << BTN_PARAR));
    PORTD |= (1 << BTN_AVANZAR) | (1 << BTN_PARAR);

    DDRB &= (uint8_t)~((1 << BTN_RETROCEDER) | (1 << BTN_SUBIR) | (1 << BTN_BAJAR));
    PORTB |= (1 << BTN_RETROCEDER) | (1 << BTN_SUBIR) | (1 << BTN_BAJAR);

    DDRC &= (uint8_t)~((1 << FIN_PISTON) | (1 << BTN_FOTO) | (1 << BTN_EMERGENCIA));
    PORTC |= (1 << FIN_PISTON) | (1 << BTN_FOTO) | (1 << BTN_EMERGENCIA);

    // LDR: entradas sin pull-up (tienen su resistencia de 10 kohm a GND)
    DDRC &= (uint8_t)~((1 << LASER_CAMARA) | (1 << LASER_STACKER));
    PORTC &= (uint8_t)~((1 << LASER_CAMARA) | (1 << LASER_STACKER));
}

int main(void)
{
    configurar_pines();
    configurar_temporizador();
    configurar_adc();
    uart_init();

    sei();

    uart_texto("Clasificadora iniciada\r\n");

    for (uint8_t i = 0; i < 2; i++) adc_tick();

    uint32_t ultimo = tiempo_ms();
    entradas_iniciar(ultimo);

    while (1) {
        uint32_t ahora = tiempo_ms();
        if (ahora == ultimo) continue;      // todo corre una vez por ms
        ultimo = ahora;

        adc_tick();
        entradas_actualizar(ahora);
#if DEBUG_UART
        debug_entradas();
#endif
        piston_tick(ahora);
        ejecutar_secuencia(ahora);
        cinta_tick(ahora);
        foto_tick(ahora);
        buzzer_tick(ahora);
    }
}