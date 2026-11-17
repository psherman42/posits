#ifndef GPIO_H__INCLUDED
#define GPIO_H__INCLUDED

enum gpio_dir_t { GPIO_IN, GPIO_OUT };
enum gpio_iof_t { GPIO_GPIO, GPIO_UART, GPIO_SPI, GPIO_I2C, GPIO_PWM };
enum gpio_xor_t { GPIO_NORM, GPIO_INV };

void gpio_init (void);

uint8_t gpio_read (uint8_t g);
void gpio_high (uint8_t g);
void gpio_low (uint8_t g);
void gpio_dir (uint8_t g, enum gpio_dir_t x);
void gpio_iof (uint8_t g, enum gpio_iof_t x);
void gpio_pu_dis (uint8_t g);
void gpio_pu_ena (uint8_t g);
void gpio_ds_low (uint8_t g);
void gpio_ds_high (uint8_t g);
void gpio_xor (uint8_t g, enum gpio_xor_t x);

#endif//GPIO_H__INCLUDED
