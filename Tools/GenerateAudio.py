"""Create Clanka's original game sounds with the Python standard library.

Run from any directory: python Tools/GenerateAudio.py
The editor bootstrap imports Content/Audio/Source/*.wav into /Game/Audio.
"""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 44100
DESTINATION = Path(__file__).resolve().parents[1] / "Content" / "Audio" / "Source"
TAU = 2.0 * math.pi


def bell(t, frequency, decay=5.0):
    if t < 0:
        return 0.0
    attack = min(1.0, t / 0.006)
    return attack * math.exp(-decay * t) * (
        math.sin(TAU * frequency * t)
        + 0.22 * math.sin(TAU * frequency * 2.003 * t) * math.exp(-5 * t)
        + 0.07 * math.sin(TAU * frequency * 3.997 * t) * math.exp(-10 * t)
    )


def synthesize(name, duration, sample, loop=False, peak_level=0.56):
    count = int(RATE * duration)
    values = [sample(i / RATE) for i in range(count)]
    peak = max(abs(value) for value in values) or 1.0
    gain = peak_level / peak
    pcm = bytearray()
    for i, value in enumerate(values):
        fade = 1.0 if loop else min(1.0, i / (RATE * 0.003), (count - 1 - i) / (RATE * 0.055))
        pcm.extend(struct.pack("<h", int(max(-1.0, min(1.0, value * gain * fade)) * 32767)))
    output = DESTINATION / (name + ".wav")
    with wave.open(str(output), "wb") as sound:
        sound.setnchannels(1)
        sound.setsampwidth(2)
        sound.setframerate(RATE)
        sound.writeframes(pcm)
    with wave.open(str(output), "rb") as check:
        assert check.getnframes() == count and check.getnchannels() == 1
        assert check.getsampwidth() == 2 and check.getframerate() == RATE
    print(f"{name}: {duration:.2f}s, {output.stat().st_size} bytes")


def main():
    DESTINATION.mkdir(parents=True, exist_ok=True)
    randomizer = random.Random(73021)

    def pop(t):
        # An elastic downward chirp with a brief rubbery air puff.
        phase = TAU * (110 * t + 43 * (1 - math.exp(-35 * t)))
        puff = randomizer.uniform(-1, 1) * math.exp(-50 * t)
        return math.sin(phase) * math.exp(-18 * t) + 0.43 * puff

    filtered = 0.0

    def splash(t):
        nonlocal filtered
        filtered = 0.86 * filtered + 0.14 * randomizer.uniform(-1, 1)
        wash = filtered * (1 - math.exp(-90 * t)) * math.exp(-7 * t) * 2.4
        bubbles = 0.0
        for start, frequency in ((0.045, 570), (0.11, 730), (0.18, 480), (0.26, 850)):
            delta = t - start
            if delta >= 0:
                bubbles += 0.16 * math.sin(TAU * (frequency * delta + 160 * delta * delta)) * math.exp(-30 * delta)
        return wash + bubbles

    synthesize("S_Pop", 0.40, pop)
    synthesize("S_Splash", 0.72, splash)
    synthesize("S_Confirm", 0.66, lambda t: bell(t, 659.25, 8) * 0.7 + bell(t - 0.10, 987.77, 8))
    synthesize("S_Wave", 1.70, lambda t: (
        bell(t, 523.25, 4.8) * 0.65
        + bell(t - 0.13, 659.25, 4.8) * 0.72
        + bell(t - 0.26, 783.99, 4.3) * 0.82
        + bell(t - 0.41, 1046.50, 3.8)
        + bell(t - 0.41, 523.25, 4.2) * 0.26
    ))

    # Integer-period oscillators ensure sample-continuous boundaries without fades.
    stream = [(randomizer.randrange(190, 2900), randomizer.random() * TAU) for _ in range(42)]
    synthesize("S_WaterLoop", 1.0, lambda t: sum(
        math.sin(TAU * frequency * t + phase) / math.sqrt(frequency)
        for frequency, phase in stream
    ) * (0.82 + 0.10 * math.sin(TAU * 3 * t) + 0.08 * math.sin(TAU * 7 * t)),
        loop=True, peak_level=0.26)

    breeze = [(randomizer.randrange(180, 4200) / 8.0, randomizer.random() * TAU) for _ in range(22)]

    def ambience(t):
        wind = sum(math.sin(TAU * f * t + phase) / math.sqrt(f) for f, phase in breeze)
        wind *= 0.10 * (0.72 + 0.28 * math.sin(TAU * t / 8))
        pad = sum(math.sin(TAU * f * t) for f in (196, 262, 330)) * 0.012
        chimes = 0.0
        for start, frequency in ((0.7, 784), (3.1, 1046), (5.5, 659)):
            delta = (t - start) % 8.0
            chimes += bell(delta, frequency, 3.2) * min(1.0, (8.0 - delta) / 0.5) * 0.055
        return wind + pad + chimes

    synthesize("S_Ambience", 8.0, ambience, loop=True, peak_level=0.18)


if __name__ == "__main__":
    main()
