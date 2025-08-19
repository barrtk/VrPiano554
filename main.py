import os
import sys
import pygame
import mido
import socket
import json
import time
import threading
import pretty_midi

# Path to your MIDI file
MIDI_FILE_PATH = r"C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\Source\VrPiano554\midi\Promise.mid"
POSITION_FILE_PATH = r"C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\piano_position.json"

# --- CRITICAL FIX FOR portmidi.dll DEPENDENCIES ---
try:
    pygame_path = os.path.dirname(pygame.__file__)
    os.environ['PATH'] = pygame_path + os.pathsep + os.environ['PATH']
    print(f"INFO: Successfully added Pygame directory to PATH to find DLLs: {pygame_path}")
except Exception as e:
    print(f"WARNING: Could not add pygame path to PATH: {e}")

# Use portmidi backend for mido
os.environ.setdefault('MIDO_BACKEND', 'mido.backends.portmidi')

# --- Pygame Mixer Setup ---
def init_pygame_mixer():
    try:
        pygame.mixer.quit()
        pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=512)
        pygame.mixer.set_num_channels(64)
        print("INFO: pygame.mixer initialized successfully.")
    except Exception as e:
        print(f"WARNING: pygame.mixer init failed: {e}")
        try:
            pygame.mixer.quit()
            pygame.mixer.init()
            pygame.mixer.set_num_channels(64)
            print("INFO: pygame.mixer initialized with default parameters.")
        except Exception as e2:
            print(f"ERROR: pygame.mixer fallback init failed: {e2}")

init_pygame_mixer()

# --- Sound Loading ---
SOUND_DIR = r"C:\Users\Bartek\Desktop\VRpianoAPP\Clean_Grand_Mistral"
sounds = {}
try:
    for i in range(21, 109):
        sound_file = os.path.join(SOUND_DIR, f"Clean Grand Mistral - GDCGM {i:03d}L.wav")
        if os.path.exists(sound_file):
            sounds[i] = pygame.mixer.Sound(sound_file)
    print(f"SUCCESS: Loaded {len(sounds)} sounds.")
except Exception as e:
    print(f"ERROR: Could not load sounds: {e}")

# --- Network Setup ---
UDP_IP_SEND = "127.0.0.1"
UDP_PORT_SEND = 5005
send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

UDP_IP_RECEIVE = "127.0.0.1"
UDP_PORT_RECEIVE = 5006
receive_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receive_sock.bind((UDP_IP_RECEIVE, UDP_PORT_RECEIVE))

# --- Sound & State Management ---
muted_all = False
muted_live = False
muted_parser = True
log_live = True
log_parser = True
is_paused = False
volume = 0.5
speed_factor = 1.0
active_channels = {}
file_thread = None
note_off_timers = []
was_playing = False
live_hold_mode = False
loop_midi = False

# --- NEW: Wait-for-key-press (Practice Mode) ---
wait_for_key_mode = False
notes_to_wait_for = set()
key_pressed_event = threading.Event()

stop_event = threading.Event()
state_lock = threading.Lock()

def send_udp_message(message_dict):
    """Generic function to send any dictionary as a JSON UDP message."""
    try:
        json_message = json.dumps(message_dict)
        message_bytes = json_message.encode('utf-8') + b'\0'
        send_sock.sendto(message_bytes, (UDP_IP_SEND, UDP_PORT_SEND))
    except Exception as e:
        print(f"ERROR sending UDP message: {e}")

def send_midi_message(msg_type, note, velocity=None, duration=None, source="live"):
    global muted_all, muted_parser, speed_factor
    with state_lock:
        if muted_all:
            return

    message = {"type": msg_type, "note": int(note), "source": source}
    if velocity is not None:
        message["velocity"] = int(velocity)
    if duration is not None:
        try:
            adj_duration = float(duration) / speed_factor if source == "file" else float(duration)
            message["duration"] = adj_duration
        except Exception:
            pass
    send_udp_message(message)
    if (source == "live" and log_live) or (source == "file" and log_parser):
        print(f"Sent: {json.dumps(message)}")

def play_sound(note, source="live"):
    global muted_all, muted_live, volume
    with state_lock:
        if muted_all or (source == "live" and muted_live) or (source == "file" and muted_parser):
            return
    if note in sounds:
        channel = pygame.mixer.find_channel(True)
        if channel:
            channel.set_volume(volume)
            channel.play(sounds[note])
            active_channels[note] = channel

def stop_sound(note, source="live", force=False):
    global live_hold_mode, active_channels
    if note in active_channels:
        if not force and source == "live" and live_hold_mode:
            active_channels.pop(note, None)
            return
        try:
            active_channels[note].fadeout(1000)
            active_channels.pop(note, None)
        except (KeyError, Exception):
            pass

