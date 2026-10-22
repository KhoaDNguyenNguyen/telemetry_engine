import re

with open('app/main.c', 'r') as f:
    src = f.read()

i2c_funcs = """static inline void i2c_delay(void) {
    for(volatile int i = 0; i < 2000; i++) __asm__ volatile("nop");
}

static void i2c_set_scl(uint8_t state) {
    if (state) {
        hal_gpio_set_function(I2C_SCL, GPIO_INPUT);
    } else {
        hal_gpio_write(I2C_SCL, 0);
        hal_gpio_set_function(I2C_SCL, GPIO_OUTPUT);
    }
    i2c_delay();
}

static void i2c_set_sda(uint8_t state) {
    if (state) {
        hal_gpio_set_function(I2C_SDA, GPIO_INPUT);
    } else {
        hal_gpio_write(I2C_SDA, 0);
        hal_gpio_set_function(I2C_SDA, GPIO_OUTPUT);
    }
    i2c_delay();
}

static void i2c_start(void) {
    i2c_set_sda(1); i2c_set_scl(1);
    i2c_set_sda(0); i2c_set_scl(0);
}

static void i2c_stop(void) {
    i2c_set_sda(0); i2c_set_scl(1); i2c_set_sda(1);
}

static void i2c_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        i2c_set_sda((data & 0x80U) ? 1 : 0);
        data <<= 1;
        i2c_set_scl(1); i2c_set_scl(0);
    }
    i2c_set_sda(1); i2c_set_scl(1); i2c_set_scl(0);
}

static void lcd_send(uint8_t data, uint8_t mode) {
    uint8_t high = (uint8_t)(data & 0xF0U);
    uint8_t low = (uint8_t)((data << 4) & 0xF0U);
    uint8_t bl = 0x08U;
    
    i2c_start();
    i2c_write_byte((uint8_t)(LCD_ADDR << 1)); 
    i2c_write_byte((uint8_t)(high | mode | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(high | mode | bl)); i2c_delay();
    i2c_write_byte((uint8_t)(low | mode | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(low | mode | bl)); i2c_delay();
    i2c_stop();
}

static void lcd_send_cmd_raw(uint8_t data) {
    uint8_t bl = 0x08U;
    i2c_start();
    i2c_write_byte((uint8_t)(LCD_ADDR << 1));
    i2c_write_byte((uint8_t)(data | bl | 0x04U)); i2c_delay();
    i2c_write_byte((uint8_t)(data | bl)); i2c_delay();
    i2c_stop();
}
"""

src = re.sub(r'static inline void i2c_delay\(void\) \{.*?static void lcd_init\(void\)', i2c_funcs + '\nstatic void lcd_init(void)', src, flags=re.DOTALL)

with open('app/main.c', 'w') as f:
    f.write(src)
