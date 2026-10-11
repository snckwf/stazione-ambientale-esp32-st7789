STAZIONE AMBIENTALE V4

Funzioni aggiunte:
- APDS-9930 reintegrato solo per luminosita ambiente (niente gesti)
- storico dati con buffer circolare (60 campioni)
- mini grafici sul display
- soglie personalizzate con allarmi
- export via rete: /api/data /api/history /api/alerts /api/export.csv
- integrazione Home Assistant pronta con snippet YAML su /api/ha_yaml
- doppio bus I2C mantenuto

Bus I2C:
- I2C0 SDA=21 SCL=22 -> ENS160 + AHT21 + APDS-9930
- I2C1 SDA=16 SCL=17 -> BMP280 + AHT20 (si usa solo BMP280)

Display ST7789:
- CS=5
- DC=27
- RST=26
- SCK=18
- MOSI=23
- VCC=3V3
- GND=GND

Librerie Arduino IDE:
- Adafruit AHTX0
- Adafruit Unified Sensor
- Adafruit BMP280 Library
- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- DFRobot_ENS160

Home Assistant:
- apri /api/ha_yaml e copia il blocco YAML in configuration.yaml
- riavvia Home Assistant
- usa /api/alerts per automazioni e notifiche personalizzate


APDS-9930:
- indirizzo I2C 0x39 sul bus I2C0
- usato solo per la luminosita ambiente
- nessuna prossimita o gesture nel firmware
- il dimmer AUTO puo usare le soglie lux configurabili da pagina web
