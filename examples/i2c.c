#include "stm32f4xx.h"

#define CS43L22_ADDR_WRITE 0x94
#define CS43L22_ADDR_READ 0x95

void cs43l22_hw_reset(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  GPIOD->MODER &= ~GPIO_MODER_MODER4;
  GPIOD->MODER |= GPIO_MODER_MODER4_0;

  GPIOD->BSRR = GPIO_BSRR_BR4;
  GPIOD->BSRR = GPIO_BSRR_BS4;
}

uint8_t cs43l22_read_reg(uint8_t reg_addr) {
  uint8_t data;

  // Generate START condition
  I2C1->CR1 |= I2C_CR1_START;
  while (!(I2C1->SR1 & I2C_SR1_SB))
    ; // Wait for start bit

  // Send Device Address (Write mode)
  I2C1->DR = CS43L22_ADDR_WRITE;
  while (!(I2C1->SR1 & I2C_SR1_ADDR))
    ; // Wait for Address match
  (void)I2C1->SR1;
  (void)I2C1->SR2; // Clear ADDR flag by reading SR1 then SR2

  // Send Register Address
  I2C1->DR = reg_addr;
  while (!(I2C1->SR1 & I2C_SR1_TXE))
    ; // Wait for Transmit buffer empty
  while (!(I2C1->SR1 & I2C_SR1_BTF))
    ; // Wait for Byte Transfer Finished

  // Generate RESTART condition
  I2C1->CR1 |= I2C_CR1_START;
  while (!(I2C1->SR1 & I2C_SR1_SB))
    ;

  // Send Device Address (Read Mode)
  I2C1->DR = CS43L22_ADDR_READ;
  while (!(I2C1->SR1 & I2C_SR1_ADDR))
    ;

  // Before clearing ADDR, disable ACK and configure STOP
  // (since we are only reading 1 byte)
  I2C1->CR1 &= ~I2C_CR1_ACK;

  (void)I2C1->SR1; // Dummy read
  (void)I2C1->SR2; // Clear ADDR flag

  I2C1->CR1 |= I2C_CR1_STOP;

  // Wait for data to arrive
  while (!(I2C1->SR1 & I2C_SR1_RXNE))
    ;
  data = I2C1->DR;

  // Re-enable ACK for future operations
  I2C1->CR1 |= I2C_CR1_ACK;

  return data;
}

void i2c1_init(void) {

  // Enable Clocks
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

  // Configure PB6 (SCL) and PB9 (SDA)
  // Set to Alternate Function mode (b'10)
  GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER9);
  GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER9_1);

  // Set output type to Open Drain (1)
  GPIOB->OTYPER |= (GPIO_OTYPER_OT6 | GPIO_OTYPER_OT9);

  // Set speed to High (10)
  GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR6_1 | GPIO_OSPEEDER_OSPEEDR9_1);

  // Enable pull-up resistors (01)
  GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR9);
  GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR9_0);

  // Set Alternate Function to AF4 for both pins
  // PB6 is in AFR[0] (low), PB9 is in AFR[1] (high)
  GPIOB->AFR[0] |= (4 << (6 * 4));       // AF4 for Pin 6
  GPIOB->AFR[1] |= (4 << ((9 - 8) * 4)); // AF4 for Pin 9

  // Reset and Configure I2C1
  I2C1->CR1 |= I2C_CR1_SWRST;
  I2C1->CR1 &= ~I2C_CR1_SWRST;

  // Assuming standard 16 MHz APB1 clock (HSI default for unconfigured clock
  // tree).
  I2C1->CR2 = 16;

  // Configure Clock Control Register (CCR) for Standard Mode (100 kHz)
  // T_high = CCR * T_pclk1 -> CCR = PCLK1 / (2 * 100,000)
  I2C1->CCR = 80;

  // Max rise time: (PCLK1 frequency in MHz) + 1
  I2C1->TRISE = 17;

  // Enable the peripheral
  I2C1->CR1 |= I2C_CR1_PE;
}

int main(void) {
  i2c1_init();
  cs43l22_hw_reset();
  volatile uint8_t child_id = cs43l22_read_reg(0x01);

  while (1)
    ;
}
