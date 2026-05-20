

import sys
import time
import serial
import serial.tools.list_ports

from Adafruit_IO import MQTTClient

# ─────────────────────────────────────────
# CONFIGURACIÓN
# ─────────────────────────────────────────
ADAFRUIT_IO_USERNAME = "Samdragunov"
ADAFRUIT_IO_KEY      = "aio_rGgD22tDcbSvjKeFEHHGjeKLMLRS"

PUERTO_SERIAL = "COM4"
BAUD_RATE     = 9600

GRUPO = "proyecto"

FEED_SERVO0  = f"{GRUPO}.servo0"
FEED_SERVO1  = f"{GRUPO}.servo1"
FEED_SERVO2  = f"{GRUPO}.servo2"
FEED_SERVO3  = f"{GRUPO}.servo3"
FEED_MANUAL  = f"{GRUPO}.manual"
FEED_EEPROM  = f"{GRUPO}.eeprom"
FEED_UART    = f"{GRUPO}.uart"
FEED_GUARDAR = f"{GRUPO}.guardar"
FEED_SLOT    = f"{GRUPO}.slot"
FEED_STATUS  = f"{GRUPO}.status"

MAPA_MODO_FEED = {
    "manual": "MODE:M",
    "eeprom": "MODE:E",
    "uart":   "MODE:U",
}

# ─────────────────────────────────────────
# SERIAL
# ─────────────────────────────────────────
def conectar_serial():
    try:
        arduino = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=1)
        time.sleep(2)
        print(f"[Serial] Conectado a {PUERTO_SERIAL} a {BAUD_RATE} baud")
        return arduino
    except serial.SerialException:
        print(f"[Error] No se pudo abrir {PUERTO_SERIAL}")
        print("[Info] Puertos disponibles:")
        for p in serial.tools.list_ports.comports():
            print(f"   {p.device}  —  {p.description}")
        sys.exit(1)

arduino = conectar_serial()

def enviar_comando(comando):
    if not comando.endswith('\n'):
        comando += '\n'
    arduino.write(comando.encode())
    print(f"[Arduino] Enviado: {comando.strip()}")

# Último valor publicado al feed status — evita publicar duplicados
ultimo_status_publicado = ""

def leer_respuesta(client):
    """
    Lee una línea del Arduino si hay datos disponibles.
    Solo publica al feed status si el mensaje es relevante
    y distinto al último publicado (evita desperdiciar rate).
    """
    global ultimo_status_publicado

    if arduino.in_waiting > 0:
        linea = arduino.readline().decode('utf-8', errors='ignore').strip()
        if not linea:
            return

        print(f"[Arduino] Respuesta: {linea}")

        # Solo publicar a Adafruit si es un mensaje útil
        # y no es el mismo que ya se publicó antes
        if linea != ultimo_status_publicado:
            try:
                client.publish(FEED_STATUS, linea)
                ultimo_status_publicado = linea
            except Exception as e:
                print(f"[Error] No se pudo publicar a status: {e}")

# ─────────────────────────────────────────
# CALLBACKS
# ─────────────────────────────────────────
def connected(client):
    print("[Adafruit] Conectado a Adafruit IO")

    feeds = [FEED_SERVO0, FEED_SERVO1, FEED_SERVO2, FEED_SERVO3,
             FEED_MANUAL, FEED_EEPROM, FEED_UART,
             FEED_GUARDAR, FEED_SLOT]

    for feed in feeds:
        client.subscribe(feed)
        print(f"[Adafruit] Suscrito a: {feed}")

    enviar_comando("MODE:U")
    print("[Info] Sistema listo.\n")

def disconnected(client):
    print("[Adafruit] Desconectado")
    sys.exit(1)

def message(client, feed_id, payload):
    print(f"[Adafruit] Feed: {feed_id}  Valor: {payload}")

    nombre = feed_id.split(".")[-1]

    # ── Servos ──
    if nombre in ("servo0", "servo1", "servo2", "servo3"):
        numero = nombre[-1]
        try:
            angulo = int(float(payload))
            angulo = max(0, min(180, angulo))
            enviar_comando(f"A{numero}:{angulo}")
        except ValueError:
            print(f"[Error] Valor inválido: {payload}")

    # ── Modos ──
    elif nombre in ("manual", "eeprom", "uart"):
        try:
            if int(float(payload)) == 1:
                enviar_comando(MAPA_MODO_FEED[nombre])
            # silencio total al soltar — no imprimir nada para mantener log limpio
        except ValueError:
            pass

    # ── Guardar ──
    elif nombre == "guardar":
        try:
            if int(float(payload)) == 1:
                enviar_comando("SAVE:0")
        except ValueError:
            pass

    # ── Slot ──
    elif nombre == "slot":
        try:
            slot = int(float(payload))
            slot = max(0, min(9, slot))
            enviar_comando(f"PLAY:{slot}")
        except ValueError:
            print(f"[Error] Slot inválido: {payload}")

# ─────────────────────────────────────────
# CONEXIÓN
# ─────────────────────────────────────────
client = MQTTClient(ADAFRUIT_IO_USERNAME, ADAFRUIT_IO_KEY)
client.on_connect    = connected
client.on_disconnect = disconnected
client.on_message    = message

print("[Adafruit] Conectando...")
client.connect()
client.loop_background()

# ─────────────────────────────────────────
# LAZO PRINCIPAL
# Solo lee respuestas del Arduino — no envía nada por cuenta propia
# ─────────────────────────────────────────
print("[Info] Corriendo. Ctrl+C para salir.\n")

try:
    while True:
        leer_respuesta(client)
        time.sleep(0.1)

except KeyboardInterrupt:
    print("\n[Info] Cerrando...")
    arduino.close()
    sys.exit(0)