#include <SPI.h>

// SRAM instruction set
#define CMD_READ_STATUS  0x05 // Läser statusregistret
#define CMD_WRITE_STATUS 0x01 // skriver till statusregistret
#define CMD_READ         0x03 //Läsa data från SRAM
#define CMD_WRITE        0x02 //Skriva data till SRAM

// Funktionen som läser en byte från SRAM vid en specifik adress 
uint8_t ReadByteFromSRAM(uint16_t addr) {
  uint8_t received_data; //lagrar mottagna data
  PORTB &= ~(1 << PORTB2);  // Sätter Chip Select till låg för att aktivera SRAM
  SPI.transfer(CMD_READ); // Skickar läskommandon
  SPI.transfer((addr >> 8) & 0xFF);  // Skickar höga byten av adress 
  SPI.transfer(addr & 0xFF);         // Skickar låga byten av adress
  received_data = SPI.transfer(0x00); // Läs data från SRAM
  PORTB |= (1 << PORTB2);  // Chip Select till hög för att avaktivera SRAM
  return received_data; // returnerar mottagna byten
}

 // Skriver en byte till SRAM vid en specifik adress
 void WriteByteToSRAM(uint16_t addr, uint8_t value) {
  PORTB &= ~(1 << PORTB2);  // Aktiverar chip 
  SPI.transfer(CMD_WRITE); // skickar skriv kommandon 
  SPI.transfer((addr >> 8) & 0xFF);  
  SPI.transfer(addr & 0xFF);         
  SPI.transfer(value);               // Skicka datan som ska skrivas 
  PORTB |= (1 << PORTB2);  // Avaktivera chip
}

void setup() {
  Serial.begin(9600); //initierar seriekommunikation med 9600 baud
  SPI.begin(); //initiera SPI-kommunikation 
}

void loop() {
  if (Serial.available() > 0) { // kontroll om det finns data att läsa från serieporten 
    char command = Serial.read();  // läs det första kommandot R,W
    uint16_t addr = 0; // Variable för adress 
    uint8_t data = 0; // variable för data

    while (Serial.available() && Serial.peek() == ' ') Serial.read(); // HOppar över mellanslag 

    // Process 'W' and 'R' commands that require an address
    if (command == 'W' || command == 'R') { 
      addr = Serial.parseInt();  // Läser adressen från serieporten
      while (Serial.available() && Serial.peek() == ' ') Serial.read(); // hoppa över mellaslag
    }

    if (command == 'W') {  // Skriv kommando
      data = Serial.parseInt();  // Läser datavärdet från porten
      WriteByteToSRAM(addr, data); // Skriver datavärdet till angivna adress
      Serial.print("Data "); 
      Serial.print(data); // Skriv ut data
      Serial.print(" written to address ");
      Serial.println(addr); // skriv ut adress
    } 
    else if (command == 'R') {  
      data = ReadByteFromSRAM(addr); //Läs data från det angivna adress
      Serial.print("Address "); 
      Serial.print(addr); //skriv ut 
      Serial.print(" contains value: ");
      Serial.println(data); //skriv ut lästa datavärdet
    } 
    }

    while (Serial.available()) Serial.read(); // Rensa återstående data i serieporten
  }

