import serial
import numpy as np
import matplotlib.pyplot as plt


# ---------------- SETTINGS ---------------

PORT = "COM10"
BAUD = 460800

SAMPLE_RATE = 5000
BUFFER_SIZE = 1024

SPECTROGRAM_HISTORY = 80

ser = serial.Serial(
    PORT,
    BAUD,
    timeout=2
)

plt.ion()


# ---------------- DASHBOARD ----------------

fig = plt.figure(figsize=(13, 8))

ax_scope = fig.add_subplot(2, 2, 1)
ax_fft = fig.add_subplot(2, 2, 2)
ax_spec = fig.add_subplot(2, 2, 3)
ax_info = fig.add_subplot(2, 2, 4)


# ---------------- SPECTROGRAM BUFFER ----------------

# Frequency bins for rFFT
spec_freqs = np.fft.rfftfreq(
    BUFFER_SIZE,
    d=1 / SAMPLE_RATE
)

# Only show up to 1000 Hz
spec_mask = spec_freqs <= 1000
spec_freqs_display = spec_freqs[spec_mask]

spectrogram_data = np.zeros(
    (
        len(spec_freqs_display),
        SPECTROGRAM_HISTORY
    )
)


# ---------------- WAVEFORM DETECTION ----------------

def detect_waveform(voltage):

    vpp = np.max(voltage) - np.min(voltage)

    if vpp < 0.2:
        return "DC"

    near_max = (
        voltage >
        np.max(voltage) - 0.15 * vpp
    )

    near_min = (
        voltage <
        np.min(voltage) + 0.15 * vpp
    )

    extreme_ratio = np.mean(
        near_max | near_min
    )

    if extreme_ratio > 0.8:
        return "SQUARE"

    # Sine detection
    normalized = (
        voltage - np.mean(voltage)
    ) / (vpp / 2)

    std = np.std(normalized)

    if 0.65 < std < 0.8:
        return "SINE"

    # Triangle detection
    normalized = (
        voltage - np.min(voltage)
    ) / vpp

    hist, _ = np.histogram(
        normalized,
        bins=10
    )

    uniformity = (
        np.std(hist)
        /
        np.mean(hist)
    )

    if uniformity < 0.35:
        return "TRIANGLE"

    return "UNKNOWN"


# ---------------- HARMONICS / THD ----------------

def harmonic_analysis(
    freqs,
    magnitude,
    fundamental_frequency,
    harmonics=5
):

    results = []

    fundamental_index = np.argmin(
        np.abs(
            freqs -
            fundamental_frequency
        )
    )

    fundamental_amp = magnitude[
        fundamental_index
    ]

    if fundamental_amp == 0:
        return [], None

    harmonic_amps = []

    for harmonic in range(
        2,
        harmonics + 1
    ):

        target_frequency = (
            fundamental_frequency *
            harmonic
        )

        if target_frequency >= SAMPLE_RATE / 2:
            break

        target_index = np.argmin(
            np.abs(
                freqs -
                target_frequency
            )
        )

        low = max(
            0,
            target_index - 2
        )

        high = min(
            len(magnitude),
            target_index + 3
        )

        local_index = (
            low +
            np.argmax(
                magnitude[low:high]
            )
        )

        harmonic_frequency = (
            freqs[local_index]
        )

        harmonic_amp = (
            magnitude[local_index]
        )

        harmonic_amps.append(
            harmonic_amp
        )

        results.append(
            (
                harmonic,
                harmonic_frequency,
                harmonic_amp
            )
        )

    if len(harmonic_amps) == 0:
        return results, 0.0

    harmonic_amps = np.array(
        harmonic_amps
    )

    thd = (
        np.sqrt(
            np.sum(
                harmonic_amps ** 2
            )
        )
        /
        fundamental_amp
        *
        100
    )

    return results, thd


# ---------------- MAIN LOOP ----------------