def midi_to_note_name(midi_note):
    if not 21 <= midi_note <= 108:
        return "Invalid Note"
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    note = note_names[midi_note % 12]
    octave = (midi_note // 12) - 1
    return f"{note}{octave}"

# ---------- MIDI FILE PLAYBACK ----------
def play_midi_file_prettymidi(file_path):
    global was_playing, is_paused, muted_all, muted_parser, note_off_timers, stop_event, speed_factor, wait_for_key_mode, notes_to_wait_for, key_pressed_event, loop_midi

    while not stop_event.is_set():
        try:
            pm = pretty_midi.PrettyMIDI(file_path)
        except Exception as e:
            print(f"ERROR: pretty_midi failed to load file: {e}")
            return

        all_notes = []
        for instrument in pm.instruments:
            all_notes.extend(instrument.notes)
        
        sorted_notes = sorted(all_notes, key=lambda note: note.start)

        start_t0 = time.time()
        last_start_time = 0.0
        print(f"[FilePlayback] Starting playback of {file_path} with {len(sorted_notes)} notes.")
        was_playing = True

        for note in sorted_notes:
            while is_paused and not stop_event.is_set():
                time.sleep(0.1)
                start_t0 = time.time() - (last_start_time / speed_factor)

            if stop_event.is_set() or muted_all:
                break

            wait_duration = note.start - last_start_time
            if wait_duration > 0 and not wait_for_key_mode:
                target_time = start_t0 + (note.start / speed_factor)
                to_sleep = target_time - time.time()
                if to_sleep > 0:
                    time.sleep(to_sleep)
            
            last_start_time = note.start

            if wait_for_key_mode:
                with state_lock:
                    notes_to_wait_for.clear()
                    notes_to_wait_for.add(note.pitch)
                
                send_udp_message({"type": "highlight_on", "notes": [note.pitch]})
                key_pressed_event.clear()
                notes_str = f"{midi_to_note_name(note.pitch)} ({note.pitch})"
                print(f"\033[93m[PRACTICE] Waiting for key: {notes_str}\033[0m")
                
                key_pressed_event.wait()
                if stop_event.is_set(): break

            duration = max(0.0, note.end - note.start)
            send_velocity = 1 if muted_parser else note.velocity
            send_midi_message("note_on", note.pitch, velocity=send_velocity, duration=duration, source="file")
            play_sound(note.pitch, source="file")
            send_udp_message({"command": "update_midi_info", "midi_info": f"File: {midi_to_note_name(note.pitch)}"})

            adj_duration = duration / speed_factor
            if adj_duration > 0:
                timer = threading.Timer(adj_duration, lambda n=note.pitch: send_midi_message("note_off", n, source="file"))
                note_off_timers.append(timer)
                timer.start()
            else:
                send_midi_message("note_off", note.pitch, source="file")
                stop_sound(note.pitch, source="file")

        if not loop_midi or stop_event.is_set():
            break

    for timer in note_off_timers:
        timer.cancel()
    note_off_timers.clear()
    print("[FilePlayback] Finished playback.")
    was_playing = False

def file_playback_thread(file_path):
    global file_thread
    if not os.path.exists(file_path):
        print(f"[FilePlayback] MIDI file not found: {file_path}")
        return
    try:
        play_midi_file_prettymidi(file_path)
    except Exception as e:
        print(f"[FilePlayback] Exception in playback thread: {e}")
    file_thread = None

def start_file_playback():
    global file_thread, was_playing
    if file_thread and file_thread.is_alive():
        restart_file_playback()
    else:
        print("Rozpoczeto odtwarzanie pliku MIDI.")
        init_pygame_mixer()
        file_thread = threading.Thread(target=file_playback_thread, args=(MIDI_FILE_PATH,), daemon=True)
        file_thread.start()
    was_playing = True

def restart_file_playback():
    global file_thread, active_channels, note_off_timers, was_playing, stop_event, key_pressed_event
    if file_thread and file_thread.is_alive():
        stop_event.set()
        key_pressed_event.set()
        file_thread.join(timeout=2.0)
        stop_event.clear()

    for note in list(active_channels.keys()): stop_sound(note, force=True)
    for timer in note_off_timers: timer.cancel()
    note_off_timers.clear()

    print("[FilePlayback] Restarting MIDI playback...")
    init_pygame_mixer()
    file_thread = threading.Thread(target=file_playback_thread, args=(MIDI_FILE_PATH,), daemon=True)
    file_thread.start()
    was_playing = True

# ---------- LIVE MIDI INPUT ----------
def live_midi_thread(preferred_port_substr="Arturia"):
    global muted_all, muted_live, log_live, stop_event, wait_for_key_mode, notes_to_wait_for, key_pressed_event
    try:
        input_ports = mido.get_input_names()
    except Exception as e:
        print(f"[LiveMIDI] Error getting MIDI input names: {e}")
        return

    if not input_ports: print("[LiveMIDI] No MIDI input devices found."); return

    print("\n[LiveMIDI] Available MIDI input devices:")
    for i, port in enumerate(input_ports): print(f"  {i}: {port}")

    selected_port = next((p for p in input_ports if preferred_port_substr in p and "MIDIOUT2" not in p), None)
    if not selected_port: selected_port = input_ports[0]
    print(f"[LiveMIDI] Using MIDI port: {selected_port}")

    try:
        with mido.open_input(selected_port) as inport:
            print(f"[LiveMIDI] Listening on {selected_port} ...")
            for msg in inport:
                if stop_event.is_set(): break
                
                is_note_on = msg.type == "note_on" and msg.velocity > 0
                is_note_off = msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0)

                if wait_for_key_mode and is_note_on:
                    with state_lock:
                        if msg.note in notes_to_wait_for:
                            udp_msg = {"type": "highlight_off", "notes": [msg.note]}
                            send_udp_message(udp_msg)
                            print(f"\033[94m[PRACTICE] Sent highlight_off for {midi_to_note_name(msg.note)} ({msg.note})\033[0m")
                            
                            notes_to_wait_for.remove(msg.note)
                            print(f"\033[92m[PRACTICE] Correct key: {msg.note}. Remaining: {len(notes_to_wait_for)}\033[0m")
                            if not notes_to_wait_for:
                                key_pressed_event.set()
                
                with state_lock:
                    if muted_all or muted_live: continue

                if is_note_on:
                    send_midi_message("note_on", msg.note, velocity=msg.velocity, source="live")
                    play_sound(msg.note, source="live")
                elif is_note_off:
                    send_midi_message("note_off", msg.note, source="live")
                    stop_sound(msg.note, source="live")
    except Exception as e:
        print(f"[LiveMIDI] Exception in MIDI input thread: {e}")

