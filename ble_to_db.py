import asyncio  
from bleak import BleakClient  # BLE-klientbibliotek för kommunikation med Bluetooth Low Energy-enheter
import mysql.connector  # Bibliotek för att ansluta till och hantera en MySQL-databas
from datetime import datetime  # Används för att skapa tidsstämplar
import serial  # Bibliotek för att kommunicera med Arduino via seriell port


database_config = {
    'host': 'localhost',       # Databasens värdnamn
    'user': 'user_sensor',     # Användarnamn 
    'password': 'pass123',     # Lösenord för användare 
    'database': 'sensor_readings'  # Namn på databasen
}

# BLE-konfiguration 
BLE_DEVICE_ADDRESS = "E8:78:20:78:84:EC"  # Adress till BLE-enheten (nRF5340)
HR_UUID = "2A37"  # UUID för Heart Rate Measurement

# Arduino-konfiguration
SERIAL_PORT = '/dev/ttyACM0'  # Port för att ansluta till Arduino
BAUD_RATE = 9600  # Baud-rate för kommunikation med Arduino

# Buffert för att samla data innan den skrivs till databasen
sensor_data_buffer = []  # lagrar hjärtfrekvensvärden
MAX_BUFFER_SIZE = 10  # Maxstorlek för bufferten 

# Funktion för att initiera databasanslutningen
def initialize_database():
    connection = mysql.connector.connect(**database_config)  # Anslut till databasen med konfigurationen
    db_cursor = connection.cursor()  # Skapa en (cursor) för att köra SQL-frågor
    return connection, db_cursor  # Returnera anslutningen och pekaren

# skickar värde till arduino
def send_to_arduino(data_value):
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as arduino: #öppna connection med arduino 
            cmd = f"SEND {data_value}\n"  # Skapa ett kommando för att skicka värdet
            arduino.write(cmd.encode())  # Skicka kommandot till Arduino
            while True:
                response = arduino.readline().decode().strip()  # Läs svaret från Arduino
                if response:  # Om svaret inte är tomt
                    print(f"Arduino svarade: {response}")  # Skriv ut svaret
                else:
                    break  
    except Exception as error:
        print(f"Fel vid kommunikation med Arduino: {error}")  # Hantera fel vid kommunikation

# Asynkron funktion för att hantera inkommande hjärtfrekvensdata från NRF
async def process_heart_rate(sender, packet):
    global sensor_data_buffer  # Använd den globala bufferten
    heart_rate = int.from_bytes(packet, byteorder='little')  # Konvertera bytepaketet till ett heltal (hjärtfrekvensvärde)
    
    # Skicka hjärtfrekvensvärdet till Arduino
    send_to_arduino(str(heart_rate))
    
    # lägg till värdet i buffert 
    sensor_data_buffer.append((datetime.now(), heart_rate))
    
    # om buffert är full  skriv data till databasen
    if len(sensor_data_buffer) >= MAX_BUFFER_SIZE:
        try:
            db_connection, db_cursor = initialize_database()  # Initiera databasanslutningen
            insert_query = "INSERT INTO heart_rate (timestamp, value) VALUES (%s, %s)"  # SQL-fråga för att lägga till data
            db_cursor.executemany(insert_query, sensor_data_buffer)  # Kör frågan för varje post i bufferten
            db_connection.commit()  # Spara ändringarna i databasen
            print(f"{len(sensor_data_buffer)} rader skrivna till databasen")  # Skriv ut antal skrivna rader
            sensor_data_buffer = []  # Töm buffert efter att den är klar
        except Exception as db_error:
            print(f"Databasfel: {db_error}")  # Hantera databasfel
        finally:
            db_cursor.close()  
            db_connection.close()  

# Huvudfunktion för att ansluta till BLE-enheten och börja ta emot hjärtfrekvensdata
async def run_ble_client():
    try:
        # Skapa en BLE-klient och anslut till den angivna BLE-enheten
        async with BleakClient(BLE_DEVICE_ADDRESS) as ble_client:
            print("Ansluten till nRF5340")  # Bekräfta anslutning
            await ble_client.start_notify(HR_UUID, process_heart_rate)  # Börja ta emot notifikationer för hjärtfrekvens
            await asyncio.sleep(float('inf'))  # tar emot data 
    except Exception as ble_error:
        print(f"BLE-fel: {ble_error}")  # Hantera fel vid BLE-anslutning

# Programstart
if __name__ == "__main__":
    asyncio.run(run_ble_client())  # Kör den asynkrona huvudfunktionen
