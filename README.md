# Arduino-UART-SPI-NOR-testbench
**An Arduino firmware to read, write, erase, and transfer files to SPI NOR Flash via UART.**

## Features
- **SPI NOR Flash Memory Control**  
  -  Erase specific blocks (4KB, 32KB, 64KB)  
  -  Write and read single bytes or pages  
  -  Chip erase (full memory wipe)  
  -  Read device ID and status registers  
  -  Configurable SPI settings ([Hardware Setup](#example))  
  -  Comaptible with x25xx Spi Nor flash

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

### **Connections**  
| **SPI Signal** | **Arduino Minima R4 Pin** |  
|--------------|--------------|  
| **MISO (CIPO)** | **D11** |  
| **MOSI (COPI)** | **D12** |  
| **SCK** | **D13** |  
| **CS (Chip Select)** | **D10** |  
| **RST (Optional Reset)** | **D9** |  

## Supported Commands  

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
 - Read Flash '4'
   ![image](https://github.com/user-attachments/assets/88f47708-c48a-4194-8c03-36aa8c0f4a23)

- Reads data from UART and writes it to flash, remember erase flash!!! '9'

  ![image](https://github.com/user-attachments/assets/c110c573-0ce3-4b9f-b37d-77a5d0c29647)

  ![image](https://github.com/user-attachments/assets/fb817c43-2e45-4a90-8ebf-b05d22a5be97)

-  CRC32 calc '10'
  
   ![image](https://github.com/user-attachments/assets/9bcf0cf3-fa83-4708-980f-3303859be8fd)



  





## Hardware connection 
![a](https://github.com/user-attachments/assets/4a8a9036-b9ae-4f12-9ee3-b4c323d49c8b)
![aas](https://github.com/user-attachments/assets/5d469e7a-bb47-4316-a73b-4eeb28fa0461)



### **Clone the Repository**  
```sh
git clone https://github.com/Jampag/UART_to_SPI-NOR.git
