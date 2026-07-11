import csv
import os
import re
import sys
import time
from typing import List, Optional, Tuple

import matplotlib.pyplot as plt
import serial

# Configuration
SERIAL_PORT = "COM10"          # Change this to your serial port
SERIAL_BAUD = 115200           # Change this if your UART uses another baud rate
COMPARE_CSV_PATH = "DISCRETEPI_NOEROSION_VP_74P43_VI_6I87_converted.csv"
SAVE_FILENAME = "sweep_data.csv"  # Default file name for UART sweep save


NUMBER_TRIPLE_RE = re.compile(
    r"([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?),"
    r"([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?),"
    r"([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)"
)


def parse_triples(text: str) -> List[Tuple[float, float, float]]:
    matches = NUMBER_TRIPLE_RE.findall(text)
    return [(float(f), float(m), float(p)) for f, m, p in matches]


def load_csv_data(filename: str) -> Tuple[List[float], List[float], List[float]]:
    freqs: List[float] = []
    mags: List[float] = []
    phases: List[float] = []

    with open(filename, newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            if row[0].strip().lower() == "frequency":
                continue
            if len(row) < 3:
                continue
            try:
                freqs.append(float(row[0]))
                mags.append(float(row[1]))
                phases.append(float(row[2]))
            except ValueError:
                continue

    return freqs, mags, phases


def build_csv_name_from_header(line: str) -> str:
    header = line.split("ILOOP Parameters:", 1)[-1].strip()
    parts = {}
    for segment in header.split(","):
        item = segment.strip()
        if not item:
            continue
        match = re.match(r"(?:ILOOP\s+)?(.+?)\s*:\s*(.+)", item, flags=re.IGNORECASE)
        if not match:
            continue
        key = match.group(1).strip()
        value = match.group(2).strip()
        parts[key] = value

    # Build filename parts in consistent order
    result = []
    if "FX" in parts:
        val = re.sub(r"[^A-Za-z0-9]+", "", parts["FX"])
        result.append(f"FX_{val}")
    
    # Detect and fix swapped GM/PM (GM should be DB, PM should be DEG)
    gm_val = parts.get("GM", "")
    pm_val = parts.get("PM", "")
    
    if gm_val and pm_val:
        gm_clean = re.sub(r"[^A-Za-z0-9]+", "", gm_val)
        pm_clean = re.sub(r"[^A-Za-z0-9]+", "", pm_val)
        gm_unit = re.sub(r"[0-9]+", "", gm_val).strip().upper()
        pm_unit = re.sub(r"[0-9]+", "", pm_val).strip().upper()
        
        # If GM has DEG and PM has DB, they're swapped—fix it
        if "DEG" in gm_unit and "DB" in pm_unit:
            result.append(f"PM_{gm_clean}")
            result.append(f"GM_{pm_clean}")
        else:
            result.append(f"GM_{gm_clean}")
            result.append(f"PM_{pm_clean}")
    elif gm_val:
        val = re.sub(r"[^A-Za-z0-9]+", "", gm_val)
        result.append(f"GM_{val}")
    elif pm_val:
        val = re.sub(r"[^A-Za-z0-9]+", "", pm_val)
        result.append(f"PM_{val}")
    
    if not result:
        return "sweep_data.csv"
    return f"ILOOP_{'_'.join(result)}.csv"


def plot_bode(
    freqs: List[float],
    mags: List[float],
    phases: List[float],
    compare_freqs: Optional[List[float]] = None,
    compare_mags: Optional[List[float]] = None,
    compare_phases: Optional[List[float]] = None,
    compare_label: Optional[str] = None,
) -> None:
    if not freqs:
        print("No sweep data to plot.")
        return

    fig, (ax_mag, ax_phase) = plt.subplots(2, 1, sharex=True, figsize=(10, 7))

    ax_mag.semilogx(freqs, mags, marker="o", linestyle="-", color="tab:blue", label="Actual Response")
    ax_mag.set_ylabel("Magnitude (dB)")
    ax_mag.grid(True, which="both", ls="--", lw=0.5)

    ax_phase.semilogx(freqs, phases, marker="o", linestyle="-", color="tab:orange", label="Actual Response")
    ax_phase.set_ylabel("Phase (deg)")
    ax_phase.set_xlabel("Frequency (Hz)")
    ax_phase.grid(True, which="both", ls="--", lw=0.5)

    if compare_freqs and compare_mags and compare_phases:
        label = compare_label or "Compare"
        ax_mag.semilogx(compare_freqs, compare_mags, marker="x", linestyle="--", color="tab:green", label=label)
        ax_phase.semilogx(compare_freqs, compare_phases, marker="x", linestyle="--", color="tab:green", label=label)

    ax_mag.legend()
    ax_phase.legend()

    plt.tight_layout()
    plt.show()


def save_csv(filename: str, freqs: List[float], mags: List[float], phases: List[float]) -> None:
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["frequency", "magnitude_db", "phase_deg"])
        for fr, mg, ph in zip(freqs, mags, phases):
            writer.writerow([fr, mg, ph])