while True:

    try:

        line = (
            ser.readline()
            .decode(
                "utf-8",
                errors="ignore"
            )
            .strip()
        )

        if not line:
            continue


        adc = np.array(
            line.split(","),
            dtype=float
        )


        # Reject incomplete / broken packets

        if len(adc) != BUFFER_SIZE:
            continue


        # ---------------- VOLTAGE ----------------

        voltage = (
            adc *
            3.3 /
            4095
        )

        time_ms = (
            np.arange(
                len(voltage)
            )
            /
            SAMPLE_RATE
            *
            1000
        )

        vmax = np.max(voltage)
        vmin = np.min(voltage)
        vpp = vmax - vmin
        vavg = np.mean(voltage)

        vrms = np.sqrt(
            np.mean(
                voltage ** 2
            )
        )

        current_voltage = voltage[-1]


        # ---------------- FREQUENCY ----------------

        frequency = None
        duty_cycle = None

        if vpp > 0.2:

            mid = (
                vmax +
                vmin
            ) / 2

            digital = voltage > mid

            edges = np.where(
                np.diff(
                    digital.astype(int)
                ) == 1
            )[0]

            if len(edges) >= 2:

                periods = np.diff(
                    edges
                )

                frequency = (
                    SAMPLE_RATE /
                    np.mean(periods)
                )

            duty_cycle = (
                np.mean(digital)
                * 100
            )


        # ---------------- FFT ----------------

        signal = (
            voltage -
            np.mean(voltage)
        )

        window = np.hanning(
            len(signal)
        )

        windowed_signal = (
            signal *
            window
        )

        fft = np.fft.rfft(
            windowed_signal
        )

        freqs = np.fft.rfftfreq(
            len(signal),
            d=1 / SAMPLE_RATE
        )

        magnitude = np.abs(fft)

        # Ignore DC
        magnitude[0] = 0

        peak_index = np.argmax(
            magnitude
        )

        fundamental_frequency = (
            freqs[peak_index]
        )


        # ---------------- HARMONICS / THD ----------------

        harmonic_data = []
        thd = None

        if vpp > 0.2:

            harmonic_data, thd = (
                harmonic_analysis(
                    freqs,
                    magnitude,
                    fundamental_frequency,
                    harmonics=5
                )
            )


        # ---------------- WAVEFORM ----------------

        wave = detect_waveform(
            voltage
        )


        # ---------------- SPECTROGRAM ----------------

        current_spectrum = (
            magnitude[spec_mask]
        )

        # Log scale so weak harmonics are visible
        current_spectrum_db = (
            20 *
            np.log10(
                current_spectrum + 1e-6
            )
        )

        # Shift old columns left
        spectrogram_data = np.roll(
            spectrogram_data,
            -1,
            axis=1
        )

        # Add newest spectrum on right
        spectrogram_data[:, -1] = (
            current_spectrum_db
        )


        # ---------------- TEXT ----------------

        freq_text = (
            f"{frequency:.2f} Hz"
            if frequency is not None
            else "N/A"
        )

        fft_text = (
            f"{fundamental_frequency:.2f} Hz"
            if vpp > 0.2
            else "N/A"
        )

        duty_text = (
            f"{duty_cycle:.2f}%"
            if (
                wave == "SQUARE"
                and
                duty_cycle is not None
            )
            else "N/A"
        )

        thd_text = (
            f"{thd:.2f}%"
            if thd is not None
            else "N/A"
        )


        # ---------------- DASHBOARD ----------------

        ax_scope.clear()
        ax_fft.clear()
        ax_spec.clear()
        ax_info.clear()


        # -------- OSCILLOSCOPE --------

        ax_scope.plot(
            time_ms,
            voltage
        )

        ax_scope.set_ylim(
            0,
            3.3
        )

        ax_scope.set_xlabel(
            "Time (ms)"
        )

        ax_scope.set_ylabel(
            "Voltage (V)"
        )

        ax_scope.set_title(
            "Oscilloscope"
        )

        ax_scope.grid(True)


        # -------- FFT --------

        ax_fft.plot(
            freqs,
            magnitude
        )

        ax_fft.set_xlim(
            0,
            1000
        )

        ax_fft.set_xlabel(
            "Frequency (Hz)"
        )

        ax_fft.set_ylabel(
            "Magnitude"
        )

        ax_fft.set_title(
            "FFT / Harmonic Spectrum"
        )

        ax_fft.grid(True)


        # Harmonic markers

        for (
            number,
            harmonic_freq,
            harmonic_amp
        ) in harmonic_data:

            ax_fft.plot(
                harmonic_freq,
                harmonic_amp,
                "o"
            )


        # -------- SPECTROGRAM --------

        ax_spec.imshow(
            spectrogram_data,
            aspect="auto",
            origin="lower",
            extent=[
                0,
                SPECTROGRAM_HISTORY,
                spec_freqs_display[0],
                spec_freqs_display[-1]
            ]
        )

        ax_spec.set_xlabel(
            "Time History"
        )

        ax_spec.set_ylabel(
            "Frequency (Hz)"
        )

        ax_spec.set_title(
            "Live Spectrogram"
        )


        # -------- INFO PANEL --------

        info = (
            f"Waveform: {wave}\n\n"

            f"Frequency: {freq_text}\n"
            f"FFT Fundamental: {fft_text}\n"

            f"Vmax: {vmax:.2f} V\n"
            f"Vmin: {vmin:.2f} V\n"
            f"Vpp: {vpp:.2f} V\n"

            f"Vrms: {vrms:.2f} V\n"
            f"Vavg: {vavg:.2f} V\n"
            f"Current: {current_voltage:.2f} V\n"

            f"Duty Cycle: {duty_text}\n"
            f"THD: {thd_text}"
        )

        ax_info.axis("off")

        ax_info.text(
            0.05,
            0.95,
            info,
            fontsize=11,
            verticalalignment="top"
        )


        fig.suptitle(
            "ESP32 Smart Signal Analyzer"
        )

        plt.tight_layout()

        plt.pause(0.001)


    except ValueError:
        continue
