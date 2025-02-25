/*
 * Project: Arduino Project to Communication and Write with SPI-NOR
 * Author:Jampag [https://github.com/Jampag/UART_to_SPI-NOR.git]
 * Data YYYY-MM-DD: 2025-02-25
 * Version : 0.1.2
 * Description : 
 *   This firmware allows communication with an SPI NOR Flash memory via UART, 
 *   providing operations such as reading, writing, erasing, and CRC32 
 *   calculation.
 *   It supports file transfer using the XMODEM protocol or send file with 
 *   Teraterm or similar.        
 *
 * Repository: https://github.com/Jampag/Arduino-UART-SPI-NOR-testbench
 *
 * Features:
 *   - Only support 1x SPI
 *   - Read/Write to SPI NOR Flash memory
 *   - Sector/Block erase operations
 *   - CRC32 calculation over a memory range
 *   - File transfer using XMODEM or UART
 *   - Configurable SPI settings
 *   - Dump Flash memory to UART
 *   - Compatible with 25-series SPI NOR Flash
 *     Max Flash size is 16777216bits/2,09715MByte
 *     (The addresses are 24 bits equal to 16777216 bits)
 *
 * Hardware Requirements:
 *   - Arduino Minima R4 microcontroller
 *     MISO(CIPO)=D12; MOSI(COPI)=D11; SCK=D13
 *   - Compatible SPI NOR Flash memory x25 family
 *   - Level translator bidirectional 3V3<->5V
 *   
 *
 * Setting Arduino Serial Monitor: 
 *    Carriage Return; 115200baud; not use to UART to flash
 * 
 * TERATERM 
 *  General Setting:
 *      -Setup>Terminal> Receive=AUTO; Transmit=CR
 *      -Setup>Serial port> Speed=115200; Data=8b; Parity=none; Stop bits=1;
 *        Flowcontrol=Xon/Xoff; Transmit delay=0msec
 *  Version:
 *      -Tested with verion 5.3
 *  Tranfer setting:
 *      -File>Send file> 
 *          Filename: <file>
 *          Binary:[checked]
 *          delay type: <per sendsize>
 *          send size(bytes): 256
 *          delay time(ms): <0 or 1>
 *
 * Changelog:
 *    0.1.2 - Added CRC32 compute on data from UART, at option '9'
 *    0.1.1 - Added print ASCII at option 4
 *    0.1.0 - Initial version with XMODEM support and basic SPI NOR Flash 
 *            functions.
 *            Tested with SPI-NOR MX25L128356 128Mbit (Mn.ID=C2 D.ID=17) 
 *            attention 3V3 
 *            Arduino Minima R4 needed voltage level translator to interconnect
 *            SPI-NOR.
 */
 

#include <SPI.h>
//#include <CRC32.h> //Thanks Christopher Baker 2.0.0
#define XON  0x11
#define XOFF 0x13

#define CS_FLASH 10  // Chip Select SPI Flash
#define RST_FLASH 9  // Reset SPI Flash

const int BUFFER_SIZE = 256; // byte
char buffer[BUFFER_SIZE];
int bufferIndex = 0;
uint32_t flashAddress = 0;
bool receiving = false;
bool inWriteLoop = false;
bool Attend_buffer = true;
unsigned long execStart = 0;
unsigned long execStop = 0;
unsigned long exec = 0;
unsigned long lastWriteTime = 0;
unsigned long totalBytesWritten = 0;
unsigned long totalBlocksWritten = 0;
uint32_t addr;
uint32_t numByte;
int blockSize, numBlocks;
uint32_t crc = ~0L; // CRC32
//SPI
uint32_t clkSPI = 2000000;
BitOrder bitOrder = MSBFIRST;
uint8_t spiMode = SPI_MODE0;

//XMODEM
#define SOH  0x01  // Start of Header (inizio blocco)
#define EOT  0x04  // End of Transmission (fine trasmissione)
#define ACK  0x06  // Acknowledge (dati ricevuti correttamente)
#define NAK  0x15  // Negative Acknowledge (errore, ritrasmetti)
#define CAN  0x18  // Cancel
#define XMODEM_BUFFER_SIZE 128  // Evita conflitti con BUFFER_SIZE


void setup() {
    Serial.begin(115200);
    delay(2000);   
    //Arduino R4 MISO(CIPO)=D12; MOSI(COPI)=D11; SCK=D13
    init_default_SPI();
    showMenu();
}


