#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <util/delay.h>

#include "uart.h"

const uint8_t header_len = 2 + 4 + 2 + 2;
struct hex_record {
    uint8_t data_length[2];
    uint8_t address[4];
    uint8_t record_type[2];
    uint8_t data[(32 + 2) * 2];
};

static void boot_app(void)  __attribute__((noreturn));
static uint8_t record_checksum_verify(struct hex_record* record);
static uint8_t hex_to_uint8(const unsigned char c[2]);
static uint16_t hex_to_uint16(const unsigned char c[4]);
static void program_page(uint16_t page, uint8_t* data);

ISR(TIMER1_COMPA_vect) {
    boot_app();
}

int main(void)
{
    // Use interrupts of bootloader
    MCUCR = _BV(IVCE);
    MCUCR = _BV(IVSEL);

    uart_init();
    uart_sendstr("Press p to program\n");

    // 16-Bit timer in CTC mode, Prescaler 1024
    TCCR1A = _BV(WGM12);
    OCR1A = 65535;
    TIMSK1 = _BV(OCIE1A);
    TCCR1B = _BV(CS12) | _BV(CS10);

    sei();

    // Loop until p is read, or we get killed by the timer.
    unsigned char c;
    while (1) {
        if (!uart_read(&c, 1)) {
            continue;
        }

        if (c == 'p') {
            break;
        }
    }

    // Disable timer.
    TCCR1B = 0;

    uart_sendstr("Entering programming mode\n");

    uint8_t page_buffer_start_set = 0;
    uint16_t page_buffer_start = 0;
    uint8_t page_buffer[SPM_PAGESIZE];

    for (uint8_t i = 0; i < SPM_PAGESIZE; i++) {
        page_buffer[i] = 0xFF;
    }

    while (1) {
        unsigned char c;
        while (!uart_read(&c, 1) || c != ':');

        struct hex_record record;
        uint8_t data_read = 0;
        while (data_read < sizeof(struct hex_record) && (data_read < header_len || 2 * hex_to_uint8(record.data_length) + header_len != data_read)) {
            data_read += uart_read(((unsigned char*) &record) + data_read, 1);
        }

        if (!record_checksum_verify(&record)) {
            uart_sendstr("Checksum verification failed\n");
            continue;
        }

        uint16_t recordAddr;

        switch (record.record_type[1]) {
            /* End of Transmission */
            case '1':
                if (page_buffer_start_set) {
                    program_page(page_buffer_start, page_buffer);
                }

                boot_app();
                break;
            /* Data */
            case '0':
                recordAddr = hex_to_uint16(record.address);

                if (!page_buffer_start_set) {
                    page_buffer_start_set = 1;
                    page_buffer_start = recordAddr - recordAddr % SPM_PAGESIZE;
                }

                uint8_t dataLength = hex_to_uint8(record.data_length);
                for (uint8_t i = 0; i < dataLength; i++) {
                    uint16_t byteAddr = recordAddr + i;

                    if (byteAddr >= page_buffer_start + sizeof(page_buffer) || byteAddr < page_buffer_start) {
                        program_page(page_buffer_start, page_buffer);
                        for (uint8_t i = 0; i < SPM_PAGESIZE; i++) {
                            page_buffer[i] = 0xFF;
                        }

                        page_buffer_start = byteAddr - byteAddr % sizeof(page_buffer);
                    }

                    uint8_t offset = byteAddr - page_buffer_start;
                    page_buffer[offset] = hex_to_uint8(record.data + i*2);
                }

                continue;
            default:
                continue;
        }
    }

    while (1);
}

static void boot_app(void) {
    // Disable timer.
    TCCR1B = 0;

    uart_sendstr("Booting app\n");

    cli();

    // Move interrupts back to address 0
    MCUCR = _BV(IVCE);
    MCUCR = 0;

    goto *0;
}

static uint8_t record_checksum_verify(struct hex_record* record) {
    uint8_t sum = 0;
    unsigned char* p = (unsigned char*) record;
    uint8_t record_length = header_len/2 + hex_to_uint8(record->data_length);

    for (uint8_t i = 0; i < record_length; i++) {
        sum += hex_to_uint8(p + i*2);
    }

    return sum == 0;
}

static uint8_t hex_to_uint4(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }

    return 0;
}

static uint8_t hex_to_uint8(const unsigned char c[2]) {
    return hex_to_uint4(c[0]) << 4 |
            hex_to_uint4(c[1]);
}

static uint16_t hex_to_uint16(const unsigned char c[4]) {
    return hex_to_uint8(c) << 8 |
            hex_to_uint8(c + 2);
}

static void program_page(uint16_t page, uint8_t* data) {
    uart_sendstr("Programming page ");
    uart_sendhex16(page);
    uart_putc('\n');

    uart_xoff();
    _delay_ms(1);
    cli();

    boot_page_erase(page);
    boot_spm_busy_wait();

    for (uint8_t i = 0; i < SPM_PAGESIZE; i += 2) {
        boot_page_fill(page + i, data[i] | data[i + 1] << 8);
    }

    boot_page_write(page);
    boot_spm_busy_wait();

    boot_rww_enable();

    sei();
    uart_xon();
}
