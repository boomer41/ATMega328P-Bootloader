#ifndef UART_H_
#define UART_H_

#define FLASH_STR(x) ((const __flash char[]) {x})
#define STATIC_FLASH_STR(x) ({ static const __flash char __string[] = (x); __string; })

#define uart_sendstr(x) uart_sendstr_flash(STATIC_FLASH_STR(x))

void uart_putc(unsigned char c);
void uart_init();
void uart_sendstr_flash(const __flash char* c);
void uart_sendhex16(uint16_t value);
uint8_t uart_read(unsigned char* buf, uint8_t bufsiz);

void uart_xoff();
void uart_xon();

#endif /* UART_H_ */