void loop() {
    int command=0;

    if (inWriteLoop) {
        UART_to_SNOR();
        return;
    }
    else{
    
        Serial.print(" ");
        String input = readSerialCommand();  // Legge il comando
        if (input.length() == 0) return;
        // Serial.println();

        command = input.toInt();  // Converte l'input in numero
        //Initialized variable:
        addr=0;
        numByte=0;
        blockSize=0;
        numBlocks=0;
        bufferIndex=0;
        flashAddress=0;
    }

    switch (command) {
        case 1: { // blockErase

            Serial.print(" Enter start address (hex): ");
            addr = strtoul(readSerialCommand().c_str(), NULL, 16);
            
            Serial.print(" Enter block size (4096, 32768, 65536): ");
            blockSize = readIntInput();
            
            Serial.print(" Enter number of blocks: ");
            numBlocks = readIntInput();

            execStart = millis(); 
            blockErase(addr, blockSize, numBlocks);
            execStop = millis();
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print(">");
            break;
        }

        case 2: // writeEnable
            writeEnable();
            Serial.print(" WREN enabled,latch/self clear ");
            Serial.print(">");
            break;

        case 3:
            execStart = millis();           
            chipErase();
            execStop = millis();
            
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print(">");
            break;

            
        case 4: {
            Serial.print(" Enter start address (hex): ");
            addr = strtoul(readSerialCommand().c_str(), NULL, 16);

            Serial.print(" Enter number of bytes (hex): ");
            numByte = strtoul(readSerialCommand().c_str(), NULL, 16);

            execStart = millis();
            readFlash(addr, numByte);
            
            execStop = millis();
            Serial.print("\nExecution time: ");
            Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print(">");
            break;
        }

        case 5: // Status register
            printStatusRegister();
            showMenu();
            break;

        case 6: { // Write data
            Serial.println("Erase block before write! ");
            Serial.print(" Enter start address (hex): ");
            addr = strtoul(readSerialCommand().c_str(), NULL, 16);
            Serial.print(" Enter data byte (hex): ");
            byte data = (byte) strtoul(readSerialCommand().c_str(), NULL, 16);
            
            writeByte(addr, data);            
            
            Serial.print(">");
            break;
        }

        case 7: 
            readREMS();
            Serial.print(">");
            break;

        case 8:
            readRDID();
            Serial.print(">");
            break;        

        case 9:
            crc = ~0L;
            Serial.print("Enter start address (hex): ");
            flashAddress = strtoul(readSerialCommand().c_str(), NULL, 16); //
            inWriteLoop = true;
            delay(100);
            Attend_buffer = false; // Si avvia solo dopo 1 buffer 256
            Serial.println(" ****** TERATEM ******");
            Serial.println(" * General Setting: ");
            Serial.println(" *     -Setup>Terminal> Receive=AUTO; Transmit=CR");
            Serial.println(" *     -Setup>Serial port> ");
            Serial.println(" *       Speed=115200; Data=8b; Parity=none; Stop bits=1");
            Serial.println(" *       Flowcontrol=Xon/Xoff; Transmit delay=0msec");
            Serial.println(" * Version:");
            Serial.println(" *     -Tested with verion 5.3");
            Serial.println(" * Tranfer setting:");
            Serial.println(" *     -File>Send file> ");
            Serial.println(" *         Filename: <file>");
            Serial.println(" *         Binary:[checked]");
            Serial.println(" *         delay type: <per sendsize>");
            Serial.println(" *         [send size(bytes): 256 ]");
            Serial.println(" *         delay time(ms): <0 or 1>");
            Serial.println("\nExecute chip Erase or erase blocks equal to the file size!");
            Serial.println("\nMinimun file size is 256byte!");
            Serial.println("\nReady to recive data from UART...");
            
            execStart = millis();
            UART_to_SNOR();
            return;
        
        case 10: {  
            Serial.print(" Enter start address (hex): ");
            addr = strtoul(readSerialCommand().c_str(), NULL, 16);

            Serial.print(" Enter number of bytes (hex): ");
            numByte = strtoul(readSerialCommand().c_str(), NULL, 16);

            execStart = millis();
            unsigned long crc32 = calcCRC32(addr, numByte);
            execStop = millis();
            Serial.print(" CRC32: 0x");
            Serial.println(crc32, HEX);
            
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print(">");
            break;
        }
        case 11:
            hardwareRST();
            Serial.print(">");
            break;
        case 12:
            Serial.println(" ****** TERATEM ******");
            Serial.println(" * General Setting: ");
            Serial.println(" *     -Setup>Terminal> Receive=AUTO; Transmit=CR");
            Serial.println(" *     -Setup>Serial port> Speed=115200; Data=8b; Parity=none; Stop bits=1;");
            Serial.println(" *       Flowcontrol=Xon/Xoff; Transmit delay=0msec");
            Serial.println(" * Version:");
            Serial.println(" *     -Tested with verion 5.3");
            Serial.println(" * Acquire setting:");
            Serial.println(" *     -File>Log...> ");
            Serial.println(" *         Filename: <file>");
            Serial.println(" *         Write mode: Append");
            Serial.println(" *         Binary:[checked]");
            Serial.println(" *Execute this configuration then set waiting time");
            Serial.println(" * 1-Start log file while LED is blinking");
            Serial.println(" * 2-When the LED is ON,the acquisition is in progress");
            Serial.println(" * 3-When the LED is OFF,close the log file\n");
            dumpFlashToUART();
            break;
        case 13:
            setSPI();
            Serial.print(">");
            break;
        case 14:
            execStart = millis();
            customSPITransaction();
            execStop = millis();
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print("\n>");
            break;
        case 15:
            receiveXmodem();
            Serial.print(">");
            break;
        case 16:
            sendXmodem();
            Serial.print(">");
            break;
        default:
            Serial.println(" Command not found!");
            showMenu();  // Mostra di nuovo il menu
            break;
    }
    
}

