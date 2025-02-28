import socket
import threading
import requests
import time
import json
import websocket
from queue import Queue

# ALVR Server konfigurace
ALVR_URL = "http://192.168.0.30:8082/api/set-buttons"
SEND_INTERVAL = 0  # 20ms interval pro lepší odezvu
BUFFER_SIZE = 1024   # UDP buffer

# Porty pro ovladače
CONTROLLER_PORTS = {"left": 8888, "right": 8889}

# Sdílené proměnné
state_queue = Queue()
last_sent_data = {"left": None, "right": None}

# ESP32 Haptic konfigurace – IP bude resolvováno přes mDNS
ESP32_PORTS = {"left": 8888, "right": 8891}

# UDP socket pro haptiku
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Persistentní HTTP session pro ALVR
session = requests.Session()

def resolve_esp_mdns(hostname="neogrip.local"):
    try:
        return socket.gethostbyname(hostname)
    except Exception as e:
        print(f"[ERROR] MDNS resolution selhalo pro {hostname}: {e}")
        return "192.168.0.16"  # fallback

ESP32_IP = resolve_esp_mdns()

# Funkce pro odeslání stavu tlačítek a joysticku do ALVR serveru
def send_to_alvr(controller, data):
    print(f"[ALVR] Odesílám pro {controller}: {json.dumps(data, indent=2)}")
    try:
        response = session.post(ALVR_URL, json=data)
        if response.status_code != 200:
            print(f"[ALVR ERROR] {controller.upper()} - {response.status_code}: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"[ALVR ERROR] {controller.upper()} - {e}")

# Funkce pro zpracování UDP paketu z ESP
def process_packet(controller, packet):
    try:
        print(f"[ESP] Surová data z {controller}: {packet}")
        if len(packet) != 14:
            print(f"[ERROR] Neplatná délka paketu: {len(packet)}")
            return

        # Dekódování paketu
        button_primary     = int(packet[0])
        button_secondary   = int(packet[1])
        button_menu_or_sys = int(packet[2])
        button_TRIG        = int(packet[3])
        button_SQZ         = int(packet[4])
        joyX               = int(packet[5:9])
        joyY               = int(packet[9:13])
        button_JOYCLK      = int(packet[13])

        # Konverze do formátu kompatibilního s ALVR
        data = [
            {"path": f"/user/hand/{controller}/input/trigger/value", "value": {"Scalar": 0.8 if button_TRIG else 0.0}},
            {"path": f"/user/hand/{controller}/input/squeeze/value", "value": {"Scalar": 1.0 if button_SQZ else 0.0}},
            {"path": f"/user/hand/{controller}/input/{'x' if controller == 'left' else 'a'}/click", "value": {"Binary": button_primary == 1}},
            {"path": f"/user/hand/{controller}/input/{'y' if controller == 'left' else 'b'}/click", "value": {"Binary": button_secondary == 1}},
            {"path": f"/user/hand/{controller}/input/{'menu' if controller == 'left' else 'system'}/click", "value": {"Binary": button_menu_or_sys == 1}},
            {"path": f"/user/hand/{controller}/input/thumbstick/x", "value": {"Scalar": (joyX - 2047) / 2047.0}},
            {"path": f"/user/hand/{controller}/input/thumbstick/y", "value": {"Scalar": (joyY - 2047) / 2047.0}},
            {"path": f"/user/hand/{controller}/input/thumbstick/click", "value": {"Binary": button_JOYCLK == 1}},
        ]
        
        state_queue.put((controller, data))
    except ValueError as e:
        print(f"[ERROR] Chyba při dekódování paketu pro {controller}: {e}")

# Vlákno pro periodické odesílání změn do ALVR
def update_alvr():
    while True:
        if not state_queue.empty():
            controller, data = state_queue.get()
            if data != last_sent_data[controller]:
                send_to_alvr(controller, data)
                last_sent_data[controller] = data
        time.sleep(SEND_INTERVAL)

# --- Haptická komunikace ---

# Globální proměnná pro haptický příkaz (formát "DDDMMM" – duty cycle a doba v ms)
# Výchozí hodnota "vypnuto": duty 255, doba 000
current_haptic = "255000"
haptic_lock = threading.Lock()

def update_haptic_from_event(amplitude, duration_ms):
    # Převod amplitude (0.0–1.0) na duty (0–255); 255 znamená "vypnuto"
    duty = int(amplitude * 255)
    # Formátování na 3-cifernou hodnotu; duration omezeno na max 999 ms
    command = f"{duty:03d}{min(int(duration_ms), 999):03d}"
    with haptic_lock:
        global current_haptic
        current_haptic = command
    print(f"[HAPTIC] Nový příkaz: {command}")

def haptic_sender():
    while True:
        with haptic_lock:
            command = current_haptic
            # Po odeslání resetujeme na výchozí hodnotu (vypnuto)
            current_haptic = "255000"
        try:
            # Zde můžete rozlišovat ovladače – příklad posíláme levému ovladači
            sock.sendto(command.encode(), (ESP32_IP, ESP32_PORTS["left"]))
            print(f"[HAPTIC] Odesláno: {command}")
        except Exception as e:
            print(f"[HAPTIC ERROR] {e}")
        time.sleep(0.02)  # Odesílat každých 20 ms

# Zpracování zpráv z WebSocketu (ALVR eventy)
def on_message(ws, message):
    try:
        data = json.loads(message)
        event_data = data.get("event_type", {}).get("data", {})
        path = event_data.get("path", "")
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        if "/user/hand/left" in path:
            controller = "left"
        elif "/user/hand/right" in path:
            controller = "right"
        else:
            return

        if duration and amplitude is not None:
            duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)
            update_haptic_from_event(amplitude, duration_ms)
    except Exception as e:
        print(f"[ERROR] Zpracování zprávy WebSocket: {e}")

def start_websocket():
    ws = websocket.WebSocketApp("ws://localhost:8082/api/events", on_message=on_message)
    ws.run_forever()

# UDP listener pro ovladače
def listen_for_controller(controller, port):
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(('0.0.0.0', port))
    print(f"Naslouchám {controller} ovladači na portu {port}...")

    while True:
        try:
            data, addr = udp_socket.recvfrom(BUFFER_SIZE)
            packet = data.decode()
            print(f"Obdrženo od {controller} ({addr}): {packet}")
            if len(packet) == 15:  # Kontrola délky paketu
                process_packet(controller, packet[1:])  # Přeskočíme identifikátor ovladače
            else:
                print(f"Neplatná délka paketu z {addr}: {len(packet)}")
        except Exception as e:
            print(f"[ERROR] Listener {controller}: {e}")
            break
    udp_socket.close()

def main():
    try:
        threading.Thread(target=update_alvr, daemon=True).start()
        threading.Thread(target=start_websocket, daemon=True).start()
        threading.Thread(target=haptic_sender, daemon=True).start()
        for controller, port in CONTROLLER_PORTS.items():
            threading.Thread(target=listen_for_controller, args=(controller, port), daemon=True).start()
        print("[SYSTEM] Proxy server běží. Pro ukončení stiskněte Ctrl+C.")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[SYSTEM] Ukončuji proxy server...")

if __name__ == "__main__":
    main()