def main() -> int:
    compare_freqs: Optional[List[float]] = None
    compare_mags: Optional[List[float]] = None
    compare_phases: Optional[List[float]] = None
    compare_label: Optional[str] = None
    csv_filename = SAVE_FILENAME

    if COMPARE_CSV_PATH:
        if not os.path.isfile(COMPARE_CSV_PATH):
            print(f"Compare file not found: {COMPARE_CSV_PATH}. Continuing with UART sweep only.")
        else:
            compare_freqs, compare_mags, compare_phases = load_csv_data(COMPARE_CSV_PATH)
            compare_label = os.path.basename(COMPARE_CSV_PATH)
            print(f"Loaded comparison CSV: {COMPARE_CSV_PATH} ({len(compare_freqs)} points)")

    print("SFRA Plotter Started")
    input("Please Connect STM32 and hold Reset Button, then press Enter to continue...")
    print("Hold Reset Button and wait for 3 seconds...\n")
    time.sleep(1)
    print("Hold Reset Button and wait for 2 seconds...\n")
    time.sleep(1)
    print("Hold Reset Button and wait for 1 second...\n")
    time.sleep(1)
    print("Release Reset Button and wait for the data to be printed...\n")

    ser = serial.Serial(port=SERIAL_PORT, baudrate=SERIAL_BAUD, timeout=1)

    in_sweep = False
    freqs: List[float] = []
    mags: List[float] = []
    phases: List[float] = []

    def start_new_sweep() -> None:
        nonlocal in_sweep, freqs, mags, phases
        in_sweep = True
        freqs.clear()
        mags.clear()
        phases.clear()
        print("Detected start of frequency sweep; buffering data...")

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            print(line)

            if "ILOOP Parameters:" in line:
                csv_filename = build_csv_name_from_header(line)
                print(f"Using CSV filename from header: {csv_filename}")

            if "Starting Frequency Sweep" in line:
                if in_sweep:
                    print("New sweep header detected before previous sweep ended; discarding partial data and restarting buffer.")
                start_new_sweep()
                continue

            if in_sweep and "frequency,magnitude_db,phase_deg" in line:
                # The header line inside an active sweep is not data; ignore it.
                continue

            if not in_sweep and "frequency,magnitude_db,phase_deg" in line:
                start_new_sweep()
                continue

            if in_sweep:
                triples = parse_triples(line)
                if triples:
                    for f, m, p in triples:
                        freqs.append(f)
                        mags.append(m)
                        phases.append(p)

                if "End of Frequency Sweep" in line or "End of frequency sweep" in line:
                    print("Detected end of sweep; plotting data...")
                    save_csv(csv_filename, freqs, mags, phases)
                    print(f"Saved {len(freqs)} points to {csv_filename}")
                    plot_bode(
                        freqs,
                        mags,
                        phases,
                        compare_freqs=compare_freqs,
                        compare_mags=compare_mags,
                        compare_phases=compare_phases,
                        compare_label=compare_label,
                    )
                    break

    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        if ser.is_open:
            ser.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