// Mostra il menu
void showMenu() {
    delay(100);
    Serial.println(" ");
    Serial.println("############## MENU ##############");
    Serial.println("1. blockErase 4096/32768/65536 [Opcode 20h/52h/D8h]");
    Serial.println("2. writeEnable [Opcode 06h WREN]");
    Serial.println("3. chipErase [Opcode C7h]");
    Serial.println("4. readFlash [Opcode 03h RDSR]");
    Serial.println("5. statusRegister [Opcode=0x05 RDSR]");
    Serial.println("6. writeData [Opcode 02h PP]");
    Serial.println("7. deviceID  [Opcode 90h REMS]");
    Serial.println("8. deviceID  [Opcode 9Fh RDID]");
    Serial.println("9. Reads data from UART and writes it to flash");
    Serial.println("10. CRC32 calc");
    Serial.println("11. HW Reset cylce");
    Serial.println("12. Dump Flash to UART");
    Serial.println("13. SPI setting");
    Serial.println("14. Custom SPI Transaction");
    Serial.println("15. Receive file XMODEM and write into flash");
    Serial.println("16. Send file XMODEM");
    Serial.print(">");
}

String readSerialCommand() {
    String command = "";
    bool isFirstChar = true;
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {  // Rileva ENTER (CR o LF)
                if (isFirstChar){
                  command = "0";
                  break;
                } 
                if (command.length() > 0) break;  // Solo se c'è almeno un carattere
            } else {
                Serial.print(c);  // Echo sulla console
                command += c;
                isFirstChar = false;
            }
        }
    }
    Serial.println();  // Nuova riga dopo l'input
    return command;
}


int readIntInput() {
    String input = readSerialCommand();
    return input.toInt();
}

void UART_to_SNOR(){
    //Minimun size is 256byte
    if (Serial.available()) {
        if (!receiving) {
            Serial.write(XOFF);  
            Serial.println("Writing to SPI NOR started...");
            receiving = true;
            
        }
        while (Serial.available() && bufferIndex < BUFFER_SIZE) {
            buffer[bufferIndex++] = Serial.read();
            
        }

        // Se il buffer è pieno, esegui la scrittura su SPI NOR
        if (bufferIndex == BUFFER_SIZE) {
            Serial.write(XOFF);
            writeEnable();  // Abilita la scrittura sulla memoria flash
            writePage(flashAddress, buffer, BUFFER_SIZE);  // Scrive i dati
            waitForWriteCompletion();  // Aspetta che la scrittura sia completata

            flashAddress += BUFFER_SIZE;  // Aggiorna l'indirizzo
            totalBytesWritten += BUFFER_SIZE;
            totalBlocksWritten++;

          // Calcola CRC32 per i dati nel buffer
          for (int i = 0; i < BUFFER_SIZE; i++) {
            uint8_t data = buffer[i];
            // Aggiornamento CRC con il nuovo byte letto
            crc ^= data;
            for (uint8_t j = 0; j < 8; j++) {
              crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
          }

            Serial.print("Write ");
            Serial.print(BUFFER_SIZE);
            Serial.print(" bytes - Block #");
            Serial.println(totalBlocksWritten);

            bufferIndex = 0;  
            Attend_buffer = true ;
            lastWriteTime = millis();  // Memorizza il tempo dell'ultima scrittura
            Serial.write(XON);  // Riabilita l'invio dei dati dalla seriale
        }

        
    }

    // Se la scrittura è in corso e ci sono ancora dati nel buffer
    if (receiving && Serial.available() == 0 && bufferIndex > 0 && Attend_buffer) {
        unsigned long currentTime = millis();  // Ottieni il tempo corrente

        // Se è passato un tempo sufficiente dall'ultima scrittura
        if (currentTime - lastWriteTime >= 1000) {
            Serial.print("Writing last byte buffer: ");
            Serial.println(bufferIndex);

            writeEnable();
            writePage(flashAddress, buffer, bufferIndex);  // Scrivi l'ultimo buffer
            waitForWriteCompletion();  // Attendi la fine della scrittura

            // Calcola CRC32 per i dati nel buffer
            for (int i = 0; i < bufferIndex; i++) {
              uint8_t data = buffer[i];
              // Aggiornamento CRC con il nuovo byte letto
              crc ^= data;
              for (uint8_t j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
              }
            }    


            totalBytesWritten += bufferIndex;
            totalBlocksWritten++;

            Serial.print("Last buffer size");
            Serial.print(bufferIndex);
            Serial.println(" byte!");

            bufferIndex = 0;  // Reset del buffer
            Serial.println("File saved into SPI NOR!");
            Serial.print("Total blocks written: ");
            Serial.println(totalBlocksWritten);
            Serial.print("Total bytes written: ");
            Serial.println(totalBytesWritten);
            Serial.print("CRC32: ");
            Serial.println(~crc, HEX);   		            

            receiving = false;  // Fine della ricezione dei dati
            inWriteLoop = false;  // Esci dal ciclo di scrittura
            execStop = millis();
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");
            Serial.print("> ");	
            addr=0;
            numByte=0;
            blockSize=0;
            numBlocks=0;
            bufferIndex=0;
            flashAddress=0;
            totalBytesWritten=0;
            totalBlocksWritten=0;
        }
    }
}



// **Abilita la scrittura**
void writeEnable() {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x06);
    digitalWrite(CS_FLASH, HIGH);
}

