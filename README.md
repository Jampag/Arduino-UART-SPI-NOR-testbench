# Arduino-UART-SPI-NOR-testbench
**An Arduino firmware to read, write, erase, and transfer files to SPI NOR Flash via UART.**

## Features
- **SPI NOR Flash Memory Control**  
  -  Only support 1x SPI
  -  Erase specific blocks (4KB, 32KB, 64KB)  
  -  Write and read single bytes or pages  
  -  Chip erase (full memory wipe)  
  -  Read device ID and status registers  
  -  Configurable SPI settings 
  -  Compatible with 25-series SPI NOR Flash (e.g., Winbond, Macronix, SST, Microchip)
    [OPCODE LIST](#supported-commands)

- **Data Integrity & Transfers**  
  -  **CRC32 Calculation** to verify memory integrity  
  -  **Dump Flash Memory to UART** (for logging or backups)  
  -  **Custom SPI Transactions** (for example costum OPCODE)  
  -  **Receive files via XMODEM** and store them in SPI NOR Flash  
  -  **Send files via XMODEM** from Flash to UART  

- **Hardware Control**  
  -  Software & hardware reset support  

##  Hardware Setup  
### Supported Boards  
- **Arduino Minima R4**  
- Any board level translator bi-directional
- SPI-NOR Flash 25-series 

### **Connections**  
| **SPI Signal** | **Arduino Minima R4 Pin** |  
|--------------|--------------|  
| **MISO (CIPO)** | **D11** |  
| **MOSI (COPI)** | **D12** |  
| **SCK** | **D13** |  
| **CS (Chip Select)** | **D10** |  
| **RST (Optional Reset)** | **D9** |  

## Supported Commands  
See in a your SPI-NOR datasheet  if are compatible OPCODE with list below

| **Command** | **Description** | **Opcode** |  
|------------|---------------|----------|  
| `1` | Block Erase (4KB, 32KB, 64KB) | `20h / 52h / D8h` | 
| `2` | writeEnable | `06h` WREN |
| `3` | chipErase | `C7h` |
| `4` | readFlash | `03h` RDSR| 
| `5` | statusRegister | `05h` RDSR |
| `6` | writeData | `02h` PP |
| `7` | deviceID  | `90h` REMS |
| `8` | deviceID  | `9Fh` RDID |
| `9` | Reads data from UART and writes it to flash | `02h` |
| `10` | CRC32 calc | `03h` |
| `11` | HW Reset cylce | - | 
| `12` | Dump Flash to UART | `03h` |
| `13` | SPI setting | - |
| `14` | Custom SPI Transaction | - |
| `15` | Receive file XMODEM and write into flash | `02h` |
| `16` | Send file XMODEM | `3h` |

## Example
- **Read Flash ('4')**  
    1. Enter the start address (hex).  
    2. Enter the number of bytes to read (hex).  
    3. Data is displayed in hexadecimal format.  
        - The output format is `0xADDR: DATA DATA DATA`  
        - Each row contains 16 bytes. The right image is Notepad++ Hex view 
   ![image](https://github.com/user-attachments/assets/88f47708-c48a-4194-8c03-36aa8c0f4a23)

- **Reads data from UART and writes it to flash, remember erase flash!!! ('9')**
    1. Enter the starting address where you want to start writing (hex).

       ![image](https://github.com/user-attachments/assets/ba5357ce-ad12-4857-adec-9d6e20333ed9)

    
    2. Configure Teraterm to send binary file per size, do not send any characters from the keyboard

       ![image](https://github.com/user-attachments/assets/072262a1-724d-4cf9-a430-9df3006b676c)

    3. The file transfer will start
 
       ![image](https://github.com/user-attachments/assets/06130826-bd19-4df3-8ba9-95b17a83e5de)

    4. At the end the file size must match the size indicated.

       ![image](https://github.com/user-attachments/assets/b1403d5c-4ac2-4982-b977-288c62c75188)


- **CRC32 calc ('10')**
  1. Enter the starting address where you want to start cal CRC32 (hex).
  2. Enter the number of byte ugual to size file (hex) in this example 2192112byte is 2172F0
  3. Compare CRC32 file on your pc using 7zip(or similar)

     ![image](https://github.com/user-attachments/assets/fdadc027-9cb6-436b-af5f-c32d21d3328f)




  





## Hardware connection 
![a](https://github.com/user-attachments/assets/4a8a9036-b9ae-4f12-9ee3-b4c323d49c8b)
![aas](https://github.com/user-attachments/assets/5d469e7a-bb47-4316-a73b-4eeb28fa0461)



### **Clone the Repository**  
```sh
git clone https://github.com/Jampag/UART_to_SPI-NOR.git
