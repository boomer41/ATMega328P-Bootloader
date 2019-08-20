#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "uart.h"
#include "config.h"

#define XOFF ((unsigned char) 0x13)
#define XON ((unsigned char) 0x11)

#define ARRSIZE(x) (sizeof(x)/sizeof(*(x)))

#define BAUD 38400
#define UBBRVAL (F_CPU/16/BAUD-1)

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

static volatile unsigned char uart_buffer[128];
static volatile uint8_t uart_buffer_count;
static volatile uint8_t uart_buffer_read_pos;
static volatile uint8_t uart_buffer_write_pos;
static volatile uint8_t uart_rx_enabled;

void uart_putc(unsigned char c) {
    while (!(UCSR0A & _BV(UDRE0)));
    UDR0 = c;
}

void uart_init() {
    uart_buffer_read_pos = 0;
    uart_buffer_write_pos = 0;
    uart_buffer_count = 0;
    uart_rx_enabled = 1;

    UBRR0H = (UBBRVAL >> 8) & 0xFF;
    UBRR0L = UBBRVAL & 0xFF;
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

    uart_putc(XON);

    uart_sendstr("\n\n\n\n");
}

static uint8_t uart_must_stop_rx() {
    return uart_buffer_count > ARRSIZE(uart_buffer) - 32;
}

static uint8_t uart_can_start_rx() {
    return uart_buffer_count < ARRSIZE(uart_buffer) - 4;
}

void uart_xon() {
    cli();
    if (uart_rx_enabled || !uart_can_start_rx()) {
        sei();
        return;
    }

    uart_rx_enabled = 1;
    uart_putc(XON);
    sei();
}

void uart_xoff() {
    cli();
    if (!uart_rx_enabled) {
        sei();
        return;
    }

    uart_rx_enabled = 0;
    uart_putc(XOFF);
    sei();
}

void uart_sendstr_flash(const __flash char* c) {
    while (*c) {
        uart_putc(*c++);
    }
}

ISR(USART_RX_vect) {
    uint8_t rx = UDR0;

    if (uart_buffer_count >= ARRSIZE(uart_buffer)) {
        return;
    }

    uart_buffer[uart_buffer_write_pos] = rx;
    uart_buffer_write_pos = (uart_buffer_write_pos + 1) % ARRSIZE(uart_buffer);
    uart_buffer_count++;

    if (uart_must_stop_rx()) {
        uart_xoff();
    }
}

uint8_t uart_read(unsigned char* buf, uint8_t bufsiz) {
    cli();

    uint8_t toRead = MIN(bufsiz, uart_buffer_count);

    for (uint8_t i = 0; i < toRead; ++i) {
        buf[i] = uart_buffer[uart_buffer_read_pos];
        uart_buffer_read_pos = (uart_buffer_read_pos + 1) % ARRSIZE(uart_buffer);
    }

    uart_buffer_count -= toRead;

    uart_xon();

    return toRead;
}

static const __flash unsigned char HEX_LOOKUP[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

void uart_sendhex16(uint16_t value) {
    uart_sendstr("0x");
    uart_putc(HEX_LOOKUP[(value >> 12) & 0x0F]);
    uart_putc(HEX_LOOKUP[(value >> 8) & 0x0F]);
    uart_putc(HEX_LOOKUP[(value >> 4) & 0x0F]);
    uart_putc(HEX_LOOKUP[value & 0x0F]);
}