// Scrive una pagina da 256 byte [Opcode 0x02]
void writePage(uint32_t addr, char *data, int len) {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x02);  

    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    for (int i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }

    digitalWrite(CS_FLASH, HIGH);
}


// Scrive un singolo byte [Opcode 0x02]
void writeByte(uint32_t addr, byte data) {
    writeEnable();
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x02);  // PP PageProgram

    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    SPI.transfer(data);

    digitalWrite(CS_FLASH, HIGH);
    waitForWriteCompletion();
    Serial.print(" Byte written to address 0x");
    Serial.print(addr, HEX);
    Serial.print(": 0x");
    Serial.println(data, HEX);
}


// **Attende il completamento della scrittura leggendo lo Status Register**
void waitForWriteCompletion() {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x05); // Comando RDSR (Read Status Register)

    while (SPI.transfer(0x00) & 0x01) {  // Controlla il bit BUSY
        delayMicroseconds(10);
    }

    digitalWrite(CS_FLASH, HIGH);
}


// Cancella tutta la memoria [Opcode 0xC7]
void chipErase() {
    writeEnable();
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0xC7);
    digitalWrite(CS_FLASH, HIGH);
    int i=0;

    Serial.println("Chip Erase in progress...");
    while (isBusy()) {
        delay(500);
        Serial.print(".");
        i++;
        if (i == 16){
          Serial.println(".");
          i=0;
        }

    }
    Serial.println("Chip Erase completed!");
}

// **Cancella più settori specifici (4KB, 32KB, 64KB)**
void blockErase(uint32_t addr, int blockSize, int numBlocks) {
    
    if (blockSize != 4096 && blockSize != 32768 && blockSize != 65536) {
        Serial.println("Unsupported size! Use 4096, 32768, or 65536.");
        return;
    }

    for (int i = 0; i < numBlocks; i++) {
        writeEnable();
        digitalWrite(CS_FLASH, LOW);

        if (blockSize == 4096) SPI.transfer(0x20);  // Sector Erase (4KB)
        else if (blockSize == 32768) SPI.transfer(0x52);  // Block Erase (32KB)
        else if (blockSize == 65536) SPI.transfer(0xD8);  // Block Erase (64KB)

        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);

        digitalWrite(CS_FLASH, HIGH);

        Serial.print("Block erase ");
        Serial.print(blockSize / 1024);
        Serial.print("KB from address 0x");
        Serial.println(addr, HEX);

        while (isBusy()) {
            delay(100);
        }
        addr += blockSize;
    }

    Serial.println("All blocks are erased!");
}

// **Verifica se la memoria è occupata**
bool isBusy() {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x05); // Comando RDSR (Read Status Register)
    bool busy = SPI.transfer(0x00) & 0x01;
    digitalWrite(CS_FLASH, HIGH);
    return busy;
}

// **Legge lo Status Register e lo stampa**
void printStatusRegister() {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x05); // Comando RDSR (Read Status Register)
    byte status = SPI.transfer(0x00);
    digitalWrite(CS_FLASH, HIGH);

    Serial.print("Status Register: 0x");
    Serial.println(status, HEX);

    if (status & 0x01) Serial.println("Memory BUSY");
    if (status & 0x02) Serial.println("Write Enabled");
    if (status & 0x0C) Serial.println("Write protection");
}