def start_live_midi():
    global live_thread, stop_event
    stop_event.clear()
    live_thread = threading.Thread(target=live_midi_thread, daemon=True)
    live_thread.start()

# ---------- Position Management ----------
def save_position(position):
    try:
        with open(POSITION_FILE_PATH, 'w') as f:
            json.dump(position, f)
        print(f"Position saved to {POSITION_FILE_PATH}")
    except Exception as e:
        print(f"ERROR saving position: {e}")

def load_position():
    try:
        if os.path.exists(POSITION_FILE_PATH):
            with open(POSITION_FILE_PATH, 'r') as f:
                position = json.load(f)
                print(f"Position loaded from {POSITION_FILE_PATH}")
                return position
    except Exception as e:
        print(f"ERROR loading position: {e}")
    return None

# ---------- UDP Command Receiver ----------
def udp_receiver_thread():
    global muted_all, muted_live, muted_parser, is_paused, live_hold_mode, speed_factor, wait_for_key_mode, loop_midi
    print(f"Listening for commands on UDP {UDP_IP_RECEIVE}:{UDP_PORT_RECEIVE}")
    while not stop_event.is_set():
        try:
            data, addr = receive_sock.recvfrom(1024)
            message = json.loads(data.decode('utf-8'))
            command = message.get("command")
            
            print(f"Received command: {command}")

            if command == "kalibracja_x_mniej":
                send_udp_message({"command": "adjust_x", "value": -1.0})
            elif command == "kalibracja_x_wiecej":
                send_udp_message({"command": "adjust_x", "value": 1.0})
            elif command == "kalibracja_y_mniej":
                send_udp_message({"command": "adjust_y", "value": -1.0})
            elif command == "kalibracja_y_wiecej":
                send_udp_message({"command": "adjust_y", "value": 1.0})
            elif command == "kalibracja_z_mniej":
                send_udp_message({"command": "adjust_z", "value": -1.0})
            elif command == "kalibracja_z_wiecej":
                send_udp_message({"command": "adjust_z", "value": 1.0})
            elif command == "reset_pozycji":
                send_udp_message({"command": "reset_position"})
            elif command == "wczytaj_pozycje":
                position = load_position()
                if position:
                    send_udp_message({"command": "set_position", "position": position})
            elif command == "zapisz_pozycje":
                # This command should be sent from Unreal with the current position
                position = message.get("position")
                if position:
                    save_position(position)
            elif command == "start_restart":
                restart_file_playback()
            elif command == "pauza":
                with state_lock: is_paused = not is_paused
                print(f"Pauza {'wlaczona' if is_paused else 'wylaczona'}.")
            elif command == "tryb_nauki":
                with state_lock:
                    wait_for_key_mode = not wait_for_key_mode
                    key_pressed_event.set()
                print(f"Tryb nauki {'WLACZONY' if wait_for_key_mode else 'WYLACZONY'}")
            elif command == "life_hold":
                with state_lock: live_hold_mode = not live_hold_mode
                print(f"LIVE hold {'wlaczone' if live_hold_mode else 'wylaczone'}.")
            elif command == "midi_wolniej":
                with state_lock: speed_factor = round(max(speed_factor - 0.05, 0.1), 2)
                print(f"Predkosc odtwarzania: {int(speed_factor * 100)}%")
            elif command == "midi_szybciej":
                with state_lock: speed_factor = round(min(speed_factor + 0.05, 4.0), 2)
                print(f"Predkosc odtwarzania: {int(speed_factor * 100)}%")
            elif command == "mute_file":
                with state_lock: muted_parser = not muted_parser
                print(f"Plik MIDI {'wyciszony' if muted_parser else 'odtwarzany'}.")
            elif command == "mute_live":
                with state_lock: muted_live = not muted_live
                print(f"Live MIDI {'wyciszone' if muted_live else 'odtwarzane'}.")
            elif command == "unmute_all":
                with state_lock: muted_all = False; muted_live = False; muted_parser = False
                print("Wszystkie tryby wyciszenia wylaczone.")
            elif command == "toggle_loop":
                with state_lock: loop_midi = not loop_midi
                send_udp_message({"command": "update_button_state", "button": "toggle_loop", "is_active": loop_midi})
                print(f"Looping MIDI {'wlaczone' if loop_midi else 'wylaczone'}.")

        except Exception as e:
            print(f"Error processing command: {e}")


