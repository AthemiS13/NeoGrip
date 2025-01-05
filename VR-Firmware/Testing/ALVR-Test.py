import requests
import keyboard
import time

# Define the URL
url = "http://192.168.0.28:8082/api/set-buttons"

# Function to send all button states
def send_all_button_states(trigger_value, grab_value):
    data = [
        {
            "path": "/user/hand/right/input/thumbstick/x",
            "value": {"Scalar": trigger_value}  # Trigger (spacebar)
        },
        {
            "path": "/user/hand/right/input/thumbstick/y",
            "value": {"Scalar": grab_value}  # Grab (Ente r)
        }
    ]
    try:
        response = requests.post(url, json=data)
        if response.status_code == 200:
            print(f"Data sent successfully: Trigger={trigger_value}, Grab={grab_value}")
        else:
            print(f"Failed to send data. Status code: {response.status_code}, Response: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"An error occurred: {e}")

# Main loop to send button states periodically
try:
    print("Press the spacebar to simulate trigger, press Enter to simulate grab. Press Ctrl+C to exit.")

    trigger_state = 0.0  # Initial trigger state
    grab_state = 0.0    # Initial grab state
    update_interval = 0.01  # Periodic update interval (10 ms)

    while True:
        # Update the state of each button
        if keyboard.is_pressed('space'):
            trigger_state = 1.0  # Spacebar pressed
        else:
            trigger_state = 0.0  # Spacebar released

        if keyboard.is_pressed('enter'):
            grab_state = -1.0  # Enter pressed
        else:
            grab_state = 0.0  # Enter released

        # Send all states
        send_all_button_states(trigger_state, grab_state)

        # Wait for the next update
        time.sleep(update_interval)
except KeyboardInterrupt:
    print("\nProgram terminated.")