// **Lettura di verifica con formattazione esadecimale**
// **Lettura di verifica con formattazione esadecimale e ASCII**
void readFlash(uint32_t addr, uint32_t numByte) {
    const int bytesPerRow = 16; // Stampa massimo 16 byte per riga
    uint32_t bytesToRead = numByte;
    char asciiBuffer[bytesPerRow + 1]; // Buffer per i caratteri ASCII
    asciiBuffer[bytesPerRow] = '\0'; // Null terminator per la stringa

    Serial.println(" Reading SNOR:");

    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x03); // Opcode Read Data

    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    for (uint32_t i = 0; i < bytesToRead; i++) {
        if (i % bytesPerRow == 0) { 
            // Stampa il buffer ASCII della riga precedente
            if (i != 0) {
                Serial.print("  |");
                Serial.print(asciiBuffer);
                Serial.print("|");
            }
            // Inizia una nuova riga
            Serial.print("\n0x");
            Serial.print(addr + i, HEX);
            Serial.print(": ");
        }

        byte data = SPI.transfer(0x00);
        
        // Stampa esadecimale con zero padding se minore di 16
        Serial.print(data < 16 ? "0" : "");
        Serial.print(data, HEX);
        Serial.print(" ");

        // Conversione in ASCII o sostituzione con un punto se non stampabile
        asciiBuffer[i % bytesPerRow] = (data >= 32 && data <= 126) ? (char)data : '.';
    }

    digitalWrite(CS_FLASH, HIGH);

    // Stampa l'ultima riga del buffer ASCII
    uint32_t remainingBytes = bytesToRead % bytesPerRow;
    if (remainingBytes > 0) {
        // Aggiunge spazi per allineare l'output ASCII
        for (uint32_t j = remainingBytes; j < bytesPerRow; j++) {
            Serial.print("   "); // Spazio per ogni byte mancante
        }
    }
    
    Serial.print("  |");
    Serial.print(asciiBuffer);
    Serial.print("|");

    Serial.println("\n Read complete!");
}


// Legge il Device ID della memoria [Opcode 0x90]
void readREMS() {
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x90); 
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    
    byte manufacturerID = SPI.transfer(0x00);
    byte deviceID = SPI.transfer(0x00);
    digitalWrite(CS_FLASH, HIGH);

    Serial.print("Manufacturer ID: 0x");
    Serial.println(manufacturerID, HEX);
    Serial.print("Device ID: 0x");
    Serial.println(deviceID, HEX);
}

// **Legge il Device ID della memoria Opcode 0x9F**
void readRDID() {
    digitalWrite(CS_FLASH, LOW);
    
    SPI.transfer(0x9F);  // Comando per leggere Device ID   
    byte manufacturerID = SPI.transfer(0x00);
    byte memoryType = SPI.transfer(0x00);
    byte memoryCapacity = SPI.transfer(0x00);

    digitalWrite(CS_FLASH, HIGH);

    Serial.print("Manufacturer ID: 0x");
    Serial.println(manufacturerID, HEX);
    Serial.print("Memory Type: 0x");
    Serial.println(memoryType, HEX);
    Serial.print("Memory Capacity: 0x");
    Serial.println(memoryCapacity, HEX);
}


void hardwareRST() {
    digitalWrite(RST_FLASH, HIGH);
    delay(100);
    digitalWrite(RST_FLASH, LOW);
    delay(100);
    digitalWrite(RST_FLASH, HIGH);
}

unsigned long calcCRC32(uint32_t startAddr, uint32_t numBytes) {
    
    crc = ~0L;  // Inizializza CRC32 con tutti i bit a 1
    uint8_t data;

    Serial.println("Opcode 03h");
    Serial.println("Calculating CRC32...");

    while (numBytes--) {
        // Lettura di un byte dalla memoria SPI
        digitalWrite(CS_FLASH, LOW);
        SPI.transfer(0x03); // Opcode READ DATA
        SPI.transfer((startAddr >> 16) & 0xFF);
        SPI.transfer((startAddr >> 8) & 0xFF);
        SPI.transfer(startAddr & 0xFF);
        data = SPI.transfer(0x00);
        digitalWrite(CS_FLASH, HIGH);

        // Aggiornamento CRC con il nuovo byte letto
        crc ^= data;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }

        startAddr++;  // Incrementa l'indirizzo per il prossimo byte
    }

    Serial.println("CRC32 Calculation Complete!");
    return ~crc;  // XOR finale per completare il calcolo CRC32



/* libreria più efficente di 3 volte
    CRC32 crc32;
    uint32_t bytesRead = 0;

    Serial.println("Calculating CRC32");

    while (bytesRead < numBytes) {
        uint32_t chunkSize = min((uint32_t)BUFFER_SIZE, numBytes - bytesRead);

        // Inizio lettura SPI Flash
        digitalWrite(CS_FLASH, LOW);
        SPI.transfer(0x03); // Comando Read Data
        SPI.transfer((startAddr >> 16) & 0xFF);
        SPI.transfer((startAddr >> 8) & 0xFF);
        SPI.transfer(startAddr & 0xFF);

        // Lettura e aggiornamento CRC
        for (uint32_t i = 0; i < chunkSize; i++) {
            byte data = SPI.transfer(0x00);
            crc32.update(data);
        }

        digitalWrite(CS_FLASH, HIGH);

        // Aggiornamento indici
        bytesRead += chunkSize;
        startAddr += chunkSize;

        // Evita il reset del watchdog ESP8266
       // yield();
    }

    return crc32.finalize();
  */
}