# ---------- Control / Command Loop ----------
def main_loop():
    global muted_all, muted_live, muted_parser, is_paused, volume, stop_event, was_playing, live_hold_mode, speed_factor, wait_for_key_mode

    print("Komendy:\n" 
          " s - start/restart file MIDI playback\n" 
          " w - toggle practice mode (wait for key press)\n" 
          " p - toggle pause\n" 
          " f - toggle mute file playback\n" 
          " v - toggle mute live MIDI\n" 
          " m - toggle mute all\n" 
          " u - unmute all\n" 
          " h - toggle LIVE hold\n" 
          " . - przyspiesz o 5%\n" 
          " , - zwolnij o 5%\n" 
          " q - quit\n")
    start_live_midi()
    
    receiver = threading.Thread(target=udp_receiver_thread, daemon=True)
    receiver.start()

    while True:
        try:
            cmd = input("Podaj komende: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\nExiting..."); break

        if cmd == "q":
            print("Zamykanie programu..."); stop_event.set(); key_pressed_event.set(); break
        elif cmd == "s" or cmd == "r":
            restart_file_playback()
        elif cmd == "w":
            with state_lock:
                wait_for_key_mode = not wait_for_key_mode
                key_pressed_event.set()
            print(f"\033[96m[PRACTICE] Tryb nauki {'WLACZONY' if wait_for_key_mode else 'WYLACZONY'}\033[0m")
        elif cmd == "f":
            with state_lock: muted_parser = not muted_parser
            print(f"Plik MIDI {'wyciszony' if muted_parser else 'odtwarzany'}.")
        elif cmd == "v":
            with state_lock: muted_live = not muted_live
            print(f"Live MIDI {'wyciszone' if muted_live else 'odtwarzane'}.")
        elif cmd == "m":
            with state_lock: muted_all = not muted_all
            if muted_all: 
                for n in list(active_channels.keys()): stop_sound(n, force=True)
                print("Calkowite wyciszenie wlaczone.")
            else: print("Calkowite wyciszenie wylaczone.")
        elif cmd == "u":
            with state_lock: muted_all = False; muted_live = False; muted_parser = False
            print("Wszystkie tryby wyciszenia wylaczone.")
        elif cmd == "p":
            with state_lock: is_paused = not is_paused
            print(f"Pauza {'wlaczona' if is_paused else 'wylaczona'}.")
        elif cmd == "h":
            with state_lock: live_hold_mode = not live_hold_mode
            print(f"LIVE hold {'wlaczone' if live_hold_mode else 'wylaczone'}.")
        elif cmd == ".":
            with state_lock: speed_factor = round(min(speed_factor + 0.05, 4.0), 2)
            print(f"\033[91m[SPEED] Predkosc odtwarzania: {int(speed_factor * 100)}%\033[0m")
        elif cmd == ",":
            with state_lock: speed_factor = round(max(speed_factor - 0.05, 0.1), 2)
            print(f"\033[91m[SPEED] Predkosc odtwarzania: {int(speed_factor * 100)}%\033[0m")
        else:
            print(f"Nieznana komenda: {cmd}")

    # Cleanup on exit
    stop_event.set()
    key_pressed_event.set()
    if file_thread and file_thread.is_alive():
        file_thread.join(timeout=2)
    for n in list(active_channels.keys()):
        stop_sound(n, force=True)
    print("Program zakonczyl dzialanie.")

if __name__ == "__main__":
    main_loop()