void dumpFlashToUART() {
    Serial.println(" Opcode 03h");
    Serial.print(" Enter start address (hex): ");
    uint32_t addr = strtoul(readSerialCommand().c_str(), NULL, 16);

    Serial.print(" Enter number of bytes (hex): ");
    uint32_t numBytes = strtoul(readSerialCommand().c_str(), NULL, 16);

    Serial.print(" Enter the waiting time before/after acquisition: ");
    int wait = strtoul(readSerialCommand().c_str(), NULL, 10);

    pinMode(13, OUTPUT);
    for (int i = 0;i < wait; i++){
      digitalWrite(13, HIGH);
      delay(500);
      digitalWrite(13, LOW);
      delay(500);
    }

    init_default_SPI();

    uint32_t bytesRead = 0;

    while (bytesRead < numBytes) {
        //Se i byte rimanenti sono meno di 256.
        uint32_t chunkSize = min((uint32_t)BUFFER_SIZE, numBytes - bytesRead);

        // Inizio lettura SPI Flash
        digitalWrite(CS_FLASH, LOW);
        SPI.transfer(0x03); // Comando Read Data
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);

        // Attendere XON prima di inviare i dati
        while (Serial.available() && Serial.read() == XOFF) {
            delayMicroseconds(10); // Aspetta XON
        }

        // Lettura e invio dei dati via UART
        for (uint32_t i = 0; i < chunkSize; i++) {
            byte data = SPI.transfer(0x00);
            Serial.write(data);
        }

        digitalWrite(CS_FLASH, HIGH);

        // Aggiornamento indici
        bytesRead += chunkSize;
        addr += chunkSize;
    }
}

void setSPI() {
    Serial.println("Default: 2000000Hz - MSBFIRST - SPI_MODE0\n");
    Serial.print("Enter SPI Clock (max 20000000 Hz): ");
    clkSPI = readSerialCommand().toInt();

    Serial.print("Enter SPI Bit Order (0=LSBFIRST, 1=MSBFIRST): ");
    int bitOrderInput = readSerialCommand().toInt();
    bitOrder = (bitOrderInput == 0) ? LSBFIRST : MSBFIRST; 

    Serial.print("Enter SPI Mode (0, 1, 2, 3): ");
    int spiModeInput = readSerialCommand().toInt();
    
    switch (spiModeInput) {
        case 0: spiMode = SPI_MODE0; break;
        case 1: spiMode = SPI_MODE1; break;
        case 2: spiMode = SPI_MODE2; break;
        case 3: spiMode = SPI_MODE3; break;
        default:
            Serial.println("Invalid mode");
            return;
    }
    SPI.begin();
    SPI.beginTransaction(SPISettings(clkSPI, bitOrder, spiMode));
    
    hardwareRST();
}

void init_default_SPI() {
    SPI.begin();
    SPI.beginTransaction(SPISettings(clkSPI, bitOrder, spiMode));
    pinMode(CS_FLASH, OUTPUT);
    digitalWrite(CS_FLASH, HIGH);
    pinMode(RST_FLASH, OUTPUT);
    hardwareRST();
}

void customSPITransaction() {
    Serial.print("Enter Write Byte (hex, e.g. opcode): ");
    uint8_t writeByte = strtoul(readSerialCommand().c_str(), NULL, 16);

    Serial.print("How many dummy bytes? ");
    uint8_t numDummyBytes = readSerialCommand().toInt();


    Serial.print("Dummy byte value (0=0x00, 1=0xFF): ");
    uint8_t dummyType = readSerialCommand().toInt();
    uint8_t dummyByte = (dummyType == 1) ? 0xFF : 0x00;
   
    Serial.print("How many bytes do you want to read? ");
    uint32_t numReadBytes = readSerialCommand().toInt();

    // Inizio Transazione SPI
    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(writeByte);  // Invia l'OpCode o Write Byte
    
    // Invio dummy bytes se necessario
    for (uint8_t i = 0; i < numDummyBytes; i++) {
        SPI.transfer(dummyByte);
    }

    // Lettura risposta
    Serial.print("Response: ");
    for (uint32_t i = 0; i < numReadBytes; i++) {
        uint8_t data = SPI.transfer(0x00);
        Serial.print(data < 16 ? "0" : "");
        Serial.print(data, HEX);
        Serial.print(" ");
    }
    
    digitalWrite(CS_FLASH, HIGH);
}


void receiveXmodem() {
    uint8_t buffer[XMODEM_BUFFER_SIZE];  // Buffer per i dati
    bool receiving = true;
    uint32_t fileSize = 0;
    uint32_t bytesWritten = 0; // Numero di byte già scritti in flash
    uint8_t lastBlockNum = 0;
    uint8_t retryCount = 0;
    const uint8_t MAX_RETRIES = 10;
    // Chiedi la dimensione del file
    Serial.println(" Opcode 02h");
    Serial.print("Enter file size (hex): ");
    fileSize = strtoul(readSerialCommand().c_str(), NULL, 16);


    Serial.print("Enter start address (hex): ");
    flashAddress = strtoul(readSerialCommand().c_str(), NULL, 16);
    

    while (!Serial.available()) {
      Serial.write(NAK);  // Segnala che Arduino è pronto
      delay(500);
    }
    execStart = millis();    

    while (receiving) {
        if (!Serial.available()) continue;  

        uint8_t header = Serial.read();

        if (header == SOH) { // Start of Header 128-byte packet
            uint8_t blockNum = Serial.read();
            uint8_t blockNumInv = Serial.read();
            Serial.readBytes(buffer, XMODEM_BUFFER_SIZE); // Legge 128 byte
            uint8_t checksum = Serial.read(); // Riceve checksum       

            // Verifica il numero di blocco
            if (blockNum != (uint8_t)~blockNumInv) {
                Serial.write(NAK); // Errore, richiedi il blocco
                retryCount++;
                if (retryCount >= MAX_RETRIES) {
                    Serial.println("Too many errors. Aborting.");
                    Serial.write(CAN);
                    receiving = false;
                }
                continue;
            }

            // Se il blocco è duplicato, inviamo comunque ACK e lo ignoriamo
            if (blockNum == lastBlockNum) {
                Serial.write(ACK);
                continue;
            }
            
            // Calcola checksum sommando tutti i bit, va in overflow unit8
            // ovviamente fa parte del calcolo del chechsum.
            uint8_t calcChecksum = 0;
            for (int i = 0; i < XMODEM_BUFFER_SIZE; i++) {
                calcChecksum += buffer[i];
            }

            if (calcChecksum != checksum) {
                Serial.write(NAK);
                retryCount++;
                if (retryCount >= MAX_RETRIES) {
                    Serial.println("Too many errors. Aborting.");
                    Serial.write(CAN);
                    receiving = false;
                }
                continue;
            }
            
            // Determina quanti byte scrivere
            uint32_t bytesToWrite = XMODEM_BUFFER_SIZE;
            if (bytesWritten + XMODEM_BUFFER_SIZE > fileSize) {
                bytesToWrite = fileSize - bytesWritten;
            }      

            // Scrivi in flash solo i byte validi
            writeEnable();
            writePage(flashAddress, (char*)buffer, bytesToWrite);
            waitForWriteCompletion();
            flashAddress += bytesToWrite;
            bytesWritten += bytesToWrite;

            lastBlockNum = blockNum;  // Salva l'ultimo blocco ricevuto
            retryCount = 0;  // Reset contatore errori

            Serial.write(ACK);  // Conferma ricezione del pacchetto
        }

        else if (header == EOT) { // End of Transmission
            Serial.write(ACK);
            Serial.println("XMODEM Transfer Completed!");
            Serial.println("File saved into SPI NOR!");
            Serial.print("Total bytes written: ");
            Serial.println(bytesWritten);   
            execStop = millis();
            Serial.print("\nExec. : ");Serial.print(execStop - execStart);Serial.println("ms");                  
            receiving = false;
        }

        else if (header == CAN) { // Cancellazione da parte del trasmettitore
            Serial.println("XMODEM Transfer Canceled.");
            receiving = false;
        }
    }
}

void sendXmodem() {
    uint8_t buffer[XMODEM_BUFFER_SIZE] = {0};  
    uint32_t fileSize = 0;
    uint32_t bytesSent = 0;  
    uint8_t blockNum = 1;
    uint8_t fillByte = 0xFF;  // Default: 0xFF

    Serial.println(" Opcode 03h");
    Serial.print("Enter start address (hex): ");
    uint32_t startAddress = strtoul(readSerialCommand().c_str(), NULL, 16);

    Serial.print("Enter file size (hex): ");
    fileSize = strtoul(readSerialCommand().c_str(), NULL, 16);

    // Controlla se il file è multiplo di 128 byte
    if (fileSize % XMODEM_BUFFER_SIZE != 0) {
        Serial.println("\nWarning: File size is not a multiple of 128 bytes.");
        Serial.print("Enter padding byte (hex, default 1A): ");
        String fillByteStr = readSerialCommand();
        if (fillByteStr.length() > 0) {
            fillByte = strtoul(fillByteStr.c_str(), NULL, 16);
        }
    }

    Serial.println("Waiting for receiver...");

    // Attendi il NAK iniziale
    while (Serial.available() == 0);
    uint8_t response = Serial.read();
    if (response != NAK) {
        Serial.println("Receiver not ready. Aborting.");
        return;
    }

    Serial.println("Sending XMODEM data...");
    execStart = millis();

    while (bytesSent < fileSize) {
        uint32_t bytesToSend = min((uint32_t)XMODEM_BUFFER_SIZE, fileSize - bytesSent);


        // **Leggi i dati dalla memoria SPI NOR**
        digitalWrite(CS_FLASH, LOW);
        SPI.transfer(0x03);  
        SPI.transfer((startAddress >> 16) & 0xFF);
        SPI.transfer((startAddress >> 8) & 0xFF);
        SPI.transfer(startAddress & 0xFF);
        for (uint32_t i = 0; i < bytesToSend; i++) {
            buffer[i] = SPI.transfer(0x00);
        }
        digitalWrite(CS_FLASH, HIGH);

        // **Se l'ultimo pacchetto è inferiore a 128 byte, riempi con il valore scelto**
        for (uint32_t i = bytesToSend; i < XMODEM_BUFFER_SIZE; i++) {
            buffer[i] = fillByte;
        }

        // **Calcola checksum**
        uint8_t checksum = 0;
        for (uint32_t i = 0; i < XMODEM_BUFFER_SIZE; i++) {
            checksum += buffer[i];
        }

        // **Invia pacchetto sempre di 128 byte**
        Serial.write(SOH);
        Serial.write(blockNum);
        Serial.write(~blockNum);
        Serial.write(buffer, XMODEM_BUFFER_SIZE);
        Serial.write(checksum);

        // **Aspetta ACK o NAK**
        while (Serial.available() == 0);
        response = Serial.read();
        if (response == ACK) {
            startAddress += bytesToSend;
            bytesSent += bytesToSend;
            blockNum++;
        } else if (response == NAK) {
            Serial.println("NAK received, resending block...");
            continue;
        } else {
            Serial.println("Transmission aborted by receiver.");
            return;
        }
    }

    // **Invia EOT finché non riceve ACK**
    do {
        Serial.write(EOT);
        delay(100);
        while (Serial.available() == 0);
        response = Serial.read();
    } while (response != ACK);

    Serial.println("XMODEM Transfer Completed!");
    Serial.print("Total bytes sent: ");
    Serial.println(bytesSent);
    execStop = millis();
    Serial.print("\nExec. time: "); Serial.print(execStop - execStart); Serial.println("ms");
}

//LECTERATURE
/*
-----.c_str()
.c_str(): Questo metodo della classe std::string restituisce un puntatore a una stringa C 
(un array di caratteri terminato con un carattere nullo) che contiene il contenuto della stringa.

strtoul(..., NULL, 16): La funzione strtoul converte la stringa in un numero intero non firmato lungo 
(unsigned long). NULL(ASCII) corrisponde a  \0 in C 
15
-----Esempio 1: Conversione di una stringa esadecimale
    const char *hexStr = "1A3F";
    char *endptr;
    unsigned long value;

    value = strtoul(hexStr, &endptr, 16); //base 16

    printf("Valore convertito: %lu\n", value);  // Output: 6719
    printf("Stringa rimanente: %s\n", endptr);  // Output: (una stringa vuota)

//DEbug
 uint32_t indirizzo = 0x00000;
 char dati[] = "Hello, world!";
 int lunghezza = sizeof(dati) / sizeof(dati[0]);
 writeEnable();
 writePage(indirizzo, dati, lunghezza);
 erase
 blockErase(0,4096,1);

//XMODEM PACKET
+------+------------------------+--------------------------------------------------+
| Byte | Nome                   | Descrizione                                      |
+------+------------------------+--------------------------------------------------+
| 1    | SOH (0x01)             | Inizio di un blocco da 128 byte                  |
| 2    | Numero Blocco          | Numero progressivo del blocco (da 0x01 a 0xFF)   |
| 3    | Numero Blocco Invertito| Complemento a 255 del Numero Blocco (~Numero)    |
| 4-131| Dati (128 byte)        | Dati trasmessi nel pacchetto                     |
| 132  | Checksum (1 byte)      | Somma modulo 256 dei 128 byte dati               |
+------+------------------------+--------------------------------------------------+

//OLD readFlash senza ASCII print
void readFlash(uint32_t addr, uint32_t numByte) {
    const int bytesPerRow = 16; // Stampa massimo 16 byte per riga
    uint32_t bytesToRead = numByte;

    Serial.println(" Reading SNOR:");

    digitalWrite(CS_FLASH, LOW);
    SPI.transfer(0x03); // Comando Read Data

    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    for (uint32_t i = 0; i < bytesToRead; i++) {
        if (i % bytesPerRow == 0) {
            Serial.print("\n0x");
            Serial.print(addr + i, HEX);
            Serial.print(": ");
        }

        byte data = SPI.transfer(0x00);
       

        Serial.print(data < 16 ? "0" : "");
        Serial.print(data, HEX);
        Serial.print(" ");
    }

    digitalWrite(CS_FLASH, HIGH);
    Serial.println("\n Read complete!");
}

*/
